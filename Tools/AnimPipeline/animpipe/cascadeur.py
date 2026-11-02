"""Card <-> Cascadeur round trip with sparse keys (owner, 2026-11-02: edit in Cascadeur, never baked curves).

    python -m animpipe casc-export <card.json>                    card take -> .casc with keys ONLY on the card's key frames
    python -m animpipe casc-import <card.json> [<scene.casc>] [--bones=<json>] [--unlock=part@frame,...] [--dry]
    python -m animpipe casc-revert <card.json> [<snapshot.json>]  put back the card as it was before an import (default: last)
    python -m animpipe casc-diff <cardA.json> <cardB.json>        per key frame, the parts whose pose differs (solved bones)

The card stays the source of truth. Export: the card's source take (author poses solved per frame) goes to Cascadeur on
the UEFN mannequin rig (same proportions as Unreal, so no scaling), and every key that is not a card key frame is deleted;
the in-betweens Cascadeur shows are its own curves, a preview only: the build keeps the card's spacing.
Import: Cascadeur's key frames are read back as component-space bones; for every key frame and every part (locks.py
parts: arm_x incl. clavicle, leg_x, trunk, head) the bones are compared with the card's solved pose relative to the
part's parent. A part within PART_CM / PART_DEG is left byte for byte as it is in the card; a changed part gets its
`author` fields fitted from the bones (fit()); fingers and wrist_out always stay the card's (Cascadeur hands are
default). A key frame the card does not have becomes a new pose (author + poses list). A changed part under an
`approved` lock stops the import unless --unlock part@frame. Before writing, the card is copied to
cards/<kit>/history/<move>_<stamp>.json (casc-revert puts it back). After an import: `author`, `build`, `check`.
fit() can only express what author.py can: one trunk rotation spread evenly on spine_01..05, one head rotation spread on
the neck, two-bone limbs with a hand/foot rotation. What the bones do beyond that is reported as the residual (worst bone,
cm) and a frame over FIT_CM is flagged, never silently approximated.
"""
import copy, glob, json, math, os, shutil, time
import numpy as np

from animpipe import card as C
from animpipe.author import Poser, PARENT, SPINE, NECK, rv, frames as author_frames

PART_CM, PART_DEG = 0.5, 1.0     # same tolerance as the approved-pose locks (locks.py)
FIT_CM = 1.0                     # owner's 1 cm round-trip rule
PARTS = {   # part -> (parent bone or None = component space, bones compared)
    "trunk": (None, ("pelvis",) + tuple(SPINE)),
    "head": ("spine_05", tuple(NECK)),
    "arm_l": ("spine_05", ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l")),
    "arm_r": ("spine_05", ("clavicle_r", "upperarm_r", "lowerarm_r", "hand_r")),
    "leg_l": ("pelvis", ("thigh_l", "calf_l", "foot_l", "ball_l")),
    "leg_r": ("pelvis", ("thigh_r", "calf_r", "foot_r", "ball_r")),
}
FIELDS = {"trunk": ("pelvis", "spine"), "head": ("head",), "arm_l": ("arm_l", "clavicle_l"), "arm_r": ("arm_r", "clavicle_r"),
          "leg_l": ("leg_l",), "leg_r": ("leg_r",)}
RESID_BONES = [b for p in PARTS.values() for b in p[1]]


# ---------------------------------------------------------------- math
def rotvec(m):
    """Rotation vector (degrees) of a rotation matrix; inverse of author.rv."""
    c = max(-1.0, min(1.0, (np.trace(m) - 1) / 2)); a = math.acos(c)
    if a < 1e-7:
        return np.zeros(3)
    if a > math.pi - 1e-4:                       # near 180: axis from the symmetric part
        k = np.sqrt(np.maximum((np.diag(m) + 1) / 2, 0)); i = int(np.argmax(k))
        k[(i + 1) % 3] *= np.sign(m[i, (i + 1) % 3] + 1e-12); k[(i + 2) % 3] *= np.sign(m[i, (i + 2) % 3] + 1e-12)
        return k / np.linalg.norm(k) * math.degrees(a)
    v = np.array([m[2, 1] - m[1, 2], m[0, 2] - m[2, 0], m[1, 0] - m[0, 1]]) / (2 * math.sin(a))
    return v * math.degrees(a)


def ang(a, b):
    return math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(a.T @ b) - 1) / 2))))


