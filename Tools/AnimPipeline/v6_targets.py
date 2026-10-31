"""Offline: v5 rig points (v5_points.json, dumped from Cascadeur) -> v6 targets (v6_targets.json) via v6_motion.
Cascadeur space: Y up, the character faces +Z, its right side is -X. Punching arm = LEFT (lead), off-hand = RIGHT.
Usage: python v6_targets.py [params json overrides]"""
import sys, os, json, math
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v6_motion as vm

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = json.load(open(os.path.join(HERE, "v5_points.json")))
PTS = {n: np.array(v, dtype=float) for n, v in SRC["points"].items()}
N = SRC["frames"]
UP = np.array([0.0, 1.0, 0.0])

# group -> points; the first one is the main point the chain is built on
GROUPS = {
    "pelvis": ["pelvis_MainPoint", "pelvis_AdditionalPoint", "thigh_MainPoint_l", "thigh_MainPoint_r"],
    "spine2": ["spine_02_MainPoint", "spine_02_AdditionalPoint"],
    "chest": ["spine_04_MainPoint", "spine_04_AdditionalPoint"],
    "neck": ["neck_01_MainPoint", "neck_01_AdditionalPoint"],
    "head": ["head_MainPoint", "head_DirectionPoint", "head_AdditionalPoint"],
    "sho_l": ["upperarm_MainPoint_l", "clavicle_MainPoint_l", "clavicle_AdditionalPoint_l"],
    "elbow_l": ["lowerarm_MainPoint_l", "lowerarm_AdditionalPoint_l", "lowerarm_LimbDir_l"],
    "hand_l": ["hand_MainPoint_l", "hand_DirectionPoint_l", "hand_AdditionalPoint_l"],
    "sho_r": ["upperarm_MainPoint_r", "clavicle_MainPoint_r", "clavicle_AdditionalPoint_r"],
    "elbow_r": ["lowerarm_MainPoint_r", "lowerarm_AdditionalPoint_r", "lowerarm_LimbDir_r"],
    "hand_r": ["hand_MainPoint_r", "hand_DirectionPoint_r", "hand_AdditionalPoint_r"],
}
# extra orientation lag (frames) of the non-main points of a group: the wrist settles after the forearm,
# the head's aim lags a touch behind its position so the eyes stay on the target
ORIENT_LAG = {"hand_l": -1.2, "hand_r": -1.2, "head": -0.6}
# wrist relax in the tail: the hand's aim drops by this angle (deg) and comes back, after the hand's own drop
WRIST_RELAX = {"hand_l": (9.0, 18.5, 10.0), "hand_r": (8.0, 20.0, 10.0)}
# points that follow the pelvis by a fraction of its new-vs-v5 deviation (knees between planted feet and the hips)
KNEES = {"calf_MainPoint_l": 0.5, "calf_AdditionalPoint_l": 0.5, "calf_LimbDir_l": 0.5,
         "calf_MainPoint_r": 0.5, "calf_AdditionalPoint_r": 0.5, "calf_LimbDir_r": 0.5}

# The rear arm goes from the cover pose (elbow in front of the chest) to the guard (elbow out at the side) with the arm
# folded tight; in v5 the elbow plane turned ~70-80 deg in ONE frame (10->11), a visible pop. Between these frames the
# bend direction is re-authored to turn smoothly around the shoulder-hand line, the elbow is solved at fixed bone lengths,
# and the pole and the hand's aim turn with the forearm. side -> (first, last) frame.
ELBOW_SWING = {"r": (8, 15)}

def _unit(v):
    return v / (np.linalg.norm(v) + 1e-9)

def _rot_between(a, b):
    """Minimal rotation matrix taking direction a to direction b."""
    a, b = _unit(a), _unit(b)
    v = np.cross(a, b); c = float(np.dot(a, b))
    if np.linalg.norm(v) < 1e-9:
        return np.eye(3)
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx * (1 / (1 + c))

def _slerp_about(w0, w1, axis, s):
    """Rotate w0 toward w1 about `axis` by fraction s (both perpendicular to axis), the short way."""
    ang = math.atan2(float(np.dot(np.cross(w0, w1), axis)), float(np.dot(w0, w1)))
    t = ang * s
    return w0 * math.cos(t) + np.cross(axis, w0) * math.sin(t) + axis * float(np.dot(axis, w0)) * (1 - math.cos(t)), math.degrees(ang)

