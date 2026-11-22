"""Large per-pose, per-view legibility shots: the real mesh (UE lit capture) next to the physics-asset parts, each part in
its own flat colour.

Why: black silhouettes hide what happens inside the outline (the rear arm over the fist). Here the right panel fills each
pixel with the colour of the front-most part and also traces every part's full outline. A fist hidden behind the rear
arm still shows as a red contour, and the mesh outline (black) shows how well the capsules match the real body.
Both panels use the same perspective camera as the lit videos (FOV 40, 320 cm from CENTER), 768 px square.

Usage: python legibility_shots.py <take> "<frame:label,...>" "<view,...>"   (take must have been measured by legibility.py)
  views: az<deg>[e<el>] (el default 10) or game (az180, el 20)
Output: sheets/legib_<take>/<take>_f<NN>_<label>_<view>.png"""
import sys, os, json, time, math
import numpy as np
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility as L

SIZE = 768
L.RES = SIZE
COL = {"head": (242, 193, 78), "torso": (141, 153, 174), "upperarm_l": (42, 157, 143), "lowerarm_l": (82, 183, 136),
       "hand_l": (230, 57, 70), "foot_l": (230, 57, 70), "upperarm_r": (58, 12, 163), "lowerarm_r": (114, 9, 183), "hand_r": (247, 37, 133),
       "thigh_l": (168, 218, 220), "shin_l": (207, 232, 232), "thigh_r": (181, 131, 141), "shin_r": (229, 193, 200)}
LEGEND = [("hand_l", "attacking fist"), ("lowerarm_l", "lead forearm"), ("upperarm_l", "lead upper arm"),
          ("hand_r", "rear hand"), ("lowerarm_r", "rear forearm"), ("upperarm_r", "rear upper arm"),
          ("head", "head + neck"), ("torso", "torso + clavicles")]
NAMES = {"az315": "3/4 front (rear-arm side)", "az45": "3/4 front (lead side)", "az270": "side (rear-arm side)",
         "az90": "side (lead side)", "az225": "3/4 rear", "az135": "3/4 rear (lead side)", "az180": "rear", "az0": "front",
         "g0": "game camera, straight behind", "g-15": "game camera, 15 deg off behind", "g+15": "game camera, 15 deg off behind",
         "g-30": "game camera, 30 deg off behind", "g+30": "game camera, 30 deg off behind"}

def view_def(v):
    if v in L.EXTRA_VIEWS:   # gameplay cameras (see legibility.py): centred crops of the real views
        return dict(L.EXTRA_VIEWS[v])
    if not v.startswith("az"):
        return dict(L.VIEWS[L.VNAMES.index(v)])
    az, _, el = v[2:].partition("e")
    return {"name": v, "kind": "persp", "az": int(az), "el": float(el or 10), "dist": 320.0, "fov": 40.0}

def font(sz):
    for f in ("C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"):
        if os.path.exists(f):
            return ImageFont.truetype(f, sz)
    return ImageFont.load_default()

def settle(anim, frame, fr):
    import ue
    want = L.bone_tf(fr, "hand_r")[0]
    ue.py("set_frame", anim_path=anim, frame=(frame + 5) % 38, hide_arm=False)
    for _ in range(30):
        got = ue.py("set_frame", anim_path=anim, frame=frame, hide_arm=False)["hand_r"]
        if max(abs(a - b) for a, b in zip(got, want)) < 0.5:
            ue.py("set_frame", anim_path=anim, frame=frame, hide_arm=False)
            return
        time.sleep(0.2)
    raise RuntimeError(f"pose {frame} did not settle")

