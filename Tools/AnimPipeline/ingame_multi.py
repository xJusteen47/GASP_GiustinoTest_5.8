"""In-game read of a built move from several angles, with and without hitstop, in ONE mp4 for the owner.
    python ingame_multi.py <card.json> <hitstop_frames> <out.mp4> [--cached]
Same capture as ingame_judge.py (lit mesh, L_PilotCapture, 60 fps, 1x), but every frame is shot from four cameras:
side (the PvP Duel camera), 3/4 front, behind, from above. Each output frame is a grid: one row per camera, hitstop on
the left (the first active frame held <hitstop> frames), whiff on the right (the clip straight through; it holds its last
frame while the hitstop column finishes). --cached reuses sheets/ingame/_<move>/multi/<cam>/<f>.png.
Needs the editor in L_PilotCapture (capture_ue.setup refuses to switch level while a map is dirty)."""
import json, os, sys, time
from PIL import Image, ImageDraw
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue
from animpipe.card import load, windows

# name, az, el, dist, fov: az 0 = camera in front of the character, 90 = its left side (capture_ue._place)
CAMS = [("lato", 90, 12, 450, 55), ("3/4 fronte", 315, 12, 450, 55), ("dietro", 180, 12, 450, 55), ("alto", 150, 70, 450, 55)]
S = 384

args = [a for a in sys.argv[1:] if not a.startswith("--")]
cached = "--cached" in sys.argv
card, hit, mp4 = load(args[0]), int(args[1]), args[2]
anim, move = card["anim"], card["move"]
w = windows(card)["anim"]
base = os.path.join(HERE, "sheets", "ingame", f"_{move}", "multi")
dirs = {c[0]: os.path.join(base, c[0].replace("/", "_").replace(" ", "_")) for c in CAMS}
for d in dirs.values():
    os.makedirs(d, exist_ok=True)

if cached:
    n = len(os.listdir(dirs["lato"])) - 1
else:
    r = ue.call("run_python_file", {"filePath": os.path.join(HERE, "ingame_ue.py").replace("\\", "/"), "entryPoint": "montage",
                                    "kwargs": {"anim_path": anim}}, timeout=300)
    n = int(round(json.loads(r["result"])["length"] * 60))
    st = ue.py("setup")
    if isinstance(st, dict) and st.get("error"):
        sys.exit(f"setup refused: {st}")
    for f in range(n + 1):
        want = ue.call("sample_pose", {"assetPath": anim, "boneNames": ["hand_r"], "frames": [f], "space": "component"})
        want = [want["samples"][0]["bones"][0]["location"][k] for k in "xyz"]
        for _ in range(30):                   # the scrub lands a tick later: wait until the mesh shows frame f
            got = ue.py("set_frame", anim_path=anim, frame=f)["hand_r"]
            if max(abs(a - b) for a, b in zip(got, want)) < 0.5:
                break
            time.sleep(0.1)
        for name, az, el, dist, fov in CAMS:
            tag = f"igm_{move}_{dirs[name].rsplit(os.sep, 1)[-1]}_{f:03d}"
            ue.py("capture", kind="lit", az=az, el=el, tag=tag, size=S, dist=dist, fov=fov)
            raw = open(os.path.join(HERE, "sheets", "raw", f"{tag}.rgb"), "rb").read()
            Image.frombytes("RGB", (S, S), raw).save(os.path.join(dirs[name], f"{f:03d}.png"))

a0 = w["active"][0]
seq = list(range(n + 1))
hs = seq[:a0 + 1] + [a0] * hit + seq[a0 + 1:]
tmp = os.path.join(base, "_frames"); os.makedirs(tmp, exist_ok=True)
for old in os.listdir(tmp):
    os.remove(os.path.join(tmp, old))
HEAD, GAP = 22, 4
W, H = 2 * S + GAP, HEAD + len(CAMS) * (S + GAP)
for i, fh in enumerate(hs):
    fw = seq[min(i, len(seq) - 1)]
    im = Image.new("RGB", (W, H), (20, 20, 20)); d = ImageDraw.Draw(im)
    held = a0 < i <= a0 + hit
    d.text((6, 5), f"CON HITSTOP ({hit}f)" + ("  [FERMO]" if held else ""), fill=(255, 220, 0))
    d.text((S + GAP + 6, 5), "SENZA HITSTOP (whiff)", fill=(255, 255, 255))
    for r, (name, *_rest) in enumerate(CAMS):
        y = HEAD + r * (S + GAP)
        for col, f in ((0, fh), (1, fw)):
            tile = Image.open(os.path.join(dirs[name], f"{f:03d}.png"))
            im.paste(tile, (col * (S + GAP), y))
            act = w["active"][0] <= f <= w["active"][1]
            d.text((col * (S + GAP) + 6, y + 6), f"{name}  f{f}" + ("  HITBOX" if act else ""), fill=(255, 80, 80) if act else (255, 255, 0))
    im.save(os.path.join(tmp, f"{i:04d}.png"))
os.system(f'ffmpeg -y -loglevel error -framerate 60 -i "{tmp}/%04d.png" -pix_fmt yuv420p -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" "{mp4}"')
print(f"{move}: {n + 1} frames x {len(CAMS)} cameras, hitstop {hit} at f{a0} -> {mp4}")