def q2m(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def _r1(v):
    return [round(float(x), 1) + 0.0 for x in v]


def bones_from_json(fr):
    """{bone: [[x,y,z], [qx,qy,qz,qw]]} (author.py's *_bones.json) or {bone: [x,y,z,pitch,yaw,roll]} (UE sample_pose,
    legibility cache) -> (P, R)."""
    P, R = {}, {}
    for b, v in fr.items():
        if len(v) == 2:
            P[b] = np.array(v[0], float); R[b] = q2m(np.array(v[1], float))
        else:
            from animpipe.author import _rot_to_quat
            P[b] = np.array(v[:3], float); R[b] = q2m(_rot_to_quat(*v[3:6]))
    return P, R


def rel(P, R, b, parent):
    if parent is None:
        return P[b], R[b]
    return R[parent].T @ (P[b] - P[parent]), R[parent].T @ R[b]


def part_delta(PA, RA, PB, RB, part):
    """Worst (cm, deg) between two poses on a part's bones, relative to the part's parent."""
    par, bones = PARTS[part]
    cm = dg = 0.0
    for b in bones:
        pa, ra = rel(PA, RA, b, par); pb, rb = rel(PB, RB, b, par)
        cm = max(cm, float(np.linalg.norm(pa - pb))); dg = max(dg, ang(ra, rb))
    return cm, dg


def residual(PA, PB, bones=RESID_BONES):
    """Worst bone position gap (cm) in component space and its bone."""
    d = {b: float(np.linalg.norm(PA[b] - PB[b])) for b in bones if b in PA and b in PB}
    b = max(d, key=d.get)
    return d[b], b


# ---------------------------------------------------------------- fit: bones -> author fields
def fit(poser, P, R, old):
    """author fields that reproduce the bones (P, R); keys fit() does not own (fingers, wrist_out, ...) come from `old`."""
    lr, R0, P0 = poser.lr, poser.R0, poser.P0
    new = copy.deepcopy(old or {})
    Dpel = R["pelvis"] @ R0["pelvis"].T
    new["pelvis"] = {"loc": _r1(P["pelvis"] - P0["pelvis"]), "rot": _r1(rotvec(Dpel))}
    # the trunk is one rotation spread evenly on the five spine bones (world axes): R5 = rv(sp) Rpel L1..L5
    ch = R["pelvis"]
    for b in SPINE:
        ch = ch @ lr[b]
    new["spine"] = _r1(rotvec(R["spine_05"] @ ch.T))
    nk = R["spine_05"]
    for b in NECK:
        nk = nk @ lr[b]
    new["head"] = _r1(rotvec(R["head"] @ nk.T))
    for s in "lr":
        new[f"clavicle_{s}"] = _r1(rotvec(R[f"clavicle_{s}"] @ (R["spine_05"] @ lr[f"clavicle_{s}"]).T))
    # limbs: targets relative to the SOLVED origin (the author chain), so the world hand/foot stays where Cascadeur put it
    tmp = copy.deepcopy(new)
    for s in "lr":
        tmp.pop(f"arm_{s}", None); tmp.pop(f"leg_{s}", None)
    Ps, Rs = poser.solve(tmp)
    Dpel_s = rv(new["pelvis"]["rot"]); Dch = rv(new["spine"]) @ Dpel_s
    for s in "lr":
        for limb, a, m, e, origin, D, key, rk in (
                ("arm", f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}", Ps["spine_05"], Dch, "hand", "hand_rot"),
                ("leg", f"thigh_{s}", f"calf_{s}", f"foot_{s}", Ps["pelvis"], Dpel_s, "foot", "foot_rot")):
            spec = dict((old or {}).get(f"{limb}_{s}") or {})
            S, K, T = P[a], P[m], P[e]
            u = (T - S) / np.linalg.norm(T - S); w = (K - S) - u * np.dot(K - S, u); n = np.linalg.norm(w)
            spec[key] = _r1(D.T @ (T - origin))
            if n > 0.5 or "pole" not in spec:         # a straight limb keeps the card's hinge plane
                pole = K + (w / max(n, 1e-9)) * 40.0
                spec["pole"] = _r1(D.T @ (pole - origin))
            spec.pop(rk, None)
            new[f"{limb}_{s}"] = spec
    # end rotations last: they depend on the solved middle bone (and, for the hands, on wrist_out)
    Ps, Rs = poser.solve(new)
    for s in "lr":
        for limb, e, rk in (("arm", f"hand_{s}", "hand_rot"), ("leg", f"foot_{s}", "foot_rot")):
            r = rotvec(R[e] @ Rs[e].T)            # world-axis rotation applied on top of the solved end bone
            if np.linalg.norm(r) > 0.05:
                new[f"{limb}_{s}"][rk] = _r1(r)
    # hand_rot sits under wrist_out in author.solve (rv(wo) @ rv(hr) @ ...): one refinement pass absorbs the order
    for _ in range(2):
        Ps, Rs = poser.solve(new)
        for s in "lr":
            for limb, e, rk in (("arm", f"hand_{s}", "hand_rot"), ("leg", f"foot_{s}", "foot_rot")):
                err = R[e] @ Rs[e].T
                if ang(err, np.eye(3)) > 0.05:
                    cur = rv(new[f"{limb}_{s}"].get(rk, [0, 0, 0]))
                    if limb == "arm" and float((new.get("wrist_out") or [0])[0]):
                        # Rs = W rv(hr) X  ->  want R = W rv(hr') X  ->  rv(hr') = W^T R Rs^T W rv(hr)
                        W = _wrist(poser, new, Ps, s)
                        nh = W.T @ err @ W @ cur
                    else:
                        nh = err @ cur
                    new[f"{limb}_{s}"][rk] = _r1(rotvec(nh))
    return new


def _wrist(poser, pose, P, s):
    wo = float((pose.get("wrist_out") or [0])[0])
    Dch = rv(pose.get("spine", [0, 0, 0])) @ rv((pose.get("pelvis") or {}).get("rot", [0, 0, 0]))
    fa = P[f"hand_{s}"] - P[f"lowerarm_{s}"]; out = Dch @ np.array([1.0 if s == "l" else -1.0, 0, 0])
    ax = np.cross(fa, out); n = np.linalg.norm(ax)
    return rv(ax / n * wo) if n > 1e-6 else np.eye(3)


# ---------------------------------------------------------------- locks (approved block, PR #24 format)
def locked(c, frame, part):
    for e in c.get("approved", []):
        if frame in e.get("frames", []) and part in e.get("parts", []):
            return e
    return None


# ---------------------------------------------------------------- import
def _card_path(p):
    return p if os.path.isabs(p) else os.path.join(C.TOOLS, p)


def merge(c, bones, unlock=(), poser=None):
    """New card (copy) with the Cascadeur bones merged in. bones = {frame(int): (P, R)} for Cascadeur's key frames.
    Returns (card, report). Raises ValueError on a lock."""
    poser = poser or Poser()
    c = copy.deepcopy(c)
    au = c["author"]; poses = au["poses"]
    interp = author_frames(au)
    rep = {"frames": {}, "new": [], "missing": [], "flag": []}
    blocked = []
    for f in sorted(bones):
        P, R = bones[f]
        is_new = str(f) not in poses
        old = poses.get(str(f)) or interp[f]
        Po, Ro = poser.solve(old)
        changed = {}
        for part in PARTS:
            cm, dg = part_delta(P, R, Po, Ro, part)
            if is_new or cm > PART_CM or dg > PART_DEG:
                changed[part] = [round(cm, 2), round(dg, 2)]
        if not changed:
            continue
        for part in changed:
            if not is_new and locked(c, f, part) and f"{part}@{f}" not in unlock:
                blocked.append(f"{part}@{f} ({changed[part][0]} cm, {changed[part][1]} deg)")
        fitted = fit(poser, P, R, old)
        pose = copy.deepcopy(old)
        for part in changed:
            for k in FIELDS[part]:
                if k in fitted:
                    pose[k] = fitted[k]
        Pn, _ = poser.solve(pose)
        cm, b = residual(P, Pn)
        rep["frames"][f] = {"parts": changed, "residual_cm": round(cm, 2), "worst_bone": b}
        if cm > FIT_CM:
            rep["flag"].append(f)
        poses[str(f)] = pose
        if is_new:
            rep["new"].append(f)
            c["poses"].append({"name": f"casc_{f}", "frame": f, "source": None})
    if blocked:
        raise ValueError("approved poses changed in Cascadeur (pass --unlock=part@frame to accept): " + ", ".join(blocked))
    rep["missing"] = sorted(int(k) for k in poses if int(k) not in bones)
    au["poses"] = {k: poses[k] for k in sorted(poses, key=int)}
    c["poses"].sort(key=lambda p: p["frame"])
    return c, rep


def snapshot(path):
    kit_dir = os.path.dirname(path)
    hist = os.path.join(kit_dir, "history"); os.makedirs(hist, exist_ok=True)
    name = os.path.splitext(os.path.basename(path))[0]
    stamp = time.strftime('%Y%m%d_%H%M%S'); dst = os.path.join(hist, f"{name}_{stamp}.json"); i = 1
    while os.path.exists(dst):                   # two snapshots in the same second (an import then a revert)
        dst = os.path.join(hist, f"{name}_{stamp}_{i}.json"); i += 1
    shutil.copyfile(path, dst)
    return dst


def write_card(path, c):
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(c, fh, indent=1, ensure_ascii=False)
        fh.write("\n")


def import_bones(card_path, bones, opt):
    path = _card_path(card_path)
    c = C.load(path)
    unlock = set(u for u in (opt.get("unlock") or "").split(",") if u)
    new, rep = merge(c, bones, unlock)
    out = os.path.join(_out(), f"{c['move']}_casc_import.json")
    json.dump(rep, open(out, "w"), indent=1)
    ch = rep["frames"]
    print(f"casc-import {c['move']}: {len(bones)} Cascadeur keys, {len(ch)} frames changed {sorted(ch)}, new {rep['new']}")
    for f in sorted(ch)[:3]:
        print(f"  {f}: {', '.join(ch[f]['parts'])} (residual {ch[f]['residual_cm']} cm {ch[f]['worst_bone']})")
    if rep["flag"] or rep["missing"]:
        print(f"  over {FIT_CM} cm: {rep['flag']}; card keys not keyed in Cascadeur (kept): {rep['missing']}")
    if not ch or "dry" in opt:
        print(("dry run, card untouched: " if ch else "nothing changed, card untouched: ") + out)
        return rep
    snap = snapshot(path)
    write_card(path, new)
    print(f"written; snapshot {os.path.relpath(snap, C.TOOLS)}; next: author, build, check")
    return rep


def revert(card_path, snap=None):
    path = _card_path(card_path)
    name = os.path.splitext(os.path.basename(path))[0]
    if not snap:
        snaps = sorted(glob.glob(os.path.join(os.path.dirname(path), "history", f"{name}_*.json")))
        if not snaps:
            raise SystemExit(f"no snapshot for {name}")
        snap = snaps[-1]
    snapshot(path)                      # the revert itself can be reverted
    shutil.copyfile(_card_path(snap), path)
    print(f"casc-revert {name}: card <- {os.path.relpath(_card_path(snap), C.TOOLS)}")


def diff(a, b):
    ca, cb = C.load(_card_path(a)), C.load(_card_path(b))
    poser = Poser()
    fa, fb = author_frames(ca["author"]), author_frames(cb["author"])
    keys = sorted(set(map(int, ca["author"]["poses"])) | set(map(int, cb["author"]["poses"])))
    rows = []
    for f in keys:
        if f not in fa or f not in fb:
            continue
        PA, RA = poser.solve(fa[f]); PB, RB = poser.solve(fb[f])
        parts = {p: [round(x, 1) for x in part_delta(PA, RA, PB, RB, p)] for p in PARTS}
        parts = {p: v for p, v in parts.items() if v[0] > PART_CM or v[1] > PART_DEG}
        if parts:
            rows.append((f, parts))
    print(f"casc-diff: {len(rows)} key frames differ" + ("" if rows else " (same poses)"))
    for f, parts in rows[:4]:
        print(f"  {f}: " + ", ".join(f"{p} {v[0]} cm {v[1]} deg" for p, v in parts.items()))
    return rows


def _out():
    from animpipe.__main__ import OUT
    os.makedirs(OUT, exist_ok=True)
    return OUT


def load_bones_json(path):
    d = json.load(open(path))
    fr = d.get("frames", d)
    keys = d.get("keys")
    return {int(f): bones_from_json(v) for f, v in fr.items() if keys is None or int(f) in keys}


# ---------------------------------------------------------------- commands (Unreal through the bridge; Cascadeur steps
# go through the Cascadeur MCP tools: import_export.import_animation, keys.delete, keys.interpolation, scene.save)
CASC_DIR = os.path.join(os.path.expanduser("~"), "Documents", "Unreal Projects", "GASP_Giustino2", "Saved", "AnimPipeline", "casc")
SAMPLE_BONES = sorted({b for p in PARTS.values() for b in p[1]})


def _ue_py(entry, **kw):
    import ue
    r = ue.call("run_python_file", {"filePath": os.path.join(os.path.dirname(os.path.abspath(__file__)), "casc_ue.py").replace("\\", "/"),
                                    "entryPoint": entry, "kwargs": kw}, timeout=600)
    v = r.get("result")
    if not isinstance(v, str) or not v.startswith("{"):
        raise RuntimeError(f"{entry}: {str(v)[-500:]} | log: {str(r.get('log_output'))[-400:]}")
    return json.loads(v)


def export_cmd(card_path):
    c = C.load(_card_path(card_path))
    os.makedirs(CASC_DIR, exist_ok=True)
    fbx = os.path.join(CASC_DIR, f"{c['move']}_take.fbx").replace("\\", "/")
    r = _ue_py("export_fbx", asset=c["source_anim"], path=fbx)
    keys = sorted(int(k) for k in c["author"]["poses"])
    n = int(c["author"]["frames"])
    plan = {"card": card_path, "fbx": fbx, "casc": fbx.replace("_take.fbx", ".casc"), "frames": n, "keys": keys,
            "delete": [f for f in range(n) if f not in keys], "ue": r}
    out = os.path.join(CASC_DIR, f"{c['move']}_export.json")
    json.dump(plan, open(out, "w"), indent=1)
    print(f"casc-export {c['move']}: {c['source_anim']} -> {fbx} (ok {r.get('ok')}), keep keys {keys}")
    print(f"Cascadeur: UEFN rig scene, import_animation {os.path.basename(fbx)}, delete keys {len(plan['delete'])} frames, "
          f"bezier_clamped, save {os.path.basename(plan['casc'])}")
    print(out)
    return plan


def sample_take(asset, frames):
    import ue
    out = {}
    for s0 in range(0, len(frames), 8):
        for _ in range(20):        # a source-control dialog after a save blocks the bridge for a few seconds
            r = ue.call("sample_pose", {"assetPath": asset, "boneNames": SAMPLE_BONES, "frames": frames[s0:s0 + 8], "space": "component"})
            if not r.get("dialogBlocking"):
                break
            time.sleep(3)
        if "samples" not in r:
            raise RuntimeError(f"sample_pose {asset}: {str(r)[:300]}")
        for s in r["samples"]:
            out[int(s["frame"])] = bones_from_json({b["name"]: [b["location"][k] for k in "xyz"] + [b["rotation"][k] for k in ("pitch", "yaw", "roll")]
                                                    for b in s["bones"]})
    return out


def import_cmd(card_path, opt):
    """--fbx=<Cascadeur export> --keys=0,2,... (Cascadeur's key frames) or --bones=<json> (offline)."""
    c = C.load(_card_path(card_path))
    if opt.get("bones"):
        bones = load_bones_json(opt["bones"])
        if opt.get("keys"):
            ks = {int(k) for k in opt["keys"].split(",")}
            bones = {f: v for f, v in bones.items() if f in ks}
    else:
        import ue
        fbx = opt.get("fbx") or os.path.join(CASC_DIR, f"{c['move']}_casc.fbx")
        asset = f"AS_Casc_{c['move']}"
        r = ue.call("import_animation", {"filePath": fbx.replace("\\", "/"), "skeletonPath": "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin",
                                         "packagePath": "/Game/AnimPipeline/Pilot", "name": asset, "removeRedundantKeys": False,
                                         "importSettings": {"animationLength": "FBXALIT_ExportedTime"}})
        if not r.get("success"):
            raise RuntimeError(f"import {fbx}: {str(r)[:300]}")
        keys = [int(k) for k in opt["keys"].split(",")]
        bones = sample_take(f"/Game/AnimPipeline/Pilot/{asset}", keys)
    return import_bones(card_path, bones, opt)
