"""Motion checks the owner judges first: weight and force, secondary motion (overlap), smooth trajectories (no zig-zag
jitter), attack direction, and weapon arcs (prepared, active once a weapon bone exists).

Input: the component-space bone samples cached by legibility.py (sheets/raw/<take>_legbones.json). UE component space:
X = character's left, Y = forward, Z = up, 60 fps. Mass model: the physics-asset primitives (volume-weighted centroids).
Thresholds: motion_thresholds.json (calibration notes in jab_log.md).

Usage: python motion_check.py <take> [contact=first-last] [faf=17]
Output: sheets/<take>_motion.json, sheets/<take>_motion.txt, sheets/<take>_motion.png
Also runs (and adds their flags): joints.py, arm_check.py, balance_check.py (centre of mass over the feet, kicks'
counterbalance), pose_rules.py (lift-off weight shift, support knee, kick chain, twinning, head, foot slide)."""
import sys, os, json, math
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility as L

HERE = os.path.dirname(os.path.abspath(__file__))
TRAJ_BONES = {"hand_l": "middle_01_l", "hand_r": "middle_01_r", "elbow_l": "lowerarm_l", "elbow_r": "lowerarm_r", "knee_l": "calf_l", "toe_l": "ball_l",
              "head": "head", "pelvis": "pelvis", "foot_l": "foot_l", "foot_r": "foot_r"}
CHAINS = {"lead arm": ["upperarm_l", "lowerarm_l", "hand_l", "middle_01_l"],      # shoulder, elbow, wrist, hand
          "rear arm": ["upperarm_r", "lowerarm_r", "hand_r", "middle_01_r"],
          "lead leg": ["thigh_l", "calf_l", "foot_l", "ball_l"],
          "chest > head": ["spine_05", "head"],
          "pelvis > chest": ["pelvis", "spine_05"]}
JOINT_NAMES = {"thigh_l": "hip", "calf_l": "knee", "foot_l": "ankle", "ball_l": "toes", "upperarm_l": "shoulder", "lowerarm_l": "elbow", "hand_l": "wrist", "middle_01_l": "hand",
               "upperarm_r": "shoulder", "lowerarm_r": "elbow", "hand_r": "wrist", "middle_01_r": "hand",
               "spine_05": "chest", "head": "head", "pelvis": "pelvis"}
TH = {}

def load_th():
    p = os.path.join(HERE, "motion_thresholds.json")
    TH.update(json.load(open(p)) if os.path.exists(p) else {})

def series(frames, bone):
    return np.array([L.bone_tf(frames[str(f)], bone)[0] for f in range(len(frames))])

def com(fr):
    tot, acc = 0.0, np.zeros(3)
    for part, prims in L.part_prims(fr).items():
        for pr in prims:
            if pr[0] == "cap":
                a, b, r = pr[1], pr[2], pr[3]
                v = math.pi * r * r * np.linalg.norm(b - a) + 4.0 / 3.0 * math.pi * r ** 3
                c = (a + b) / 2
            else:
                c, e = pr[1], pr[3]
                v = 8 * e[0] * e[1] * e[2]
            tot += v; acc += v * c
    return acc / tot

def horiz(v):
    v = np.array(v, dtype=float); v[2] = 0
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v


# ---------------------------------------------------------------- 1. weight and force (Terry's forward tilt)
def weight(frames, c0, c1, faf, effector="middle_01_l"):
    fist = series(frames, effector); chest = series(frames, "spine_05"); pelvis = series(frames, "pelvis")
    a = horiz(fist[c0] - chest[0])                                    # attack direction: guard chest -> fist at contact
    C = np.array([com(frames[str(f)]) for f in range(len(frames))])
    fl, fr_ = series(frames, "foot_l"), series(frames, "foot_r")
    def tilt(f):                                                      # spine pitch toward the attack, deg (+ = leaning in)
        s = chest[f] - pelvis[f]
        return math.degrees(math.atan2(np.dot(s, a), s[2]))
    def over_feet(f):                                                 # COM along a between back foot (0) and front foot (1)
        pa, pb = sorted([np.dot(fl[f], a), np.dot(fr_[f], a)])
        return (np.dot(C[f], a) - pa) / max(1e-6, pb - pa)
    win = range(0, faf + 1)
    out = {"attack_dir_xy": [round(float(x), 3) for x in a[:2]],
           "spine_tilt_deg": {"guard": round(tilt(0), 1), "contact": round(tilt(c0), 1), "delta": round(tilt(c0) - tilt(0), 1),
                              "max": round(max(tilt(f) for f in win), 1)},
           "com_shift_cm": {"contact": round(float(np.dot(C[c0] - C[0], a)), 1), "max": round(max(float(np.dot(C[f] - C[0], a)) for f in win), 1)},
           "com_over_feet": {"guard": round(over_feet(0), 2), "contact": round(over_feet(c0), 2)},
           "pelvis_drop_cm": {"contact": round(float(pelvis[c0][2] - pelvis[0][2]), 1), "min": round(float(min(pelvis[f][2] for f in win) - pelvis[0][2]), 1)},
           "anticipation_cm": {"frames": c0, "com_back": round(min([0.0] + [float(np.dot(C[f] - C[0], a)) for f in range(0, c0)]), 1),
                               "fist_back": round(min([0.0] + [float(np.dot(fist[f] - fist[0], a)) for f in range(0, c0)]), 1)},
           "curves": {"tilt": [round(tilt(f), 1) for f in range(len(frames))],
                      "com_along": [round(float(np.dot(C[f] - C[0], a)), 1) for f in range(len(frames))],
                      "pelvis_dz": [round(float(pelvis[f][2] - pelvis[0][2]), 1) for f in range(len(frames))]}}
    return out, a


