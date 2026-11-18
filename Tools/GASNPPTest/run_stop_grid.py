"""Side-by-side grid of the run stop captures (rows = variants, columns = the same frames). Rule 4: writes files, prints one line.
usage: python run_stop_grid.py <root> <sequence> <view> <first> <last> <step> <out.jpg> [variants=base,blend,clip]"""
import sys, os
from PIL import Image, ImageDraw
root, seq, view, a, z, step, out = sys.argv[1:8]
variants = (sys.argv[8] if len(sys.argv) > 8 else "base,blend,clip").split(",")
# optional crop as fractions x0,y0,x1,y1 (the review cameras frame the stage, the body is small)
crop = [float(x) for x in sys.argv[9].split(",")] if len(sys.argv) > 9 else None
frames = list(range(int(a), int(z) + 1, int(step)))
W = 300
rows = []
for v in variants:
    cells = []
    for f in frames:
        path = os.path.join(root, v, seq, view, "f%03d.png" % f)
        im = Image.open(path).convert("RGB") if os.path.exists(path) else Image.new("RGB", (640, 360), (40, 0, 0))
        if crop:
            im = im.crop((int(crop[0] * im.width), int(crop[1] * im.height), int(crop[2] * im.width), int(crop[3] * im.height)))
        im = im.resize((W, int(im.height * W / im.width)))
        d = ImageDraw.Draw(im)
        d.rectangle((0, 0, 120, 18), fill=(0, 0, 0))
        d.text((3, 3), "%s f%d" % (v, f), fill=(255, 255, 0))
        cells.append(im)
    rows.append(cells)
H = rows[0][0].height
grid = Image.new("RGB", (W * len(frames), H * len(rows)))
for r, cells in enumerate(rows):
    for c, im in enumerate(cells):
        grid.paste(im, (c * W, r * H))
grid.save(out, quality=85)
print(out, grid.size)
