"""Side-by-side: Mario SSBU jab-1 GIF frames (G0..G18) vs our anim at the same game time (anim frame = round(G * 0.96)),
our character seen from its left side (az 90: faces right like Mario, punching arm toward the camera).
Usage: python mario_compare.py <AnimPath> <tag> <gif path>"""
import sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image, ImageDraw
import ue

ANIM, TAG, GIF = sys.argv[1], sys.argv[2], sys.argv[3]
AZ, EL = 90, 5
HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, "sheets", "raw")

def pose(frame):
    r = ue.call("sample_pose", {"assetPath": ANIM, "boneNames": ["hand_r"], "frames": [frame], "space": "component"})
    want = tuple(r["samples"][0]["bones"][0]["location"][k] for k in ("x", "y", "z"))
    ue.py("set_frame", anim_path=ANIM, frame=(frame + 5) % 38, hide_arm=False, bone="clavicle_l")
    for _ in range(30):
        got = ue.py("set_frame", anim_path=ANIM, frame=frame, hide_arm=False, bone="clavicle_l")["hand_r"]
        if max(abs(a - b) for a, b in zip(got, want)) < 0.5:
            ue.py("set_frame", anim_path=ANIM, frame=frame, hide_arm=False, bone="clavicle_l")
            return
        time.sleep(0.2)
    raise RuntimeError("pose did not settle")

g = Image.open(GIF)
mario = []
for i in range(g.n_frames):
    g.seek(i); mario.append(g.convert("RGB").copy())
ue.py("setup")
ours = {}
for G in range(len(mario)):
    k = int(round(G * 0.96))
    if k not in ours:
        pose(k)
        t = f"side_{TAG}_f{k}"
        ue.py("capture", kind="lit", az=AZ, el=EL, tag=t)
        rgb = np.fromfile(os.path.join(RAW, t + ".rgb"), dtype=np.uint8).reshape(512, 512, 3)
        ours[k] = Image.fromarray(rgb).crop((40, 30, 470, 460))
cw, ch = 200, 200
cols = len(mario)
sheet = Image.new("RGB", (cw * 10, ch * 4), (30, 30, 30))
d = ImageDraw.Draw(sheet)
for G, m in enumerate(mario):
    k = int(round(G * 0.96))
    r0 = (G // 10) * 2; c = G % 10
    sheet.paste(m.crop((20, 30, 500, 489)).resize((cw, ch)), (c * cw, r0 * ch))
    sheet.paste(ours[k].resize((cw, ch)), (c * cw, (r0 + 1) * ch))
    d.text((c * cw + 4, r0 * ch + 3), f"Mario G{G}", fill=(255, 255, 0))
    d.text((c * cw + 4, (r0 + 1) * ch + 3), f"{TAG} k{k}", fill=(255, 255, 0))
out = os.path.join(HERE, "sheets", f"mario_vs_{TAG}.png")
sheet.save(out)
print(out)