# ---------------------------------------------------------------- 2. secondary motion: speed-peak cascade
def peak_time(speed, lo, hi):
    """Fractional frame of the speed peak in [lo, hi) (speed[i] sits at i + 0.5), parabolic refinement."""
    seg = speed[lo:hi]
    i = int(np.argmax(seg)); k = lo + i
    if 0 < i < len(seg) - 1:
        y0, y1, y2 = seg[i - 1], seg[i], seg[i + 1]
        den = y0 - 2 * y1 + y2
        off = 0.5 * (y0 - y2) / den if abs(den) > 1e-9 else 0.0
    else:
        off = 0.0
    return k + 0.5 + off, float(speed[k])

def overlap(frames, c0, c1, faf):
    res = {}
    phases = {"punch": (0, c0 + 1), "recovery": (c1, faf + 1)}
    for name, bones in CHAINS.items():
        res[name] = {}
        for ph, (lo, hi) in phases.items():
            if hi - lo < TH.get("overlap_min_phase_frames", 3):
                res[name][ph] = {"peaks": [], "spread_frames": None, "lags": [], "note": f"{hi - lo - 1} frame(s): too short to cascade"}
                continue
            peaks = []
            for b in bones:
                p = series(frames, b)
                sp = np.linalg.norm(np.diff(p, axis=0), axis=1)
                t, v = peak_time(sp, lo, min(hi, len(sp)))
                peaks.append({"joint": JOINT_NAMES[b], "peak_frame": round(t, 2), "speed_cm_f": round(v, 1)})
            ts = [p["peak_frame"] for p in peaks]
            res[name][ph] = {"peaks": peaks, "spread_frames": round(max(ts) - min(ts), 2),
                             "lags": [round(ts[i + 1] - ts[i], 2) for i in range(len(ts) - 1)]}
    return res


# ---------------------------------------------------------------- 3. trajectories: zig-zag and jerk spikes
def jitter(frames, allowed):
    """Per bone: frames where the path's curvature flips side on consecutive frames (zig-zag), sharp reversals and jerk spikes,
    outside the allowed beats (contact, FAF). Positions in cm, 60 fps."""
    out = {}
    s_min = TH.get("jitter_min_speed_cm_f", 0.3)
    d_min = TH.get("jitter_min_dev_cm", 0.4)
    for label, bone in TRAJ_BONES.items():
        p = series(frames, bone)
        n = len(p)
        v = np.diff(p, axis=0)
        sp = np.linalg.norm(v, axis=1)
        d = np.zeros_like(p)                                           # deviation from the chord of the neighbours
        d[1:-1] = p[1:-1] - (p[:-2] + p[2:]) / 2
        # keep only the part of d normal to the local direction of travel
        for t in range(1, n - 1):
            u = p[t + 1] - p[t - 1]
            un = np.linalg.norm(u)
            if un > 1e-9:
                u /= un; d[t] = d[t] - np.dot(d[t], u) * u
        flags = []
        for t in range(1, n - 2):
            if t in allowed or t + 1 in allowed:
                continue
            moving = sp[t - 1] > s_min and sp[t] > s_min and sp[t + 1] > s_min
            big = np.linalg.norm(d[t]) > d_min and np.linalg.norm(d[t + 1]) > d_min
            if moving and big and np.dot(d[t], d[t + 1]) < 0:
                flags.append({"frame": t, "kind": "curvature flip", "dev_cm": [round(float(np.linalg.norm(d[t])), 2), round(float(np.linalg.norm(d[t + 1])), 2)]})
        for t in range(1, n - 1):
            if t in allowed:
                continue
            if sp[t - 1] > s_min and sp[t] > s_min:
                ang = math.degrees(math.acos(np.clip(np.dot(v[t - 1], v[t]) / (sp[t - 1] * sp[t]), -1, 1)))
                if ang > TH.get("jitter_reversal_deg", 120):
                    flags.append({"frame": t, "kind": "reversal", "deg": round(ang, 0)})
        j = np.linalg.norm(p[3:] - 3 * p[2:-1] + 3 * p[1:-2] - p[:-3], axis=1)   # jerk at t+1.5
        med = float(np.median(j)) if len(j) else 0.0
        for i, val in enumerate(j):
            t = i + 1
            if t in allowed or t + 1 in allowed:
                continue
            if val > TH.get("jerk_abs_cm", 3.0) and val > TH.get("jerk_rel", 4.0) * max(med, 0.1):
                flags.append({"frame": t, "kind": "jerk spike", "jerk_cm_f3": round(float(val), 1), "median": round(med, 2)})
        # zig-zag = the curvature flips side on >= zigzag_min_flips consecutive frames (a wobble), or two reversals within
        # three frames. One flip or one reversal is a beat (a snap, a spring), reported as info and drawn on the overlays.
        flips = sorted({f["frame"] for f in flags if f["kind"] == "curvature flip"})
        need = TH.get("zigzag_min_flips", 3)
        zz = set()
        for t in flips:
            run_ = [u for u in range(t, t + need) if u in flips]
            if len(run_) == need:
                zz.update(run_)
        revs = sorted(f["frame"] for f in flags if f["kind"] == "reversal")
        wob = {t for t in revs for u in revs if 0 < u - t <= 2} | {u for t in revs for u in revs if 0 < u - t <= 2}
        ok = set(range(n)) - set(allowed)
        A = arcs(p, ok)
        for a_ in A:
            a_["flags"] = arc_flags(a_)
        out[label] = {"flags": flags, "zigzag_frames": sorted(zz), "wobble_frames": sorted(wob), "arcs": A,
                      "path": [[round(float(x), 1) for x in q] for q in p]}
    return out


