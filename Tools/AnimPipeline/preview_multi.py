"""Capsule preview of a built move from four cameras, with and without hitstop, in one mp4. No editor needed.
    python preview_multi.py <card.json> <hitstop_frames> <out.mp4>
The body is the physics capsules of legibility.py posed by the bone cache (sheets/raw/<take>_legbones.json, written by
check), shaded by depth, one colour per part (striking limb red). Same layout as ingame_multi.py: one row per camera,
hitstop left, whiff right. A stand-in for the lit capture when the editor cannot switch to L_PilotCapture."""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import legibility as L
from animpipe.card import load, windows

CAMS = [("lato", 90, 12), ("3/4 fronte", 315, 12), ("dietro", 180, 12), ("alto", 150, 70)]
card, hit, mp4 = load(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
take = card["anim"].rsplit("/", 1)[1][3:]
L.configure(take, card["anim"])
L.RES = 320
frames = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))["frames"]
n = len(frames) - 1
zs = [v[b][2] for v in frames.values() for b in ("head", "ball_l", "ball_r")]
center = (0.0, 10.0, (min(zs) + max(zs)) / 2)
views = [{"name": nm, "kind": "persp", "az": az, "el": el, "dist": 450.0, "fov": 32.0, "center": center} for nm, az, el in CAMS]
strike = set(L.ATT_CHAIN)
pal = {}
for k, p in enumerate(L.PNAMES):
    pal[k] = (np.array([220, 60, 50]) if p in strike else np.array([90, 140, 210]) if p.endswith("_l") else
              np.array([120, 190, 140]) if p.endswith("_r") else np.array([200, 200, 200]))


def tile(f, view):
    depth = L.render(L.part_prims(frames[str(f)]), view)
    zmin = depth.min(axis=0); hitm = np.isfinite(zmin); idx = depth.argmin(axis=0)
    img = np.full((L.RES, L.RES, 3), 30, np.float32)
    if hitm.any():
        z = zmin[hitm]; shade = 1.0 - 0.55 * (z - z.min()) / max(1e-6, z.max() - z.min())
        col = np.array([pal[k] for k in idx[hitm]], np.float32)
        img[hitm] = col * shade[:, None]
    # ground line at z 0
    return Image.fromarray(img.astype(np.uint8))


cache = {(f, v["name"]): tile(f, v) for f in range(n + 1) for v in views}
w = windows(card)["anim"]; a0 = w["active"][0]
seq = list(range(n + 1)); hs = seq[:a0 + 1] + [a0] * hit + seq[a0 + 1:]
S, HEAD, GAP = L.RES, 22, 4
tmp = os.path.join(os.path.dirname(os.path.abspath(mp4)), f"_{card['move']}_preview"); os.makedirs(tmp, exist_ok=True)
for old in os.listdir(tmp):
    os.remove(os.path.join(tmp, old))
for i, fh in enumerate(hs):
    fw = seq[min(i, n)]
    im = Image.new("RGB", (2 * S + GAP, HEAD + len(views) * (S + GAP)), (10, 10, 10)); d = ImageDraw.Draw(im)
    d.text((6, 5), f"CON HITSTOP ({hit}f)" + ("  [FERMO]" if a0 < i <= a0 + hit else ""), fill=(255, 220, 0))
    d.text((S + GAP + 6, 5), "SENZA HITSTOP (whiff)", fill=(255, 255, 255))
    for r, v in enumerate(views):
        y = HEAD + r * (S + GAP)
        for c, f in ((0, fh), (1, fw)):
            im.paste(cache[(f, v["name"])], (c * (S + GAP), y))
            act = w["active"][0] <= f <= w["active"][1]
            d.text((c * (S + GAP) + 6, y + 6), f"{v['name']}  f{f}" + ("  HITBOX" if act else ""), fill=(255, 80, 80) if act else (255, 255, 0))
    im.save(os.path.join(tmp, f"{i:04d}.png"))
os.system(f'ffmpeg -y -loglevel error -framerate 60 -i "{tmp}/%04d.png" -pix_fmt yuv420p -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" "{mp4}"')
print(f"{card['move']}: {n + 1} frames x {len(views)} cameras, hitstop {hit} at f{a0} -> {mp4}")