def parts_panel(fr, view, mesh):
    depth = L.render(L.part_prims(fr), view)
    masks = np.isfinite(depth)
    U = masks.any(axis=0)
    idmap = np.where(U, depth.argmin(axis=0), -1)
    img = np.full((SIZE, SIZE, 3), 250, np.uint8)
    for k, p in enumerate(L.PNAMES):
        img[idmap == k] = COL[p]
    # every part's full outline in a darker shade of its colour; the attacking fist last and thick, so it shows when hidden
    for k, p in enumerate(L.PNAMES):
        if p != L.ATT:
            img[L.boundary(masks[k])] = tuple(int(c * 0.55) for c in COL[p])
    if mesh is not None:
        img[L.boundary(mesh)] = (0, 0, 0)
    hl = masks[L.PNAMES.index(L.ATT)]
    b = L.boundary(hl)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            img[np.roll(np.roll(b, dy, 0), dx, 1)] = (200, 0, 0)
    # numbers for the caption
    rear = masks[[L.PNAMES.index(q) for q in L.HIDERS]].any(axis=0)
    rest = masks[[i for i, q in enumerate(L.PNAMES) if q not in L.HIDERS + [L.ATT, L.ATT_FORE]]].any(axis=0)
    others = masks[[i for i, q in enumerate(L.PNAMES) if q not in (L.ATT, L.ATT_FORE)]].any(axis=0)
    a = max(1, int(hl.sum()))
    m = {"fist_distinct": round(100 * float((hl & ~others).sum()) / a), "fist_hidden_by_rear_arm_only": round(100 * float((hl & rear & ~rest).sum()) / a),
         "fist_visible_front": round(100 * float((idmap == L.PNAMES.index(L.ATT)).sum()) / a)}
    arm = masks[[L.PNAMES.index(L.ATT_FORE), L.PNAMES.index(L.ATT)]].any(axis=0)
    notarm = masks[[i for i, q in enumerate(L.PNAMES) if q not in L.ATT_CHAIN]].any(axis=0)
    m["arm_distinct"] = round(100 * float((arm & ~notarm).sum()) / max(1, int(arm.sum())))
    if mesh is not None:
        m["iou_capsules_vs_mesh"] = round(float((mesh & U).sum()) / max(1, float((mesh | U).sum())), 2)
    return img, m

def shot(take, anim, frames, frame, label, vname, out_dir):
    import ue
    view = view_def(vname)
    fr = frames[str(frame)]
    tag = f"shot_{take}_f{frame}_{vname}"
    ue.py("capture", kind="lit", az=view["az"], el=view["el"], tag=tag, size=SIZE, dist=view["dist"], fov=view["fov"],
          center=list(view.get("center", L.CENTER)))
    rgb = np.fromfile(os.path.join(L.RAW, tag + ".rgb"), dtype=np.uint8).reshape(SIZE, SIZE, 3)
    mesh = rgb.astype(int).sum(axis=2) > 3
    right, m = parts_panel(fr, view, mesh)
    r = json.load(open(os.path.join(L.SHEETS, f"{take}_legibility.json")))["frames"][str(frame)]
    W, H, TOP, LEG = SIZE * 2, SIZE, 74, 34
    canvas = Image.new("RGB", (W, TOP + H + LEG), (24, 24, 24))
    canvas.paste(Image.fromarray(rgb), (0, TOP)); canvas.paste(Image.fromarray(right), (SIZE, TOP))
    d = ImageDraw.Draw(canvas)
    d.text((14, 8), f"{take}  frame {frame}  {label}  |  {NAMES.get(vname, vname)}  (az {view['az']}, el {view['el']:.0f})", fill=(255, 255, 255), font=font(26))
    d.text((14, 42), f"attacking arm distinct {m['arm_distinct']}%   fist distinct {m['fist_distinct']}%   fist hidden only by the rear arm {m['fist_hidden_by_rear_arm_only']}%   "
                     f"rear elbow behind back {r['elbow_r_behind_back_cm']} cm   finger curl L {r['curl_l']} / R {r['curl_r']} deg   "
                     f"capsules vs mesh IoU {m.get('iou_capsules_vs_mesh')}", fill=(210, 210, 210), font=font(17))
    x = 14
    for p, txt in LEGEND:
        d.rectangle([x, TOP + H + 9, x + 16, TOP + H + 25], fill=COL[p])
        d.text((x + 22, TOP + H + 7), txt, fill=(230, 230, 230), font=font(16)); x += 30 + int(d.textlength(txt, font=font(16)))
    d.text((x + 10, TOP + H + 7), "red contour = attacking fist (also when hidden); black = mesh outline", fill=(230, 230, 230), font=font(16))
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, f"{take}_f{frame:02d}_{label}_{vname}.png")
    canvas.save(path)
    return path, m

if __name__ == "__main__":
    take = sys.argv[1]; L.configure(take)
    poses = [(int(a), b) for a, b in (x.split(":") for x in sys.argv[2].split(","))]
    views = sys.argv[3].split(",")
    d = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))
    import ue
    ue.py("setup")
    out_dir = os.path.join(L.SHEETS, f"legib_{take}")
    res = {}
    for frame, label in poses:
        settle(d["anim"], frame, d["frames"][str(frame)])
        for v in views:
            p, m = shot(take, d["anim"], d["frames"], frame, label, v, out_dir)
            res[os.path.basename(p)] = m
            print(os.path.basename(p), json.dumps(m), flush=True)
    json.dump(res, open(os.path.join(out_dir, f"{take}_shots.json"), "w"), indent=1)