# ---------------------------------------------------------------- 3b. arcs: where jitter shows (owner: "circular movements")
def fit_arc(P):
    """3D points -> plane (SVD), circle in that plane (algebraic fit). Returns centre, normal, radius, in-plane basis,
    per-point radial deviation (cm), out-of-plane deviation (cm) and the angle of each point about the centre (deg, unwrapped)."""
    c = P.mean(axis=0)
    _, _, vt = np.linalg.svd(P - c)
    e1, e2, n = vt[0], vt[1], vt[2]
    Q = np.stack([(P - c) @ e1, (P - c) @ e2], axis=1)
    A = np.c_[2 * Q, np.ones(len(Q))]
    sol, *_ = np.linalg.lstsq(A, (Q ** 2).sum(axis=1), rcond=None)
    cx, cy = sol[0], sol[1]; R = math.sqrt(max(sol[2] + cx * cx + cy * cy, 1e-9))
    rad = np.sqrt(((Q - [cx, cy]) ** 2).sum(axis=1)) - R
    off = (P - c) @ n
    ang = np.degrees(np.unwrap(np.arctan2(Q[:, 1] - cy, Q[:, 0] - cx)))
    centre = c + cx * e1 + cy * e2
    return {"centre": centre, "normal": n, "R": R, "e1": e1, "e2": e2, "radial": rad, "offplane": off, "angle": ang}

def arcs(P, frames_ok, W=None):
    """Slide a window over the moving frames; keep windows that really turn (subtended angle and radius in range), then
    measure radial wobble, out-of-plane wobble and the angular-speed profile (accelerate-decelerate-accelerate = wobble)."""
    W = W or TH.get("arc_window", 7)
    out = []
    n = len(P)
    for s0 in range(0, n - W + 1):
        idx = list(range(s0, s0 + W))
        if not all(i in frames_ok for i in idx):
            continue
        seg = P[idx]
        V = np.diff(seg, axis=0); spd = np.linalg.norm(V, axis=1)
        if spd.min() < TH.get("jitter_min_speed_cm_f", 0.3):
            continue                                                      # a stop inside: not one continuous arc
        cosang = [float(np.dot(V[i], V[i + 1]) / (spd[i] * spd[i + 1])) for i in range(len(V) - 1)]
        if min(cosang) < math.cos(math.radians(TH.get("arc_max_turn_deg", 100))):
            continue                                                      # a sharp reversal (snap, spring): a beat, not an arc
        fa = fit_arc(seg)
        turn = [float(np.dot(np.cross(V[i], V[i + 1]), fa["normal"])) for i in range(len(V) - 1)]
        sig = [t for t in turn if abs(t) > TH.get("arc_turn_min", 0.05) * spd.mean() ** 2]
        sign_changes = sum(1 for i in range(len(sig) - 1) if sig[i] * sig[i + 1] < 0)
        if sign_changes == 1:
            continue                                                      # an S-curve: two arcs, not one
        zig = sign_changes >= 2
        span = abs(fa["angle"][-1] - fa["angle"][0])
        if not zig and span < TH.get("arc_min_deg", 40) or not (TH.get("arc_min_r_cm", 3) <= fa["R"] <= TH.get("arc_max_r_cm", 150)):
            continue
        w = np.diff(fa["angle"])                                          # deg per frame
        dw = np.diff(w)
        big = [x for x in dw if abs(x) > TH.get("arc_omega_wobble_rel", 0.25) * max(abs(w).mean(), 1e-6)]
        flips = sum(1 for i in range(len(big) - 1) if big[i] * big[i + 1] < 0)
        out.append({"frames": [idx[0], idx[-1]], "R_cm": round(fa["R"], 1), "span_deg": round(float(span), 0), "turn_sign_changes": sign_changes,
                    "radial_max_cm": round(float(np.abs(fa["radial"]).max()), 2), "offplane_max_cm": round(float(np.abs(fa["offplane"]).max()), 2),
                    "omega_deg_f": [round(float(x), 1) for x in w], "omega_wobble_flips": flips,
                    "radial_by_frame": {str(i): round(float(r), 2) for i, r in zip(idx, fa["radial"])},
                    "fit": {"centre": [round(float(x), 2) for x in fa["centre"]], "normal": [round(float(x), 4) for x in fa["normal"]],
                            "e1": [round(float(x), 4) for x in fa["e1"]], "e2": [round(float(x), 4) for x in fa["e2"]], "R": round(fa["R"], 2),
                            "a0": round(float(fa["angle"][0]), 1), "a1": round(float(fa["angle"][-1]), 1)}})
    return out

