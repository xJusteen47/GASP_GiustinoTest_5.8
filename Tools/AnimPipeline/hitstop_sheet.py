"""Before/after sheet of the frames around a hitstop freeze (check hitstop_snap), one image per move.
    python hitstop_sheet.py <move> <hit_frame> <before_raw_dir> [after_raw_dir]
Rows: before, after. Columns: hit-1, hit (the frozen frame), hit+1 (first frame after the freeze), hit+2, hit+3, and an
overlay of hit (red) on hit+1: the wider the red/cyan split, the bigger the pop the player sees after the freeze.
Frames come from ingame_judge.py captures (sheets/ingame/_<move>/raw). Writes sheets/ingame/<move>_hitstop_snap.png."""
import os, sys
from PIL import Image, ImageChops, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
move, hit, before = sys.argv[1], int(sys.argv[2]), sys.argv[3]
after = sys.argv[4] if len(sys.argv) > 4 else os.path.join(HERE, "sheets", "ingame", f"_{move}", "raw")
cols = [hit - 1, hit, hit + 1, hit + 2, hit + 3]
S, T = 384, 18


def img(d, f):
    return Image.open(os.path.join(d, f"{f:03d}.png")).convert("RGB")


def overlay(d):
    a, b = img(d, hit).convert("L"), img(d, hit + 1).convert("L")
    return Image.merge("RGB", (a, b, b))      # hit in red, hit+1 in cyan: grey where they agree


sheet = Image.new("RGB", (S * (len(cols) + 1), 2 * (S + T)), (20, 20, 20)); dr = ImageDraw.Draw(sheet)
for r, (lab, d) in enumerate((("before", before), ("after", after))):
    y = r * (S + T)
    for i, f in enumerate(cols):
        sheet.paste(img(d, f), (i * S, y + T))
        dr.text((i * S + 6, y + 3), f"{lab} f{f}" + (" FROZEN (hitstop)" if f == hit else (" first after freeze" if f == hit + 1 else "")), fill=(255, 255, 0))
    sheet.paste(overlay(d), (len(cols) * S, y + T))
    dr.text((len(cols) * S + 6, y + 3), f"{lab}: f{hit} red over f{hit + 1} cyan", fill=(255, 255, 0))
out = os.path.join(HERE, "sheets", "ingame", f"{move}_hitstop_snap.png")
sheet.save(out); print(out)
