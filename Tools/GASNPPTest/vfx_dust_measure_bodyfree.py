"""Body-free landing dust numbers (A2 r3): like vfx_dust_measure.py, but the character and its shadow are masked out, so an OFF pass
whose pose drifts a few px from the ON pass can be used. Mask = pixels of the OFF frame that differ from the floor (median of the box
border) by more than `body` luma, dilated by `grow` px. Dust = remaining pixels whose ON luma differs from OFF by more than `thr`.
Per frame: dust px, bbox w/h, top (px above the box bottom), median and 10th/90th percentile dust luma, dust B/R, floor luma, floor B/R.
    python vfx_dust_measure_bodyfree.py <on_dir> <off_dir> <seq> <view> <from> <to> <x0,y0,x1,y1> <out.tsv> [thr=8] [body=14] [grow=6]
Writes a TSV, prints one line."""
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

on, off, seq, view, a, b, box, out = sys.argv[1:9]
thr = float(sys.argv[9]) if len(sys.argv) > 9 else 8.0
body = float(sys.argv[10]) if len(sys.argv) > 10 else 14.0
grow = int(sys.argv[11]) if len(sys.argv) > 11 else 6
x0, y0, x1, y1 = (int(v) for v in box.split(','))


def load(p):
    return np.asarray(Image.open(p).convert('RGB').crop((x0, y0, x1, y1)), dtype=np.float32)


def luma(c):
    return 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]


rows, meds, brs = [], [], []
for f in range(int(a), int(b) + 1):
    pa = os.path.join(on, seq, view, f"f{f:03d}.png")
    pb = os.path.join(off, seq, view, f"f{f:03d}.png")
    if not (os.path.exists(pa) and os.path.exists(pb)):
        continue
    A, B = load(pa), load(pb)
    la, lb = luma(A), luma(B)
    border = np.concatenate([lb[0], lb[-1], lb[:, 0], lb[:, -1]])
    floor_l = float(np.median(border))
    bodym = np.abs(lb - floor_l) > body
    m = Image.fromarray((bodym * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(2 * grow + 1))
    bodym = np.asarray(m) > 0
    dust = (np.abs(la - lb) > thr) & ~bodym
    floor_px = B[~bodym]
    fbr = float(np.median(floor_px[:, 2] / np.maximum(floor_px[:, 0], 1)))
    n = int(dust.sum())
    if n < 20:
        rows.append((f, n, 0, 0, 0, 0, 0, 0, 0, round(floor_l, 1), round(fbr, 2)))
        continue
    ys, xs = np.nonzero(dust)
    d = A[dust]
    dl = la[dust]
    br = float(np.median(d[:, 2] / np.maximum(d[:, 0], 1)))
    med = float(np.median(dl))
    rows.append((f, n, int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1), int((y1 - y0) - ys.min()), round(med, 1),
                 round(float(np.percentile(dl, 10)), 1), round(float(np.percentile(dl, 90)), 1), round(br, 2), round(floor_l, 1), round(fbr, 2)))
    meds.append(med / max(floor_l, 1))
    brs.append(br)
with open(out, 'w') as fh:
    fh.write('frame\tpx\tw\th\ttop\tmedL\tp10L\tp90L\tBR\tfloorL\tfloorBR\n')
    for r in rows:
        fh.write('\t'.join(str(v) for v in r) + '\n')
print(f"{len(rows)} frames, dust frames {len(meds)}, median ratio {np.median(meds) if meds else 0:.2f}, median B/R {np.median(brs) if brs else 0:.2f}, "
      f"max top {max((r[4] for r in rows), default=0)}, max w {max((r[2] for r in rows), default=0)} -> {out}")