def arc_flags(a):
    rad_lim = lambda R: max(TH.get("arc_radial_min_cm", 0.6), TH.get("arc_radial_rel", 0.06) * R)
    fl = []
    if a.get("turn_sign_changes", 0) >= 2:
        fl.append("zig-zag (turning flips back and forth)")
    if a["radial_max_cm"] > rad_lim(a["R_cm"]):
        fl.append("radial wobble")
    if a["offplane_max_cm"] > TH.get("arc_offplane_max_cm", 1.0):
        fl.append("out-of-plane wobble")
    if a["omega_wobble_flips"] >= TH.get("arc_omega_min_flips", 2):
        fl.append("uneven angular speed")
    return fl

def _selftest_arcs():
    load_th()
    t = np.linspace(0, math.radians(150), 12)
    clean = np.stack([20 * np.cos(t), 20 * np.sin(t), np.zeros_like(t)], axis=1)
    noisy = clean.copy(); noisy[3:9, 0] += np.array([1.5, -1.5, 1.5, -1.5, 1.5, -1.5])
    tw = np.cumsum([0, 10, 20, 8, 22, 9, 21, 8, 20, 9, 19, 10]) * math.pi / 180
    wob = np.stack([20 * np.cos(tw), 20 * np.sin(tw), np.zeros_like(tw)], axis=1)
    ok = set(range(12))
    f = lambda P: {x for a in arcs(P, ok) for x in arc_flags(a)}
    fn = f(noisy)
    return not f(clean) and bool(fn & {"radial wobble", "zig-zag (turning flips back and forth)"}) and "uneven angular speed" in f(wob)


# ---------------------------------------------------------------- 4. impact spacing (Sakurai on Terry's forward tilt)
def spacing_metrics(pos, k0, c0):
    """Effector positions; anticipation key k0 -> contact c0. Share of the path in the last 1 and 2 frames, where the speed
    peaks, how even the build is, and the cumulative % curve."""
    d = [float(np.linalg.norm(pos[t + 1] - pos[t])) for t in range(k0, c0)]
    tot = sum(d) or 1e-9
    cum = [0.0]
    for x in d:
        cum.append(cum[-1] + x)
    build = d[:-2] if len(d) > 2 else []
    return {"frames": len(d), "anticipation_key": k0,
            "share_last1": round(d[-1] / tot, 3) if d else 1.0,
            "share_last2": round(sum(d[-2:]) / tot, 3) if d else 1.0,
            "peak_on_contact_frame": bool(d) and int(np.argmax(d)) == len(d) - 1,
            "build_ratio_last_first": round(build[-1] / max(build[0], 1e-6), 2) if len(build) >= 2 else None,
            "cumulative_pct": [round(100 * c / tot, 1) for c in cum]}

def impact_spacing(frames, c0, a, effector="middle_01_l"):
    """The anticipation key is the frame, before contact, where the effector is furthest back along the attack direction."""
    pos = series(frames, effector)
    k0 = min(range(0, c0 + 1), key=lambda f: float(np.dot(pos[f], a))) if c0 > 0 else 0
    k0 = min(k0, c0 - 1) if c0 > 0 else 0
    r = spacing_metrics(pos, k0, c0)
    r["effector"] = effector
    return r

def _selftest_spacing():
    import easing
    a = np.array([0.0, 1.0, 0.0])
    def path(prof):
        return np.array([[0.0, 60 * p, 100.0] for p in prof])
    good = spacing_metrics(path(easing.strike(8)), 0, 8)
    bad = spacing_metrics(path(easing.even(8)), 0, 8)
    return good["share_last2"] >= 0.6 and good["peak_on_contact_frame"] and bad["share_last2"] < 0.6


