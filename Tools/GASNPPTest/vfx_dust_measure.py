"""Landing dust numbers from a VFX-on capture and its VFX-off pass (same sequence and view, frames aligned by index).
Dust pixels = pixels whose luma differs from the VFX-off frame by more than a threshold inside a box around the feet. Per frame: dust
pixel count, bbox width/height (px), mean dust luma and B/R, mean luma of the floor next to the feet in the OFF frame, ratio dust/floor,
and the 10th percentile dust luma (shaded underside) vs floor.
    python vfx_dust_measure.py <on_dir> <off_dir> <seq> <view> <from> <to> <x0,y0,x1,y1> <out.tsv> [thr=6]
The box is in pixels of the capture. Writes a TSV, prints one line (frames measured, peak width/height, median ratio)."""
import os
import sys

from PIL import Image

on, off, seq, view, a, b, box, out = sys.argv[1:9]
thr = float(sys.argv[9]) if len(sys.argv) > 9 else 6.0
x0, y0, x1, y1 = (int(v) for v in box.split(','))


def luma(p):
    return 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]


rows, ratios, peak_w, peak_h = [], [], 0, 0
for f in range(int(a), int(b) + 1):
    pa = os.path.join(on, seq, view, f"f{f:03d}.png")
    pb = os.path.join(off, seq, view, f"f{f:03d}.png")
    if not (os.path.exists(pa) and os.path.exists(pb)):
        continue
    ia = Image.open(pa).convert('RGB').crop((x0, y0, x1, y1))
    ib = Image.open(pb).convert('RGB').crop((x0, y0, x1, y1))
    da, db = ia.load(), ib.load()
    w, h = ia.size
    dust, floor_l, xs, ys = [], [], [], []
    for y in range(h):
        for x in range(w):
            la, lb = luma(da[x, y]), luma(db[x, y])
            if abs(la - lb) > thr:
                dust.append(da[x, y]); xs.append(x); ys.append(y)
            elif y > h * 0.6:
                floor_l.append(lb)
    if not dust or not floor_l:
        rows.append(f"{f}\t0"); continue
    ls = sorted(luma(p) for p in dust)
    mean_l = sum(ls) / len(ls)
    p10 = ls[len(ls) // 10]
    br = sum(p[2] for p in dust) / max(1, sum(p[0] for p in dust))
    fl = sum(floor_l) / len(floor_l)
    bw, bh = max(xs) - min(xs) + 1, max(ys) - min(ys) + 1
    peak_w, peak_h = max(peak_w, bw), max(peak_h, bh)
    ratios.append(mean_l / max(fl, 1))
    rows.append(f"{f}\t{len(dust)}\t{bw}\t{bh}\t{mean_l:.0f}\t{p10:.0f}\t{fl:.0f}\t{mean_l / max(fl, 1):.2f}\t{p10 / max(fl, 1):.2f}\t{br:.2f}")
with open(out, 'w') as fh:
    fh.write("frame\tpixels\tw\th\tdustL\tdustL_p10\tfloorL\tratio\tratio_p10\tB/R\n" + "\n".join(rows) + "\n")
med = sorted(ratios)[len(ratios) // 2] if ratios else 0
print(f"{len(ratios)} frames with dust, peak bbox {peak_w}x{peak_h} px, median dust/floor {med:.2f} -> {out}")
