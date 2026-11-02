"""In-game read of a built move: montage + hitstop + side (Duel) camera, without PIE.
    python ingame_judge.py <card.json> <hitstop_frames> [az=90]
Creates AM_<name> next to the built anim, captures every frame lit from the side camera (el 12, 450 cm, FOV 55 =
FGSTN_CameraDuelTuning), repeats the first active frame <hitstop> times (the attacker's hitstop freeze), and writes
sheets/ingame/<move>_ingame.mp4 (60 fps, 1x) and <move>_ingame_strip.png (startup end -> active -> recovery start, one image).
Judged by eye on the strip and the video: weight/force, smoothness, readability. Needs the editor in L_PilotCapture."""
import json, os, sys, time
import numpy as np
from PIL import Image, ImageDraw
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue
from animpipe.card import load, windows

card, hit = load(sys.argv[1]), int(sys.argv[2])
az = float(sys.argv[3]) if len(sys.argv) > 3 else 90.0
anim, move = card["anim"], card["move"]
out = os.path.join(HERE, "sheets", "ingame"); os.makedirs(out, exist_ok=True)
r = ue.call("run_python_file", {"filePath": os.path.join(HERE, "ingame_ue.py").replace("\\", "/"), "entryPoint": "montage",
                                "kwargs": {"anim_path": anim}}, timeout=300)
m = json.loads(r["result"])
w = windows(card)["anim"]
n = int(round(m["length"] * 60))
ue.py("setup")
S = 384


def grab(f):
    for _ in range(20):                       # a source-control dialog after the montage save blocks the bridge a few seconds
        want = ue.call("sample_pose", {"assetPath": anim, "boneNames": ["hand_r"], "frames": [f], "space": "component"})
        if "samples" in want:
            break
        time.sleep(3)
    want = [want["samples"][0]["bones"][0]["location"][k] for k in "xyz"]
    for _ in range(30):                       # the scrub lands a tick later: wait until the mesh shows frame f
        got = ue.py("set_frame", anim_path=anim, frame=f)["hand_r"]
        if max(abs(a - b) for a, b in zip(got, want)) < 0.5:
            break
        time.sleep(0.1)
    ue.py("capture", kind="lit", az=az, el=12, tag=f"ig_{move}_{f:03d}", size=S, dist=450, fov=55)
    raw = open(os.path.join(HERE, "sheets", "raw", f"ig_{move}_{f:03d}.rgb"), "rb").read()
    return Image.frombytes("RGB", (S, S), raw)


imgs = {f: grab(f) for f in range(n + 1)}
seq = [f for f in range(n + 1)]
a0 = w["active"][0]
seq = seq[:a0 + 1] + [a0] * hit + seq[a0 + 1:]
tmp = os.path.join(out, f"_{move}"); os.makedirs(tmp, exist_ok=True)
for i, f in enumerate(seq):
    im = imgs[f].copy(); d = ImageDraw.Draw(im)
    tag = "HITSTOP" if i > a0 and i <= a0 + hit else ""
    d.text((6, 6), f"{move} f{f} {tag}", fill=(255, 255, 0))
    im.save(os.path.join(tmp, f"{i:04d}.png"))
mp4 = os.path.join(out, f"{move}_ingame.mp4")
os.system(f'ffmpeg -y -loglevel error -framerate 60 -i "{tmp}/%04d.png" -pix_fmt yuv420p "{mp4}"')
strip_f = sorted(set([max(0, a0 - 3), a0 - 1, a0, w["active"][1], w["active"][1] + 2, w["startup"][1] + (w["active"][1] - a0) + 6,
                      (w["active"][1] + w["faf"]) // 2, w["faf"]]))
strip_f = [f for f in strip_f if 0 <= f <= n]
st = Image.new("RGB", (S * len(strip_f), S + 18), (20, 20, 20)); d = ImageDraw.Draw(st)
for i, f in enumerate(strip_f):
    st.paste(imgs[f], (i * S, 18)); d.text((i * S + 6, 3), f"f{f}" + (" contact" if w["active"][0] <= f <= w["active"][1] else ""), fill=(255, 255, 255))
png = os.path.join(out, f"{move}_ingame_strip.png"); st.save(png)
print(f"{move}: montage {m['montage']} {n + 1} frames + hitstop {hit} at f{a0}")
print(mp4); print(png)