# ---------------------------------------------------------------- 4b. hold and recoil (Ryu's forward smash: the limb stays out and recoils)
def hold_recoil(frames, c0, a, sheet):
    """Extension = effector progress along the attack, 0 at guard, 1 at first contact. Hold = consecutive frames after c0
    within hold_tolerance of full extension. Recoil = small back-and-forth of the effector (and chest) inside the hold:
    reversals of its motion along the attack, amplitude in cm, decay of the swings."""
    eff = series(frames, sheet.get("effector", "middle_01_l")); chest = series(frames, "spine_05")
    x = np.array([float(np.dot(p - eff[0], a)) for p in eff])
    full = x[c0] if abs(x[c0]) > 1e-6 else 1e-6
    ext = x / full
    # hold = the pose held near extension: the first hold_settle_frames after contact may settle into it, then the limb
    # stays >= hold_min_extension and moves < hold_speed_max of its reach per frame (calibrated on UFD: Ryu fsmash ~13 with
    # recoil, Little Mac fsmash ~12 frozen, Terry ftilt 5, Mario jab 1)
    hold = 0
    for f in range(c0 + 1, len(ext)):
        if ext[f] >= TH.get("hold_min_extension", 0.6) and (f <= c0 + TH.get("hold_settle_frames", 2) or abs(ext[f] - ext[f - 1]) < TH.get("hold_speed_max", 0.06)):
            hold += 1
        else:
            break
    def osc(series_):
        seg = series_[c0:c0 + hold + 1]
        dv = np.diff(seg)
        rev = int(sum(1 for i in range(len(dv) - 1) if dv[i] * dv[i + 1] < 0 and abs(dv[i]) > 0.1 and abs(dv[i + 1]) > 0.1))
        amp = float(seg.max() - seg.min()) if len(seg) else 0.0
        swings = [abs(v) for v in dv if abs(v) > 0.1]
        decays = all(swings[i + 1] <= swings[i] * 1.05 for i in range(len(swings) - 1)) if len(swings) > 1 else None
        return {"reversals": rev, "amplitude_cm": round(amp, 2), "decaying": decays}
    cx = np.array([float(np.dot(p - chest[0], a)) for p in chest])
    return {"extension": [round(float(e), 2) for e in ext[: max(c0 + 12, 1)]], "hold_frames": hold,
            "hold_frames_min": sheet["hold_frames_min"], "strength": sheet["strength"], "recoil_required": sheet.get("recoil_required", False),
            "recoil_effector": osc(x), "recoil_chest": osc(cx)}


# ---------------------------------------------------------------- 5b. attack line centring (and 6. weapon arcs, prepared)
def attack_line(frames, c0, a, effector="middle_01_l"):
    """The strike's line against the character's facing (+Y): + = toward the character's right (X is its left)."""
    fist = series(frames, effector); chest = series(frames, "spine_05")
    side_r = np.array([-1.0, 0.0, 0.0])
    return {"attack_dir_vs_facing_deg": round(math.degrees(math.atan2(-a[0], a[1])), 1),
            "fist_lateral_from_chest_cm": round(float(np.dot(fist[c0] - chest[0], side_r)), 1)}

def weapon_arc(tips, attack_dir, kind="vertical"):
    """Weapon tip positions over the active frames -> arc plane and centring.
    vertical slash: the plane should be vertical (tilt 0) and contain the attack direction (deviation 0);
    the arc's centroid direction from the character should match the attack direction (centring)."""
    P = np.array(tips, dtype=float)
    c = P.mean(axis=0)
    _, _, vt = np.linalg.svd(P - c)
    n = vt[2] / np.linalg.norm(vt[2])                                     # plane normal
    up = np.array([0.0, 0.0, 1.0]); a = horiz(attack_dir)
    tilt = 90.0 - math.degrees(math.acos(min(1.0, abs(float(np.dot(n, up))))))   # 0 = vertical plane
    dev = math.degrees(math.asin(min(1.0, abs(float(np.dot(n, a))))))            # 0 = plane contains the attack direction
    mid = horiz(c)
    cen = math.degrees(math.atan2(float(np.cross(a, mid)[2]), float(np.dot(a, mid)))) if np.linalg.norm(mid) > 1e-6 else 0.0
    return {"kind": kind, "plane_tilt_from_vertical_deg": round(tilt, 1) if kind == "vertical" else None,
            "attack_dir_out_of_plane_deg": round(dev, 1), "arc_centre_vs_attack_deg": round(cen, 1)}

def _selftest_jitter():
    """A smooth arc passes; the same arc with a 1 cm alternating wobble on frames 10-16 is flagged as a zig-zag."""
    load_th()
    t = np.linspace(0, 1, 30)
    arc = np.stack([30 * np.sin(2 * t), 40 * t, 100 + 20 * t * t], axis=1)
    bad = arc.copy()
    for k in range(10, 17):
        bad[k, 0] += 2.5 if k % 2 else -2.5                                # straight paths: only big outliers count
    def frames_of(P):
        return {str(i): {b: list(P[i]) + [0.0, 0.0, 0.0] for b in TRAJ_BONES.values()} for i in range(len(P))}
    ok = not jitter(frames_of(arc), set())["hand_l"]["zigzag_frames"]
    caught = bool(jitter(frames_of(bad), set())["hand_l"]["zigzag_frames"])
    return ok and caught

def _selftest_weapon_arc():
    t = np.linspace(-1.2, 1.2, 9)
    arc = np.stack([np.zeros_like(t), 80 + 60 * np.sin(t), 120 + 60 * np.cos(t)], axis=1)   # vertical arc in the Y-Z plane
    r = weapon_arc(arc, [0, 1, 0])
    assert abs(r["plane_tilt_from_vertical_deg"]) < 0.5 and r["attack_dir_out_of_plane_deg"] < 0.5 and abs(r["arc_centre_vs_attack_deg"]) < 0.5, r
    rot = math.radians(20)
    R = np.array([[math.cos(rot), -math.sin(rot), 0], [math.sin(rot), math.cos(rot), 0], [0, 0, 1]])
    r2 = weapon_arc(arc @ R.T, [0, 1, 0])
    assert abs(r2["attack_dir_out_of_plane_deg"] - 20) < 0.5, r2
    return True


