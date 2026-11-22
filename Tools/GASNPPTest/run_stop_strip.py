"""Side-by-side frames (variants in a row, cropped on the body) for an mp4 at real speed (rule 10). Writes JPGs, prints one line.
usage: python run_stop_strip.py <root> <sequence> <view> <first> <last> <outdir> [variants=base,clip,blend]"""
import os, sys
from PIL import Image, ImageDraw
root, seq, view, a, z, out = sys.argv[1:7]
vs = (sys.argv[7] if len(sys.argv) > 7 else "base,clip,blend").split(",")
os.makedirs(out, exist_ok=True)
n = 0
for i, f in enumerate(range(int(a), int(z) + 1)):
    cells = []
    for v in vs:
        im = Image.open(os.path.join(root, v, seq, view, "f%03d.png" % f)).convert("RGB")
        im = im.crop((int(0.05 * im.width), int(0.2 * im.height), int(0.7 * im.width), int(0.85 * im.height)))
        im = im.resize((640, int(im.height * 640 / im.width) // 2 * 2))
        d = ImageDraw.Draw(im)
        d.rectangle((0, 0, 200, 18), fill=(0, 0, 0))
        d.text((4, 4), "%s  f%d" % (v, f), fill=(255, 255, 0))
        cells.append(im)
    g = Image.new("RGB", (640 * len(cells), cells[0].height))
    for k, c in enumerate(cells):
        g.paste(c, (640 * k, 0))
    g.save(os.path.join(out, "m%03d.jpg" % i), quality=90)
    n += 1
print(out, n, "frames")
