"""One contact sheet (grid) from a folder of numbered PNG frames, each cell labelled with its frame number.
    python vfx_contact_sheet.py <out.jpg> <folder> <glob prefix> <from> <to> <step> <cols> <cell width>
[crop x0,y0,x1,y1]: crop every frame first (e.g. the fighter's area of a 1920x1080 frame)
e.g. vfx_contact_sheet.py sheet.jpg Saved/VfxV3/dkref/f30 d 1 572 15 8 240  (frames d0001.png ... every 15th)
Prints one line."""
import glob
import os
import re
import sys

from PIL import Image, ImageDraw

out, folder, prefix = sys.argv[1], sys.argv[2], sys.argv[3]
a, b, step, cols, cw = (int(v) for v in sys.argv[4:9])
crop = tuple(int(v) for v in sys.argv[9].split(',')) if len(sys.argv) > 9 else None
files = {}
for p in glob.glob(os.path.join(folder, prefix + '*.png')):
    m = re.search(r'(\d+)\.png$', p)
    if m:
        files[int(m.group(1))] = p
picks = [n for n in range(a, b + 1, step) if n in files]
def _open(n):
    img = Image.open(files[n]).convert('RGB')
    return img.crop(crop) if crop else img


first = _open(picks[0])
ch = round(first.height * cw / first.width)
rows = (len(picks) + cols - 1) // cols
sheet = Image.new('RGB', (cols * cw, rows * ch), (0, 0, 0))
d = ImageDraw.Draw(sheet)
for i, n in enumerate(picks):
    x, y = (i % cols) * cw, (i // cols) * ch
    sheet.paste(_open(n).resize((cw, ch)), (x, y))
    d.rectangle([x, y, x + 44, y + 12], fill=(0, 0, 0))
    d.text((x + 2, y), str(n), fill=(255, 255, 0))
sheet.save(out, quality=88)
print(f'{out}: {len(picks)} frames, {sheet.width}x{sheet.height}')