# ---------------------------------------------------------------- judge, report, plot
def judge(m, c0, c1, faf):
    load_th()
    fl = []
    w = m["weight"]
    # "forward tilt" in the owner's note is the NAME of Terry's move (Sakurai's example of impact spacing), not a spine lean:
    # the spine tilt is reported as info only; weight is judged by impact spacing, hold and the weight shift below.
    if str(m.get("move_sheet", {}).get("strike_limb", "")).startswith("leg"):
        # a kick on one foot keeps the whole-body centre of mass over the support foot (balance_check); the weight goes
        # INTO the kick through the hips: the pelvis drives forward from its furthest-back point to contact (owner: "push
        # into the kick", not fall), while the upper body counterbalances back against it
        drive = w.get("hip_drive_from_windup_cm")
        if drive is not None and drive < TH["com_shift_min_cm"]:
            fl.append({"check": "weight not moving into the strike", "hip_drive_from_windup_cm": drive})
    elif w["com_shift_cm"]["contact"] < TH["com_shift_min_cm"]:
        fl.append({"check": "weight not moving into the strike", "com_shift_cm": w["com_shift_cm"]["contact"]})
    for name, ph in m["overlap"].items():
        for phase, r in ph.items():
            if not r["peaks"]:
                continue
            if len(r["peaks"]) >= 3 and r["spread_frames"] < TH["overlap_min_spread_frames"]:
                fl.append({"check": "stiff chain (joints peak together)", "chain": name, "phase": phase, "spread": r["spread_frames"],
                           "peaks": {p["joint"]: p["peak_frame"] for p in r["peaks"]}})
            if len(r["peaks"]) == 2 and abs(r["lags"][0]) < TH["overlap_min_lag_frames"]:
                fl.append({"check": "no overlap", "chain": name, "phase": phase, "lag": r["lags"][0]})
    h = m["hold_recoil"]
    if h["hold_frames"] < h["hold_frames_min"]:
        fl.append({"check": "strike does not hold its pose", "hold_frames": h["hold_frames"], "min": h["hold_frames_min"]})
    if h.get("recoil_required") and h["hold_frames"] >= 3:
        if h["recoil_effector"]["reversals"] < 2 or h["recoil_effector"]["amplitude_cm"] < TH.get("recoil_min_cm", 1.0):
            fl.append({"check": "dead hold (no recoil)", **h["recoil_effector"]})
    # joints (owner): out-of-range values are flagged unless they exaggerate the motion inside the action frames
    for item in m["joints_summary"]:
        if item["verdict"] == "unnatural":
            fl.append({"check": f"unnatural joint ({item['joint']})", "frames": item["frames"][:40], "example": item["example"], "reason": item["reason"]})
    for sd, rows in m["wrist"].items():
        froz = [r["frame"] for r in rows if r["forearm_rot"] and r["forearm_rot"] > TH["frozen_forearm_fast_deg"] and r["hand_rot"] is not None
                and r["hand_rot"] < TH["frozen_ratio"] * r["forearm_rot"]]
        import wrist_check
        runs = wrist_check.frozen_frames(rows, TH["frozen_forearm_deg"], TH["frozen_ratio"], 2)
        if froz or runs:
            fl.append({"check": f"fist frozen in world while the forearm turns (hand_{sd})", "frames": sorted(set(froz) | {f for r_ in runs for f in r_}),
                       "detail": [r for r in rows if r["frame"] in set(froz)][:3]})
    sp = m["impact_spacing"]
    if sp["frames"] >= 2:
        if sp["share_last2"] < TH["impact_share_last2_min"]:
            fl.append({"check": "impact spread out (no snap to contact)", "share_last2": sp["share_last2"], "curve": sp["cumulative_pct"]})
        if not sp["peak_on_contact_frame"]:
            fl.append({"check": "speed peak before the contact frame", "curve": sp["cumulative_pct"]})
    for b, r in m["jitter"].items():
        bad = [a for a in r.get("arcs", []) if a["flags"]]
        if bad:
            spans = sorted({tuple(a["frames"]) for a in bad})
            kinds = sorted({k for a in bad for k in a["flags"]})
            fl.append({"check": "jitter on an arc", "bone": b, "windows": spans[:6], "kinds": kinds,
                       "worst_radial_cm": max(a["radial_max_cm"] for a in bad), "worst_offplane_cm": max(a["offplane_max_cm"] for a in bad)})
        if r["zigzag_frames"]:
            fl.append({"check": "trajectory zig-zag (jitter)", "bone": b, "frames": r["zigzag_frames"]})
        if r["wobble_frames"]:
            fl.append({"check": "trajectory wobble (back and forth)", "bone": b, "frames": r["wobble_frames"]})
    return fl

