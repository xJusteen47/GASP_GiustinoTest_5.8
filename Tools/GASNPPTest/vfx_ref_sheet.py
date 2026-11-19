"""Frame sheet of a reference video's frames (extracted PNGs f001.png...): a crop box, consecutive frames, labelled.
Used for VFX breakdowns of owner references (docs/notes/vfx-studies/). Reusable for any study:
    python vfx_ref_sheet.py <frames_dir> <out.jpg> <from> <to> <x0,y0,x1,y1> [cols=6] [cell_w=360] [step=1]
Prints one line."""
import os
import sys

from PIL import Image, ImageDraw

src, out, a, b, box = sys.argv[1:6]
cols = int(sys.argv[6]) if len(sys.argv) > 6 else 6
cw = int(sys.argv[7]) if len(sys.argv) > 7 else 360
step = int(sys.argv[8]) if len(sys.argv) > 8 else 1
x0, y0, x1, y1 = (int(v) for v in box.split(","))
ch = int(cw * (y1 - y0) / (x1 - x0))
frames = [f for f in range(int(a), int(b) + 1, step) if os.path.exists(os.path.join(src, f"f{f:03d}.png"))]
rows = (len(frames) + cols - 1) // cols
sheet = Image.new("RGB", (cols * cw, rows * ch))
for i, f in enumerate(frames):
    im = Image.open(os.path.join(src, f"f{f:03d}.png")).convert("RGB").crop((x0, y0, x1, y1)).resize((cw, ch), Image.LANCZOS)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 44, 14), fill=(0, 0, 0))
    d.text((3, 2), f"f{f}", fill=(255, 255, 0))
    sheet.paste(im, ((i % cols) * cw, (i // cols) * ch))
os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
sheet.save(out, quality=88)
print(f"ok {len(frames)} frames -> {out}")
