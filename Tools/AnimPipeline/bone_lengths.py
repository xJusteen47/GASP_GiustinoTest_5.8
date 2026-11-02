"""Bone-length check: every bone must keep its reference length on every frame (root and pelvis excepted, they carry the
motion). A rig-point solve that places each bone at its own point instead of at the end of its parent stretches the chain;
Ganondorf v3 (01/11) had the head's offset from neck_02 swing from 1.4 to 22.7 cm (reference 5.4): "the neck detaches from
the head". Fails on any change above 0.1 cm.

    python bone_lengths.py <file> [--ref=<file>] [--tol=0.1] [--json=<out.json>]

<file> / --ref: the model's rig points (<take>_targets_raw.json, --ref=idle_points.json: rigid point pairs), local tracks (ue_write_take.py's sheets/raw/<Asset>_tracks.json), a template frame-0 cache
(sheets/raw/<Template>_frame0.json), or component-space bones (sheets/raw/<take>_legbones.json, pose_solver.py output).
The reference is the --ref file's frame 0, else the file's own frame 0. Component-space files use the mannequin hierarchy
below (bones whose parent is missing from the file are skipped). Exit 1 on a failure; the first output line is PASS/FAIL.
Cascadeur takes stretch a little on their own (Ganondorf v2: clavicles up to 0.6 cm, spine 0.3 cm, neck/head exact), so
the Cascadeur pass reports this check and the direct-to-Unreal writer gates on it."""
import json, os, sys
import numpy as np

TOL = 0.1                                      # cm
FREE = ("root", "pelvis")                      # bones whose translation is animated

PARENT = {"pelvis": "root", "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "spine_04": "spine_03",
          "spine_05": "spine_04", "neck_01": "spine_05", "neck_02": "neck_01", "head": "neck_02"}
for _s in "lr":
    PARENT.update({f"clavicle_{_s}": "spine_05", f"upperarm_{_s}": f"clavicle_{_s}", f"lowerarm_{_s}": f"upperarm_{_s}",
                   f"hand_{_s}": f"lowerarm_{_s}", f"upperarm_twist_01_{_s}": f"upperarm_{_s}",
                   f"upperarm_twist_02_{_s}": f"upperarm_{_s}", f"lowerarm_twist_01_{_s}": f"lowerarm_{_s}",
                   f"lowerarm_twist_02_{_s}": f"lowerarm_{_s}", f"thigh_{_s}": "pelvis", f"calf_{_s}": f"thigh_{_s}",
                   f"foot_{_s}": f"calf_{_s}", f"ball_{_s}": f"foot_{_s}"})
    for _f in ("thumb", "index", "middle", "ring", "pinky"):
        _chain = ([] if _f == "thumb" else ["metacarpal"]) + ["01", "02", "03"]
        _prev = f"hand_{_s}"
        for _k in _chain:
            PARENT[f"{_f}_{_k}_{_s}"] = _prev; _prev = f"{_f}_{_k}_{_s}"


# rig-point pairs that are one rigid bone in the model (targets files: <take>_targets_raw.json); reference = idle_points.json
POINT_PAIRS = [("pelvis_MainPoint", "spine_02_MainPoint"), ("spine_02_MainPoint", "spine_04_MainPoint"),
               ("spine_04_MainPoint", "neck_01_MainPoint"), ("neck_01_MainPoint", "head_MainPoint"),
               ("head_MainPoint", "head_DirectionPoint")]
for _s in "lr":
    POINT_PAIRS += [(f"clavicle_MainPoint_{_s}", f"upperarm_MainPoint_{_s}"), (f"upperarm_MainPoint_{_s}", f"lowerarm_MainPoint_{_s}"),
                    (f"lowerarm_MainPoint_{_s}", f"hand_MainPoint_{_s}"), (f"thigh_MainPoint_{_s}", f"calf_MainPoint_{_s}"),
                    (f"calf_MainPoint_{_s}", f"foot_MainPoint_{_s}"), (f"foot_MainPoint_{_s}", f"ball_MainPoint_{_s}")]


def _pos(v):
    return np.asarray(v[0] if isinstance(v[0], (list, tuple)) else v[:3], float)


def lengths(path):
    """{bone: [length per frame]} of a tracks, frame-0 cache or component-space file."""
    d = json.load(open(path))
    if "bones" in d:                                                        # local tracks: length of the translation
        return {b: [float(np.linalg.norm(k[:3])) for k in keys] for b, keys in d["bones"].items()}
    if "local" in d:                                                        # template frame-0 cache
        return {b: [float(np.linalg.norm(v[:3]))] for b, v in d["local"].items()}
    if "targets" in d or "points" in d:                                     # model rig points / idle points
        P = d.get("targets") or {n: [v] for n, v in d["points"].items()}
        return {f"{a}->{b}": [float(np.linalg.norm(np.subtract(pa, pb))) for pa, pb in zip(P[a], P[b])]
                for a, b in POINT_PAIRS if a in P and b in P}
    F = d["frames"]; order = sorted(F, key=int)
    out = {}
    for b, p in PARENT.items():
        if b in F[order[0]] and p in F[order[0]]:
            out[b] = [float(np.linalg.norm(_pos(F[f][b]) - _pos(F[f][p]))) for f in order]
    return out


def check(L, ref=None, tol=TOL, free=FREE):
    """Failures {bone: (worst deviation, frame, length, reference)}; ref {bone: length}, default each bone's frame 0."""
    bad = {}
    for b, ls in L.items():
        if b in free or not ls:
            continue
        r = ref.get(b) if ref is not None else ls[0]
        if r is None:
            continue
        dev = [abs(x - r) for x in ls]; f = int(np.argmax(dev))
        if dev[f] > tol:
            bad[b] = (dev[f], f, ls[f], r)
    return bad


def report(bad, n_bones, tol=TOL):
    if not bad:
        return f"PASS bone lengths: {n_bones} bones within {tol} cm of the reference on every frame"
    lines = [f"FAIL bone lengths: {len(bad)} bones change by more than {tol} cm"]
    for b, (dv, f, l, r) in sorted(bad.items(), key=lambda kv: -kv[1][0]):
        lines.append(f"  {b:<22} {dv:6.2f} cm off at frame {f:<3} ({l:.2f} vs reference {r:.2f})")
    return "\n".join(lines)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[1:] if a.startswith("--"))
    tol = float(opt.get("tol") or TOL)
    L = lengths(args[0])
    ref = {b: ls[0] for b, ls in lengths(opt["ref"]).items()} if opt.get("ref") else None
    bad = check(L, ref, tol)
    print(report(bad, len(L), tol))
    if opt.get("json"):
        json.dump({"tol": tol, "fail": bool(bad), "bad": {b: list(v) for b, v in bad.items()}}, open(opt["json"], "w"), indent=1)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