def report(m, take, flags, c0, c1, faf):
    w = m["weight"]; al = m["attack_line"]
    by = {}
    for f in flags:
        by.setdefault(f["check"], []).append(f)
    lines = [f"{take}: motion check, contact {c0}-{c1}, FAF {faf}",
             "VERDICT: " + ("PASS" if not flags else "FAIL (" + ", ".join(f"{k}: {len(v)}" for k, v in by.items()) + ")"),
             f"weight: spine tilt guard {w['spine_tilt_deg']['guard']} -> contact {w['spine_tilt_deg']['contact']} deg (delta {w['spine_tilt_deg']['delta']}); "
             f"COM +{w['com_shift_cm']['contact']} cm along the attack (over feet {w['com_over_feet']['guard']} -> {w['com_over_feet']['contact']}); "
             f"pelvis {w['pelvis_drop_cm']['contact']} cm at contact (min {w['pelvis_drop_cm']['min']}); anticipation: {w['anticipation_cm']['frames']} frame(s), "
             f"COM back {w['anticipation_cm']['com_back']} cm, fist back {w['anticipation_cm']['fist_back']} cm",
             f"impact spacing ({m['impact_spacing']['effector']}): {m['impact_spacing']['frames']} frame(s) from the anticipation key "
             f"(frame {m['impact_spacing']['anticipation_key']}) to contact, last 2 frames {round(100 * m['impact_spacing']['share_last2'])}% of the path, "
             f"cumulative % {m['impact_spacing']['cumulative_pct']}" + (" - a 1-frame startup has no build at all: nothing to ease in" if m['impact_spacing']['frames'] < 2 else ""),
             f"hold: {m['hold_recoil']['hold_frames']} frame(s) held near extension after contact "
             f"(min {m['hold_recoil']['hold_frames_min']} for a {m['hold_recoil']['strength']} move); recoil effector {m['hold_recoil']['recoil_effector']}, chest {m['hold_recoil']['recoil_chest']}",
             f"attack line: attack direction {al['attack_dir_vs_facing_deg']} deg from facing, fist lateral {al['fist_lateral_from_chest_cm']} cm from the chest line"]
    for name, ph in m["overlap"].items():
        for phase, r in ph.items():
            if r.get("note"):
                lines.append(f"overlap {name} / {phase}: {r['note']}")
            else:
                lines.append(f"overlap {name} / {phase}: " + ", ".join(f"{p['joint']} {p['peak_frame']}" for p in r["peaks"]) + f"  (spread {r['spread_frames']})")
    for k, lst in by.items():
        lines.append(f"FAIL {k}:")
        for f in lst[:8]:
            lines.append("   " + json.dumps(f))
    info = {}
    for b, r in m["jitter"].items():
        for f in r["flags"]:
            info.setdefault(f["kind"], []).append(f"{b}@{f['frame']}")
    for k, v in info.items():
        lines.append(f"info, {k} (beats, not flagged): {', '.join(v)}")
    for item in m["joints_summary"]:
        lines.append(f"joint {item['joint']}: {item['verdict']} on frames {item['frames'][:20]} (e.g. {item['example']}) - {item['reason']}")
    for sd, rows in m["wrist"].items():
        lines.append(f"wrist hand_{sd} flex/dev frames 0-8: " + " ".join(f"{r['frame']}:{r['flexion']:+.0f}/{r['deviation']:+.0f}" for r in rows[:9]))
    txt = "\n".join(lines)
    open(os.path.join(L.SHEETS, f"{take}_motion.txt"), "w").write(txt)
    return txt

def plot(m, take, c0, faf, flags):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    w = m["weight"]["curves"]; n = len(w["tilt"]); xs = np.arange(n)
    fig, axs = plt.subplots(3, 1, figsize=(14, 11), constrained_layout=True)
    ax = axs[0]
    ax.plot(xs, w["tilt"], "-o", ms=3, label="spine tilt toward attack (deg)")
    ax.plot(xs, w["com_along"], "-s", ms=3, label="centre of mass along attack (cm)")
    ax.plot(xs, w["pelvis_dz"], "-^", ms=3, label="pelvis height change (cm)")
    for x in (c0, faf):
        ax.axvline(x, color="k", lw=0.8)
    ax.set_title(f"{take}: weight and force", loc="left"); ax.legend(fontsize=8); ax.set_xticks(xs)
    ax = axs[1]
    rows = []
    for name, ph in m["overlap"].items():
        for phase, r in ph.items():
            if r["peaks"]:
                rows.append((f"{name} / {phase}", r["peaks"]))
    for i, (lab, peaks) in enumerate(rows):
        ts = [p["peak_frame"] for p in peaks]
        ax.plot(ts, [i] * len(ts), "-", color="0.6")
        for p in peaks:
            ax.plot(p["peak_frame"], i, "o")
            ax.annotate(p["joint"], (p["peak_frame"], i), textcoords="offset points", xytext=(3, 5), fontsize=7)
    ax.set_yticks(range(len(rows)), [r[0] for r in rows], fontsize=8)
    ax.set_title("speed peaks per chain (overlap: joints should arrive one after another, not together)", loc="left")
    ax.set_xticks(xs)
    ax = axs[2]
    for i, (b, r) in enumerate(m["jitter"].items()):
        p = np.array(r["path"]); sp = np.r_[np.linalg.norm(np.diff(p, axis=0), axis=1), np.nan]
        ax.plot(xs, sp, label=b)
    for f in flags:
        if f["check"].startswith("trajectory") and "frame" in f:
            ax.axvline(f["frame"], color="r", lw=0.6, ls=":")
    ax.set_title("bone speed (cm/frame); red dotted = flagged trajectory frame", loc="left"); ax.legend(fontsize=7, ncol=4); ax.set_xticks(xs)
    out = os.path.join(L.SHEETS, f"{take}_motion.png")
    fig.savefig(out, dpi=80); plt.close(fig)
    return out

