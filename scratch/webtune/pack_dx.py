# Pack the bezel composite maps for renderer.cpp -- FULL FIDELITY.
# Sources the raw bakes in assets/crt_composite (EXR float data, 16-bit
# plates), NOT the web tuner's compressed embeds. Runs inside Blender
# (needs its bundled OpenImageIO):
#   blender -b -P scratch/webtune/pack_dx.py     (or via the MCP bridge)
#
# Layout: 'CRTB', u32 version=2, u32 ntex; per texture:
#   char name[16], u32 w, u32 h, u32 dxgi_format, u32 byte_size, data[]
# Formats:
#    2 = DXGI_FORMAT_R32G32B32A32_FLOAT  (warp, means, covs, weights)
#   11 = DXGI_FORMAT_R16G16B16A16_UNORM  (plate, led -- native 16-bit PNG data,
#                                         sRGB-encoded as rendered)
import OpenImageIO as oiio
import numpy as np
import struct
import os

SRC = r"c:\dev\bench\assets\crt_composite"
OUT = os.path.join(SRC, "bezel_pack.bin")


def read_exr(path):
    inp = oiio.ImageInput.open(path)
    spec = inp.spec()
    arr = np.array(inp.read_image(format="float")).reshape(
        spec.height, spec.width, spec.nchannels)
    inp.close()
    return arr


def read_parts(path):
    inp = oiio.ImageInput.open(path)
    parts = {}
    idx = 0
    while inp.seek_subimage(idx, 0):
        spec = inp.spec()
        name = str(spec.getattribute("name") or spec.channelnames[0].split(".")[0])
        arr = np.array(inp.read_image(format="float")).reshape(
            spec.height, spec.width, spec.nchannels)
        parts[name.split(".")[0].lower()] = arr[:, :, :3]
        idx += 1
    inp.close()
    return parts


def f32x4(*chans):
    return np.stack(list(chans) + [np.zeros_like(chans[0])] * (4 - len(chans)),
                    axis=-1).astype(np.float32)


texs = []

# ---- plates: native 16-bit PNGs, sRGB-encoded as rendered ----
for name, fname in (("plate", "plate_leddark.png"), ("led", "led_layer.png")):
    a = read_exr(os.path.join(SRC, fname))     # oiio reads PNG fine
    a16 = np.clip(a, 0, 1) * 65535.0
    rgba = np.zeros(a.shape[:2] + (4,), np.uint16)
    rgba[:, :, :3] = a16[:, :, :3].astype(np.uint16)
    rgba[:, :, 3] = 65535
    texs.append((name, rgba, 11))

# ---- warp: full 3200x2400 float32, strict coverage + 1px erosion ----
w = read_exr(os.path.join(SRC, "warp.exr"))
u, v, c = w[:, :, 0], w[:, :, 1], w[:, :, 2]
full = c > 0.99
er = full.copy()
for dy in (-1, 0, 1):
    for dx in (-1, 0, 1):
        er &= np.roll(np.roll(full, dy, axis=0), dx, axis=1)
texs.append(("warp", f32x4(u, v, er.astype(np.float32)), 2))

# ---- per-lobe moments straight from the raw renders, float32 ----
def lobes(stem):
    p = read_parts(os.path.join(SRC, stem + ".exr"))
    return {l: p[cc] * (p[dd] + p[ii])
            for l, (cc, dd, ii) in {"diff": ("dc", "dd", "di"),
                                     "gloss": ("gc", "gd", "gi")}.items()}

M0 = lobes("mom_M0"); M1 = lobes("mom_M1"); M2 = lobes("mom_M2")
EPS = 1e-5
TEX = 1.0 / 1600.0
for l in ("diff", "gloss"):
    wgt = M0[l]
    n = np.maximum(wgt, EPS)
    mu = M1[l][:, :, 0] / n[:, :, 0]
    mv = M1[l][:, :, 1] / n[:, :, 1]
    euu = M2[l][:, :, 0] / n[:, :, 0]
    evv = M2[l][:, :, 1] / n[:, :, 1]
    euv = M2[l][:, :, 2] / n[:, :, 2]
    cuu = np.clip(euu - mu * mu, TEX * TEX, None)
    cvv = np.clip(evv - mv * mv, TEX * TEX, None)
    cuv = euv - mu * mv
    lim = np.sqrt(cuu * cvv) * 0.99
    cuv = np.clip(cuv, -lim, lim)
    valid = (wgt.max(axis=2) > 1e-4).astype(np.float32)
    mu = np.clip(mu, 0, 1) * valid
    mv = np.clip(mv, 0, 1) * valid
    texs.append((f"{l}_mean", f32x4(mu, mv, valid), 2))
    texs.append((f"{l}_cov", f32x4(cuu * valid[..., None][:, :, 0],
                                    cvv * valid, cuv * valid), 2))
    wz = wgt * valid[:, :, None]
    texs.append((f"{l}_w", f32x4(wz[:, :, 0], wz[:, :, 1], wz[:, :, 2],
                                  np.ones_like(valid)), 2))

with open(OUT, "wb") as f:
    f.write(b"CRTB")
    f.write(struct.pack("<II", 2, len(texs)))
    for name, arr, fmt in texs:
        data = np.ascontiguousarray(arr).tobytes()
        f.write(struct.pack("<16sIIII", name.encode().ljust(16, b"\0"),
                            arr.shape[1], arr.shape[0], fmt, len(data)))
        f.write(data)
print(OUT, os.path.getsize(OUT) // (1024 * 1024), "MB,", len(texs), "textures")
