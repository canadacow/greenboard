# Moment-transport web assets: per-lobe weight / mean / covariance maps.
import numpy as np
from PIL import Image
import os

D = os.path.dirname(os.path.abspath(__file__))
SIG_MAX = 0.6   # sigma encode: byte = sqrt(sigma/SIG_MAX)*255

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
