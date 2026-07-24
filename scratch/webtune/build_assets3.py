# Moment-transport web assets: per-lobe weight / mean / covariance maps,
# plus the power-on / power-off plates.
import numpy as np
from PIL import Image
import os

D = os.path.dirname(os.path.abspath(__file__))
SIG_MAX = 0.6   # sigma encode: byte = sqrt(sigma/SIG_MAX)*255

# plate (LED dark) and LED emission layer -- additive decomposition:
# final = plate*bench_light + led*power + screen terms
p = Image.open(r"c:\dev\bench\assets\crt_composite\plate_leddark.png").convert("RGB")
p.resize((1600, 1200), Image.LANCZOS).save(os.path.join(D, "plate_on.jpg"), quality=88)
p = Image.open(r"c:\dev\bench\assets\crt_composite\led_layer.png").convert("RGB")
p.resize((1600, 1200), Image.LANCZOS).save(os.path.join(D, "led_layer.jpg"), quality=90)

# warp: STRICT coverage -- rim pixels with partial/antialiased coverage decode
# to blended UVs (wrong texels); require full coverage and erode one pixel so
# the boundary classifies as case (glow path), never as screen.
w = np.load(os.path.join(D, "warp_uvc.npy"))  # (2400, 3200, 3) u,v,cov
u, v, c = w[:, :, 0], w[:, :, 1], w[:, :, 2]
cov = (c > 0.99).astype(np.float32)

def pool2(a):
    return a.reshape(a.shape[0] // 2, 2, a.shape[1] // 2, 2).mean(axis=(1, 3))

cs = pool2(cov)
us = np.divide(pool2(u * cov), cs, out=np.zeros_like(cs), where=cs > 0)
vs = np.divide(pool2(v * cov), cs, out=np.zeros_like(cs), where=cs > 0)
full = cs > 0.999
er = full.copy()
for dy in (-1, 0, 1):
    for dx in (-1, 0, 1):
        er &= np.roll(np.roll(full, dy, axis=0), dx, axis=1)
u16 = np.clip(us, 0, 1) * 65535.0
v16 = np.clip(vs, 0, 1) * 65535.0
wa = np.zeros(cs.shape + (3,), np.uint8)
wb = np.zeros_like(wa)
wa[:, :, 0] = (u16 // 256).astype(np.uint8)
wa[:, :, 1] = (u16 % 256).astype(np.uint8)
wa[:, :, 2] = er.astype(np.uint8) * 255
wb[:, :, 0] = (v16 // 256).astype(np.uint8)
wb[:, :, 1] = (v16 % 256).astype(np.uint8)
Image.fromarray(wa).save(os.path.join(D, "warp_a.png"), optimize=True)
Image.fromarray(wb).save(os.path.join(D, "warp_b.png"), optimize=True)

for l in ("diff", "gloss"):
    mean = np.load(os.path.join(D, f"{l}_mean.npy"))   # mu, mv, valid
    cov = np.load(os.path.join(D, f"{l}_cov.npy"))     # cuu, cvv, cuv
    w = np.load(os.path.join(D, f"{l}_w.npy")).astype(np.float32)
    vmask = (mean[:, :, 2] > 0.5).astype(np.float32)[:, :, None]
    mean = mean * vmask                                # invalid pixels are 0/0 noise:
    cov = cov * vmask                                  # zero them -- undefined data,
    w = w * vmask                                      # not signal

    u16 = np.clip(mean[:, :, 0], 0, 1) * 65535.0
    v16 = np.clip(mean[:, :, 1], 0, 1) * 65535.0
    a = np.zeros(mean.shape[:2] + (3,), np.uint8)
    b = np.zeros_like(a)
    a[:, :, 0] = (u16 // 256).astype(np.uint8)
    a[:, :, 1] = (u16 % 256).astype(np.uint8)
    a[:, :, 2] = (mean[:, :, 2] > 0.5).astype(np.uint8) * 255
    b[:, :, 0] = (v16 // 256).astype(np.uint8)
    b[:, :, 1] = (v16 % 256).astype(np.uint8)
    Image.fromarray(a).save(os.path.join(D, f"{l}_mean_a.png"), optimize=True)
    Image.fromarray(b).save(os.path.join(D, f"{l}_mean_b.png"), optimize=True)

    su = np.sqrt(np.clip(cov[:, :, 0], 0, None))
    sv = np.sqrt(np.clip(cov[:, :, 1], 0, None))
    rho = cov[:, :, 2] / np.maximum(su * sv, 1e-9)
    cv = np.zeros_like(a)
    cv[:, :, 0] = (np.sqrt(np.clip(su / SIG_MAX, 0, 1)) * 255).astype(np.uint8)
    cv[:, :, 1] = (np.sqrt(np.clip(sv / SIG_MAX, 0, 1)) * 255).astype(np.uint8)
    cv[:, :, 2] = (np.clip((rho + 1) / 2, 0, 1) * 255).astype(np.uint8)
    Image.fromarray(cv).save(os.path.join(D, f"{l}_cov.png"), optimize=True)

    srgb = np.where(w <= 0.0031308, w * 12.92,
                    1.055 * np.clip(w, 0, None) ** (1 / 2.4) - 0.055)
    Image.fromarray((np.clip(srgb, 0, 1) * 255).astype(np.uint8)).save(
        os.path.join(D, f"{l}_w.png"), optimize=True)

for l in ("diff", "gloss"):
    for s in ("mean_a", "mean_b", "cov", "w"):
        f = f"{l}_{s}.png"
        print(f, os.path.getsize(os.path.join(D, f)) // 1024, "KB")
