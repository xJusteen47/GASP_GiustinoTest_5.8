"""In-game read of a built move: montage + hitstop + side (Duel) camera, without PIE.
    python ingame_judge.py <card.json> <hitstop_frames> [az=90] [--cached] [--at=<active anim frame>]
Creates AM_<name> next to the built anim, captures every frame lit from the side camera (el 12, 450 cm, FOV 55 =
FGSTN_CameraDuelTuning), repeats the first active frame <hitstop> times (the attacker's hitstop freeze), and writes
sheets/ingame/<move>_ingame.mp4 (60 fps, 1x) and <move>_ingame_strip.png (startup end -> active -> recovery start, one image).
Standard second output: the same move whiffing (no hitstop, the sequence plays straight through) as <move>_ingame_whiff.mp4, and
<move>_ingame_compare.mp4 with hitstop (left) and whiff (right) side by side. --cached reuses the frames of the last capture
(sheets/ingame/_<move>/raw) instead of driving the editor, e.g. to re-time the hitstop. --at freezes on another active frame
(a hit can land on any of them; the frame after the freeze is what check hitstop_snap limits).
Judged by eye on the strip and the video: weight/force, smoothness, readability. Needs the editor in L_PilotCapture."""
import json, os, sys, time
import numpy as np
from PIL import Image, ImageDraw
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue
from animpipe.card import load, windows

args = [a for a in sys.argv[1:] if not a.startswith("--")]
cached = "--cached" in sys.argv
card, hit = load(args[0]), int(args[1])
az = float(args[2]) if len(args) > 2 else 90.0
anim, move = card["anim"], card["move"]
out = os.path.join(HERE, "sheets", "ingame"); os.makedirs(out, exist_ok=True)
rawdir = os.path.join(out, f"_{card['move']}", "raw"); os.makedirs(rawdir, exist_ok=True)
r = None if cached else ue.call("run_python_file", {"filePath": os.path.join(HERE, "ingame_ue.py").replace("\\", "/"), "entryPoint": "montage",
                                "kwargs": {"anim_path": anim}}, timeout=300)
w = windows(card)["anim"]
if cached:
    n = len(os.listdir(rawdir)) - 1
    m = {"montage": "(cached frames)"}
else:
    m = json.loads(r["result"])
    n = int(round(m["length"] * 60))
    ue.py("setup")
S = 384


def grab(f):
    cache = os.path.join(rawdir, f"{f:03d}.png")
    if cached:
        return Image.open(cache).convert("RGB")
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
    im = Image.frombytes("RGB", (S, S), raw); im.save(cache)
    return im


imgs = {f: grab(f) for f in range(n + 1)}
a0 = w["active"][0]
for a in sys.argv[1:]:
    if a.startswith("--at="):
        a0 = int(a[5:])
seq = list(range(n + 1))
hs_seq = seq[:a0 + 1] + [a0] * hit + seq[a0 + 1:]


def render(frames, name, with_hit):
    tmp = os.path.join(out, f"_{move}", name); os.makedirs(tmp, exist_ok=True)
    for old in os.listdir(tmp):
        os.remove(os.path.join(tmp, old))
    for i, f in enumerate(frames):
        im = imgs[f].copy(); d = ImageDraw.Draw(im)
        tag = "HITSTOP" if with_hit and a0 < i <= a0 + hit else ("WHIFF" if not with_hit else "")
        d.text((6, 6), f"{move} f{f} {tag}", fill=(255, 255, 0))
        im.save(os.path.join(tmp, f"{i:04d}.png"))
    return tmp


def encode(tmp, mp4):
    os.system(f'ffmpeg -y -loglevel error -framerate 60 -i "{tmp}/%04d.png" -pix_fmt yuv420p "{mp4}"')


mp4 = os.path.join(out, f"{move}_ingame.mp4")
encode(render(hs_seq, "hitstop", True), mp4)
mp4_whiff = os.path.join(out, f"{move}_ingame_whiff.mp4")
encode(render(seq, "whiff", False), mp4_whiff)
# Side by side: the whiff plays out its length then holds its last frame while the hitstop version finishes.
cmp_tmp = os.path.join(out, f"_{move}", "compare"); os.makedirs(cmp_tmp, exist_ok=True)
for old in os.listdir(cmp_tmp):
    os.remove(os.path.join(cmp_tmp, old))
for i in range(len(hs_seq)):
    L = Image.open(os.path.join(out, f"_{move}", "hitstop", f"{i:04d}.png"))
    R = Image.open(os.path.join(out, f"_{move}", "whiff", f"{min(i, len(seq) - 1):04d}.png"))
    c = Image.new("RGB", (S * 2 + 4, S), (255, 255, 255)); c.paste(L, (0, 0)); c.paste(R, (S + 4, 0))
    c.save(os.path.join(cmp_tmp, f"{i:04d}.png"))
mp4_cmp = os.path.join(out, f"{move}_ingame_compare.mp4")
encode(cmp_tmp, mp4_cmp)
strip_f = sorted(set([max(0, a0 - 3), a0 - 1, a0, w["active"][1], w["active"][1] + 2, w["startup"][1] + (w["active"][1] - a0) + 6,
                      (w["active"][1] + w["faf"]) // 2, w["faf"]]))
strip_f = [f for f in strip_f if 0 <= f <= n]
st = Image.new("RGB", (S * len(strip_f), S + 18), (20, 20, 20)); d = ImageDraw.Draw(st)
for i, f in enumerate(strip_f):
    st.paste(imgs[f], (i * S, 18)); d.text((i * S + 6, 3), f"f{f}" + (" contact" if w["active"][0] <= f <= w["active"][1] else ""), fill=(255, 255, 255))
png = os.path.join(out, f"{move}_ingame_strip.png"); st.save(png)
print(f"{move}: montage {m['montage']} {n + 1} frames + hitstop {hit} at f{a0}")
print(mp4); print(mp4_whiff); print(mp4_cmp); print(png)
