"""Gameplay-camera rear check: lit perspective captures from behind (elevation EL) for two takes, side by side.
Usage: python rear_check.py <animA> <tagA> <animB> <tagB> <frames csv> <azimuths csv> [el=20]"""
import sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image, ImageDraw
import ue

A, TA, B, TB = sys.argv[1:5]
FRAMES = [int(x) for x in sys.argv[5].split(",")]
AZS = [int(x) for x in sys.argv[6].split(",")]
EL = float(sys.argv[7]) if len(sys.argv) > 7 else 20.0
HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, "sheets", "raw")

def pose(anim, frame):
    r = ue.call("sample_pose", {"assetPath": anim, "boneNames": ["hand_r"], "frames": [frame], "space": "component"})
    want = tuple(r["samples"][0]["bones"][0]["location"][k] for k in ("x", "y", "z"))
    ue.py("set_frame", anim_path=anim, frame=(frame + 5) % 38, hide_arm=False, bone="clavicle_l")
    for _ in range(30):
        got = ue.py("set_frame", anim_path=anim, frame=frame, hide_arm=False, bone="clavicle_l")["hand_r"]
        if max(abs(a - b) for a, b in zip(got, want)) < 0.5:
            ue.py("set_frame", anim_path=anim, frame=frame, hide_arm=False, bone="clavicle_l")
            return
        time.sleep(0.2)
    raise RuntimeError("pose did not settle")

ue.py("setup")
cells = []
for anim, tag in ((A, TA), (B, TB)):
    row = []
    for f in FRAMES:
        pose(anim, f)
        for az in AZS:
            t = f"rear_{tag}_f{f}_az{az}"
            ue.py("capture", kind="lit", az=az, el=EL, tag=t)
            rgb = np.fromfile(os.path.join(RAW, t + ".rgb"), dtype=np.uint8).reshape(512, 512, 3)
            im = Image.fromarray(rgb).crop((96, 16, 416, 400)).resize((240, 288))
            ImageDraw.Draw(im).text((6, 6), f"{tag} f{f} az{az}", fill=(255, 255, 0))
            row.append(im)
    cells.append(row)
W = len(cells[0])
sheet = Image.new("RGB", (240 * W, 288 * 2), (0, 0, 0))
for r, row in enumerate(cells):
    for c, im in enumerate(row):
        sheet.paste(im, (c * 240, r * 288))
out = os.path.join(HERE, "sheets", f"rear_{TA}_vs_{TB}.png")
sheet.save(out)
print(out)