def smooth_elbow_swing(out, side, a, b):
    S, E, H = (out[f"{k}_{side}"] for k in ("upperarm_MainPoint", "lowerarm_MainPoint", "hand_MainPoint"))
    POLE, EADD = out[f"lowerarm_LimbDir_{side}"], out[f"lowerarm_AdditionalPoint_{side}"]
    HD, HA = out[f"hand_DirectionPoint_{side}"], out[f"hand_AdditionalPoint_{side}"]
    def bend_dir(f):
        u = _unit(H[f] - S[f]); e = E[f] - S[f]
        return _unit(e - u * np.dot(e, u))
    lu0, lu1 = np.linalg.norm(E[a] - S[a]), np.linalg.norm(E[b] - S[b])
    lf0, lf1 = np.linalg.norm(H[a] - E[a]), np.linalg.norm(H[b] - E[b])
    pd0, pd1 = np.linalg.norm(POLE[a] - E[a]), np.linalg.norm(POLE[b] - E[b])
    w_a, w_b = bend_dir(a), bend_dir(b)
    total = None
    newE = {}
    for f in range(a + 1, b):
        s = vm.smooth((f - a) / (b - a))
        u = _unit(H[f] - S[f])
        wa = _unit(w_a - u * np.dot(w_a, u)); wb = _unit(w_b - u * np.dot(w_b, u))
        w, ang = _slerp_about(wa, wb, u, s)
        total = ang if total is None else total
        w = _unit(w - u * np.dot(w, u))
        lu = lu0 + (lu1 - lu0) * s; lf = lf0 + (lf1 - lf0) * s
        d = np.linalg.norm(H[f] - S[f])
        x = (lu * lu - lf * lf + d * d) / (2 * d)
        h = math.sqrt(max(lu * lu - x * x, 0.0))
        e_new = S[f] + u * x + w * h
        r = _rot_between(H[f] - E[f], H[f] - e_new)
        HD[f] = H[f] + r @ (HD[f] - H[f])
        HA[f] = H[f] + r @ (HA[f] - H[f])
        EADD[f] = e_new + r @ (EADD[f] - E[f])
        POLE[f] = e_new + w * (pd0 + (pd1 - pd0) * s)
        newE[f] = e_new
    for f, e in newE.items():
        E[f] = e
    print(f"elbow swing {side}: frames {a}->{b}, bend direction turns {total:.0f} deg about the shoulder-hand line")

def main(params=None):
    missing = [p for g in GROUPS.values() for p in g if p not in PTS] + [k for k in KNEES if k not in PTS]
    groups = {g: [p for p in pts if p in PTS] for g, pts in GROUPS.items()}
    mainpts = {g: PTS[pts[0]] for g, pts in groups.items()}
    P = dict(vm.DEFAULT)
    if params:
        P.update({k: tuple(v) for k, v in params.items()})
    new, dbg = vm.build(mainpts, P, up=UP)
    out = {}
    for g, pts in groups.items():
        lead = P[g][0]
        m_new = new[g]; m_old = PTS[pts[0]]
        for p in pts:
            traj = PTS[p].copy()
            for f in range(vm.REC0, N):
                if f <= vm.REC1:
                    t = vm.warp(f, lead) + (ORIENT_LAG.get(g, 0.0) if p != pts[0] else 0.0)
                    t = min(max(t, vm.REC0), vm.REC1)
                    rel = vm.sample(PTS[p], t) - vm.sample(m_old, t)
                else:
                    rel = PTS[p][0] - m_old[0]
                if p != pts[0] and g in WRIST_RELAX and p.startswith("hand_DirectionPoint"):
                    ang, start, dur = WRIST_RELAX[g]
                    a = math.radians(ang) * vm.bell(f - start, dur)
                    # rotate the aim vector down around the horizontal axis perpendicular to it
                    axis = np.cross(rel, UP); na = np.linalg.norm(axis)
                    if na > 1e-6 and a != 0.0:
                        k = axis / na
                        rel = rel * math.cos(a) + np.cross(k, rel) * math.sin(a) + k * np.dot(k, rel) * (1 - math.cos(a))
                traj[f] = m_new[f] + rel
            # the guard pose is exact at the end (blend of v6_motion already did it for the main point)
            for f in range(vm.BLEND0, N):
                s = vm.smooth((f - vm.BLEND0) / (vm.TAIL1 - vm.BLEND0))
                traj[f] = traj[f] * (1 - s) + PTS[p][0] * s
            out[p] = traj
    # knees: v5 knee at the pelvis' warped time, shifted by part of the pelvis' deviation from that v5 pose
    lead_p = P["pelvis"][0]
    for k, w in KNEES.items():
        if k not in PTS:
            continue
        traj = PTS[k].copy()
        for f in range(vm.REC0, N):
            t = vm.warp(f, lead_p) if f <= vm.REC1 else 0.0
            base_t = t if f <= vm.REC1 else 0.0
            dev = new["pelvis"][f] - vm.sample(PTS["pelvis_MainPoint"], base_t)
            traj[f] = vm.sample(PTS[k], base_t) + w * dev
        for f in range(vm.BLEND0, N):
            s = vm.smooth((f - vm.BLEND0) / (vm.TAIL1 - vm.BLEND0))
            traj[f] = traj[f] * (1 - s) + PTS[k][0] * s
        out[k] = traj
    for side, (a, b) in ELBOW_SWING.items():
        smooth_elbow_swing(out, side, a, b)
    feet = [n for n in PTS if n.startswith("foot_") or n.startswith("ball_")]
    res = {"frames": list(range(vm.REC0, N)), "targets": {p: out[p][vm.REC0:].tolist() for p in out},
           "feet": {n: PTS[n][0].tolist() for n in feet}, "params": P, "missing": missing}
    json.dump(res, open(os.path.join(HERE, "v6_targets.json"), "w"))
    # timing report on main points (cm per frame), Cascadeur space
    cols = ["pelvis", "chest", "head", "sho_l", "elbow_l", "hand_l", "elbow_r", "hand_r"]
    print("missing points:", missing)
    print("  f " + " ".join(f"{c:>8s}" for c in cols))
    for f in range(3, N):
        print(f"{f:3d} " + " ".join(f"{np.linalg.norm(new[c][f] - new[c][f - 1]):8.2f}" for c in cols))
    print("deviation from guard at 17:", {c: round(float(np.linalg.norm(new[c][17] - mainpts[c][0])), 2) for c in cols})

if __name__ == "__main__":
    main(json.loads(sys.argv[1]) if len(sys.argv) > 1 else None)
