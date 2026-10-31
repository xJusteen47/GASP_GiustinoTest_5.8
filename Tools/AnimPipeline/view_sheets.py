"""Standing review rule (owner, 2026-10-31): every key pose from front, right, back, left, top and bottom, one row per pose, plus
close-ups of the hands. UE lit captures (the real mesh), 512 px per cell.
Usage: python view_sheets.py <take> "<frame:label,...>" [a-b] [elbows=f,f,...]
Output: sheets/legib_<take>/<take>_views_f<NN>_<label>.png (3x2: front, right, back / left, top, bottom)
        sheets/legib_<take>/<take>_hands_<view>.png (frames a..b, close-up on both hands)
        sheets/legib_<take>/<take>_elbows_f<NN>.png (owner, v9: elbow close-ups from the six views, lead arm top row, rear
        arm bottom row; for the frames the arm check flags or the key frames)"""
import sys, os, json, time
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility_shots as S
L = S.L
CELL = 512
VIEWS6 = [("front", 0, 10.0), ("right side", 270, 10.0), ("back", 180, 10.0), ("left side", 90, 10.0), ("top", 0, 88.0), ("bottom", 0, -88.0)]

def lit(take, tag, az, el, dist=320.0, fov=40.0, center=None):
    import ue
    ue.py("capture", kind="lit", az=az, el=el, tag=tag, size=768, dist=dist, fov=fov, center=list(center) if center is not None else list(L.CENTER))
    rgb = np.fromfile(os.path.join(L.RAW, tag + ".rgb"), dtype=np.uint8).reshape(768, 768, 3)
    return Image.fromarray(rgb)

def label(im, text):
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, im.width, 30], fill=(20, 20, 20))
    d.text((8, 4), text, fill=(255, 255, 255), font=S.font(20))
    return im

if __name__ == "__main__":
    take = sys.argv[1]
    poses = [(int(a), b) for a, b in (x.split(":") for x in sys.argv[2].split(","))]
    extra = sys.argv[3:]
    hands = next((x for x in extra if "=" not in x), None)
    elbows = next(([int(f) for f in x.split("=", 1)[1].split(",")] for x in extra if x.startswith("elbows=")), [])
    d = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))
    import ue
    ue.py("setup")
    out_dir = os.path.join(L.SHEETS, f"legib_{take}"); os.makedirs(out_dir, exist_ok=True)
    for frame, lab in poses:
        S.settle(d["anim"], frame, d["frames"][str(frame)])
        sheet = Image.new("RGB", (CELL * 3, CELL * 2), (12, 12, 12))
        for i, (name, az, el) in enumerate(VIEWS6):
            im = lit(take, f"vs_{take}_{frame}_{i}", az, el).crop((96, 64, 672, 640)).resize((CELL, CELL))
            sheet.paste(label(im, f"{take} f{frame} {lab} | {name}"), ((i % 3) * CELL, (i // 3) * CELL))
        p = os.path.join(out_dir, f"{take}_views_f{frame:02d}_{lab}.png"); sheet.save(p); print(p)
    if hands:
        a, b = (int(x) for x in hands.split("-"))
        for vname, az, el in (("front", 0, 10.0), ("3-4front", 315, 10.0)):
            row = Image.new("RGB", (CELL * (b - a + 1) // 2 * 1, CELL // 2 * 2), (12, 12, 12))
            cells = []
            for frame in range(a, b + 1):
                F = d["frames"][str(frame)]
                c = 0.5 * (L.bone_tf(F, "hand_l")[0] + L.bone_tf(F, "hand_r")[0])
                S.settle(d["anim"], frame, F)
                im = lit(take, f"hz_{take}_{frame}_{vname}", az, el, dist=150.0, fov=30.0, center=c).resize((CELL // 2 * 2 // 2, CELL // 2 * 2 // 2))
                cells.append(label(im, f"f{frame} {vname}"))
            row = Image.new("RGB", (cells[0].width * len(cells), cells[0].height), (12, 12, 12))
            for i, im in enumerate(cells):
                row.paste(im, (i * im.width, 0))
            p = os.path.join(out_dir, f"{take}_hands_{vname}.png"); row.save(p); print(p)
    EC = 384
    for frame in elbows:
        F = d["frames"][str(frame)]
        S.settle(d["anim"], frame, F)
        sheet = Image.new("RGB", (EC * 6, EC * 2), (12, 12, 12))
        for row, side in enumerate("lr"):
            c = L.bone_tf(F, f"lowerarm_{side}")[0]
            for i, (name, az, el) in enumerate(VIEWS6):
                im = lit(take, f"ez_{take}_{frame}_{side}_{i}", az, el, dist=120.0, fov=30.0, center=c).resize((EC, EC))
                sheet.paste(label(im, f"f{frame} elbow_{side} | {name}"), (i * EC, row * EC))
        p = os.path.join(out_dir, f"{take}_elbows_f{frame:02d}.png"); sheet.save(p); print(p)