def move_sheet(take, contact=None, faf=None):
    """Per-move parameters (move_sheets.json), with the move class defaults; CLI values override."""
    p = os.path.join(HERE, "move_sheets.json")
    allsheets = json.load(open(p)) if os.path.exists(p) else {}
    sh = dict(allsheets.get(take, {}))
    cls = allsheets.get("_classes", {}).get(sh.get("move", "jab"), {"strength": "light", "hold_frames_min": 1})
    for k, v in cls.items():
        sh.setdefault(k, v)
    if contact:
        sh["contact"] = list(contact)
    if faf is not None:
        sh["faf"] = faf
    sh.setdefault("contact", [1, 1]); sh.setdefault("faf", 17); sh.setdefault("hold_window", sh["contact"])
    return sh

def run(take, contact=None, faf=None):
    sheet = move_sheet(take, contact, faf)
    (c0, c1), faf = sheet["contact"], sheet["faf"]
    d = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))
    frames = d["frames"]
    load_th()
    wres, a = weight(frames, c0, c1, faf, sheet.get("effector", "middle_01_l"))
    pel = series(frames, "pelvis")
    pz = [float(np.dot(pel[f] - pel[0], a)) for f in range(len(frames))]
    wres["hip_drive_from_windup_cm"] = round(pz[c0] - min(pz[: c0 + 1]), 1)
    hr = hold_recoil(frames, c0, a, sheet)
    hw = sheet["hold_window"]
    # beats the jitter detector leaves alone: contact, the declared hold window plus the measured hold (recoil lives there), FAF
    allowed = set(range(c0, c1 + 2)) | set(range(hw[0], hw[1] + 2)) | set(range(c0, c0 + hr["hold_frames"] + 2)) | {faf, faf + 1}
    # the anticipation reversal is a beat too (body loads back, then drives into the strike): the frames around the
    # anticipation key, when the move has a wind-up (Ganondorf ftilt: 6 frames; the jab's 1-frame startup has none)
    pos_e = series(frames, sheet.get("effector", "middle_01_l"))
    ak = min(range(0, c0 + 1), key=lambda f: float(np.dot(pos_e[f], a))) if c0 > 1 else 0
    if 0 < ak < c0 - 1:
        allowed |= set(range(max(ak - 2, 0), min(ak + 3, c0)))
    load_th()
    import wrist_check, joints
    jres = joints.check(frames, sheet, a, TH)
    import arm_check
    ref_p = os.path.join(L.RAW, "blockout_legbones.json")
    ref = json.load(open(ref_p))["frames"] if os.path.exists(ref_p) else frames
    arm_rows = arm_check.arm_rows(frames, ref)
    jres += arm_check.flags(arm_rows, sheet, TH, frames)
    m = {"anim": d["anim"], "wrist": wrist_check.wrist(frames), "joints_summary": joints.summary(jres), "arms": arm_rows, "selftest_jitter": _selftest_jitter(), "selftest_arcs": _selftest_arcs(), "weight": wres, "overlap": overlap(frames, c0, c1, faf), "jitter": jitter(frames, allowed),
         "move_sheet": sheet, "hold_recoil": hr,
         "attack_line": attack_line(frames, c0, a, sheet.get("effector", "middle_01_l")), "impact_spacing": impact_spacing(frames, c0, a, sheet.get("effector", "middle_01_l")),
         "selftest_spacing": _selftest_spacing(), "weapon_arc": "inactive: no weapon bone yet (weapon_arc() ready, self-test passed)"
         if _selftest_weapon_arc() else "self-test failed"}
    flags = judge(m, c0, c1, faf)
    import balance_check
    pos_e = series(frames, sheet.get("effector", "middle_01_l"))
    ak = min(range(0, c0 + 1), key=lambda f: float(np.dot(pos_e[f], a))) if c0 > 1 else 0
    brows, cb, bflags = balance_check.check(frames, sheet, TH, com, a, ak)
    m["balance"] = {"rows": brows, "counterbalance_cm": cb}
    flags += bflags
    import pose_rules
    pflags, pinfo = pose_rules.check(frames, sheet, TH, a, brows, ak)
    m["pose_rules"] = pinfo
    flags += pflags
    # recovery spacing: fast out of the hold, slow in to the guard/idle (spacing_rules.py, shared with preflight.py)
    import spacing_rules
    rec, rflags = spacing_rules.recovery_spacing(pos_e, sheet["hold_window"][1], faf)
    m["recovery_spacing"] = rec
    flags += rflags
    m["flags"] = flags
    json.dump(m, open(os.path.join(L.SHEETS, f"{take}_motion.json"), "w"), indent=1)
    png = plot(m, take, c0, faf, flags)
    return m, flags, report(m, take, flags, c0, c1, faf), png

if __name__ == "__main__":
    take = sys.argv[1]
    contact = tuple(int(x) for x in sys.argv[2].split("-")) if len(sys.argv) > 2 else None
    if contact and len(contact) == 1:
        contact = (contact[0], contact[0])
    faf = int(sys.argv[3]) if len(sys.argv) > 3 else None
    m, flags, txt, png = run(take, contact, faf)
    print(txt); print(png)
