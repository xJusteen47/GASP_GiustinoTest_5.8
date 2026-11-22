"""Motion-path overlays on large UE renders: per-frame dots and lines of the main bones (hands, elbows, head, pelvis,
feet) over the real mesh at one pose, the fitted arcs of motion_check.py (dashed) and every flagged frame (red ring),
beats that are not flagged (orange). Same cameras as legibility_shots.py (768 px).
Usage: python traj_overlay.py <take> <pose frame> "<view,...>"   (run motion_check.py <take> first)
Output: sheets/legib_<take>/<take>_traj_f<NN>_<view>.png"""
import sys, os, json
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility_shots as S
L = S.L

COLB = {"hand_l": (230, 57, 70), "elbow_l": (82, 183, 136), "hand_r": (247, 37, 133), "elbow_r": (160, 90, 230),
        "head": (242, 193, 78), "pelvis": (120, 200, 255), "foot_l": (168, 218, 220), "foot_r": (229, 193, 200), "knee_l": (255, 140, 60), "toe_l": (230, 57, 70)}

def draw(take, frame, vname, m):
    import ue
    view = S.view_def(vname)
    d = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))
    tag = f"traj_{take}_f{frame}_{vname}"
    ue.py("capture", kind="lit", az=view["az"], el=view["el"], tag=tag, size=S.SIZE, dist=view["dist"], fov=view["fov"],
          center=list(view.get("center", L.CENTER)))
    rgb = np.fromfile(os.path.join(L.RAW, tag + ".rgb"), dtype=np.uint8).reshape(S.SIZE, S.SIZE, 3)
    im = Image.fromarray((rgb * 0.45).astype(np.uint8)).convert("RGB")
    dr = ImageDraw.Draw(im)
    sheet = m["move_sheet"]; faf = sheet["faf"]
    last = min(faf + 4, len(d["frames"]) - 1)
    flagged = {}
    for b, r in m["jitter"].items():
        for a in r["arcs"]:
            if a["flags"]:
                for f in range(a["frames"][0], a["frames"][1] + 1):
                    flagged.setdefault(b, set()).add(f)
        for f in r["zigzag_frames"] + r["wobble_frames"]:
            flagged.setdefault(b, set()).add(f)
    beats = {b: {f["frame"] for f in r["flags"]} for b, r in m["jitter"].items()}
    for b, r in m["jitter"].items():
        P = np.array(r["path"])[: last + 1]
        pts = [L.project(view, p) for p in P]
        col = COLB[b]
        dr.line(pts, fill=col, width=2)
        for i, (x, y) in enumerate(pts):
            rr = 3
            dr.ellipse([x - rr, y - rr, x + rr, y + rr], fill=col)
            if i in flagged.get(b, set()):
                dr.ellipse([x - 7, y - 7, x + 7, y + 7], outline=(255, 30, 30), width=2)
            elif i in beats.get(b, set()):
                dr.ellipse([x - 6, y - 6, x + 6, y + 6], outline=(255, 160, 0), width=1)
            if i % 4 == 0 or i == sheet["contact"][0] or i == faf:
                dr.text((x + 5, y - 12), str(i), fill=col)
        for a in r["arcs"]:
            if not a["flags"]:
                continue
            fit = a["fit"]; c = np.array(fit["centre"]); e1, e2 = np.array(fit["e1"]), np.array(fit["e2"])
            # the circle's angle is measured from the fit centre in the e1/e2 plane: rebuild it at the fitted radius
            angs = np.radians(np.linspace(fit["a0"], fit["a1"], 40))
            arc3 = [c + fit["R"] * (np.cos(t) * e1 + np.sin(t) * e2) for t in angs]
            ap = [L.project(view, q) for q in arc3]
            for k in range(0, len(ap) - 1, 2):
                dr.line([ap[k], ap[k + 1]], fill=(255, 255, 255), width=1)
    top = Image.new("RGB", (S.SIZE, S.SIZE + 64), (24, 24, 24))
    top.paste(im, (0, 64))
    dt = ImageDraw.Draw(top)
    dt.text((10, 6), f"{take} motion paths, frames 0-{last} over the pose at frame {frame} | {S.NAMES.get(vname, vname)}", fill=(255, 255, 255), font=S.font(20))
    x = 10
    for b, col in COLB.items():
        dt.rectangle([x, 40, x + 12, 52], fill=col); dt.text((x + 16, 37), b, fill=(220, 220, 220), font=S.font(14)); x += 24 + int(dt.textlength(b, font=S.font(14)))
    dt.text((x + 6, 37), "red ring = flagged | orange = beat | dashed = fitted arc", fill=(220, 220, 220), font=S.font(14))
    out = os.path.join(L.SHEETS, f"legib_{take}", f"{take}_traj_f{frame:02d}_{vname}.png")
    top.save(out)
    return out

if __name__ == "__main__":
    take, frame, views = sys.argv[1], int(sys.argv[2]), sys.argv[3].split(",")
    m = json.load(open(os.path.join(L.SHEETS, f"{take}_motion.json")))
    d = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))
    import ue
    ue.py("setup")
    S.settle(d["anim"], frame, d["frames"][str(frame)])
    for v in views:
        print(draw(take, frame, v, m))
