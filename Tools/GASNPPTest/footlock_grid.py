# NIGHT-FOOTLOCK: off/on comparison grid of one view around a blend-out (same frames), legs cropped.
# Usage: python footlock_grid.py <off seq dir> <on seq dir> <view> <first> <last> <step> <out.png>
import sys
from PIL import Image, ImageDraw

off, on, view, f0, f1, step, out = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]), sys.argv[7]
frames = list(range(f0, f1 + 1, step))
cols, cell = 10, 200
rows_per = (len(frames) + cols - 1) // cols
img = Image.new('RGB', (cols * cell, 2 * rows_per * cell + 20), 'black')
d = ImageDraw.Draw(img)
for band, (label, base) in enumerate((('OFF', off), ('ON', on))):
    for i, f in enumerate(frames):
        try:
            src = Image.open(f'{base}/{view}/f{f:03d}.png')
        except OSError:
            continue
        w, h = src.size
        src = src.crop((w * 0.2, h * 0.35, w * 0.8, h * 0.95)).resize((cell, cell))
        x, y = (i % cols) * cell, (band * rows_per + i // cols) * cell + band * 20
        img.paste(src, (x, y))
        d.text((x + 4, y + 4), f'{label} f{f}', fill='yellow')
img.save(out)
print(out)
