"""Twist chain gate: the complementary rotations (owner, 01/11: "torso rotation etc., where do we stand?") on the model's rig
points, against the reference's key poses. Ganondorf v3 passed every other gate with the shoulders turning BEFORE the
pelvis in the strike (yaw-speed peaks: shoulders 4, pelvis 8) and half the reference's hip turn.

    python twist_chain.py <take> [--targets=<file>] [--quiet]        (after motion_curves.py: it reads the phase frames)

Signals per frame, yaw in the horizontal plane (deg): pelvis = hip line (thigh points), chest = shoulder line (upper-arm
points), head = its forward point; limb = the strike effector's speed (move sheet "effector").
1. ORDER: in the strike window (2 frames before the anticipation pose .. last contact frame; the wind-up turns the other
   way and is not judged) the yaw-speed peaks run
   pelvis -> chest -> limb, each at least twist_lag_min_frames after the one before; in the recovery (after the contact to
   the FAF) pelvis -> chest. Sub-frame peaks (parabola). The head is reported, not judged (it stays on the target).
2. AMPLITUDE vs the reference: side-view apparent separation of the hips and of the shoulders (body lengths: torso +
   support thigh + shin, measured in each pose, as motion_ref.py), at the five phase frames motion_curves.py matched.
   Fails when ours/ref < twist_amp_ratio_min for: the hips' range over the poses, the shoulders' range, and the shoulders'
   closing from the first to the active pose; or when the shoulders close the other way.
The 2D estimate is rough (pose-estimator hip keypoints sit wider than the hip joints; Ganondorf is not the mannequin), so
the gate compares changes and ranges, not absolute widths, and the ratio is the one motion_curves.py uses (0.6).
Output: sheets/<take>_twist.txt/.json; first line PASS/FAIL; exit 1 on a failure."""
import json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TH = json.load(open(os.path.join(HERE, "motion_thresholds.json")))
LAG = TH.get("twist_lag_min_frames", 0.5)
RATIO = TH.get("twist_amp_ratio_min", 0.6)
PHASES = ("first", "anticipation", "active", "recovery", "last")


def peak(v, lo, hi):
    """Sub-frame frame of max |v| over frames lo..hi (v[i] = speed between frames i and i+1, centred at i + 0.5)."""
    lo, hi = max(lo, 0), min(hi, len(v) - 1)
    a = np.abs(v[lo:hi + 1]); i = int(np.argmax(a)); k = lo + i
    if 0 < i < len(a) - 1:
        y0, y1, y2 = a[i - 1], a[i], a[i + 1]; den = y0 - 2 * y1 + y2
        k = k + (0.5 * (y0 - y2) / den if abs(den) > 1e-9 else 0.0)
    return float(k + 0.5), float(np.max(a))


def main():
    take = sys.argv[1]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[2:] if a.startswith("--"))
    T = json.load(open(os.path.join(HERE, opt.get("targets") or f"{take}_targets_raw.json")))
    P = {n: np.array(v, float) for n, v in T["targets"].items()}
    sheet = json.load(open(os.path.join(HERE, "move_sheets.json")))[take]
    ref_dir = os.path.join(HERE, sheet["reference"])
    K = json.load(open(os.path.join(ref_dir, "keyposes.json")))
    sup = K.get("support", "r")
    c0, c1 = sheet["contact"]; faf = sheet["faf"]
    kc = np.array(T.get("move", {}).get("kick_dir_cascadeur") or [0, 0, 1.0], float); kf = np.array([kc[0], 0.0, kc[2]]); kf /= np.linalg.norm(kf)
    up = np.array([0.0, 1.0, 0.0]); lat = np.cross(up, kf)
    n = len(P["pelvis_MainPoint"])

    def yaw(v):
        return np.unwrap(np.arctan2(v @ kf, v @ lat)) * 180 / math.pi
    Y = {"pelvis": yaw(P["thigh_MainPoint_l"] - P["thigh_MainPoint_r"]),
         "chest": yaw(P["upperarm_MainPoint_l"] - P["upperarm_MainPoint_r"]),
         "head": yaw(P["head_AdditionalPoint"] - P["head_MainPoint"])}
    eff = {"ball_l": "ball_MainPoint_l", "ball_r": "ball_MainPoint_r", "hand_l": "hand_MainPoint_l", "hand_r": "hand_MainPoint_r",
           "foot_l": "foot_MainPoint_l", "foot_r": "foot_MainPoint_r"}.get(sheet.get("effector"), "hand_MainPoint_l")
    V = {k: np.diff(v) for k, v in Y.items()}
    V["limb"] = np.linalg.norm(np.diff(P[eff], axis=0), axis=1)

    # phase frames from the curves gate (its DTW match), else first / contact / FAF
    ph = {}
    cj = os.path.join(HERE, "sheets", f"{take}_model_curves.json")
    if os.path.exists(cj):
        ph = {x["phase"]: x["frame"] for x in json.load(open(cj)).get("reference", {}).get("per_pose", []) if x.get("phase")}
    ph = {p: ph.get(p, d) for p, d in zip(PHASES, (0, max(0, c0 - 3), c0, (c1 + faf) // 2, faf))}

    fails, out = [], {"phases": ph, "order": {}, "amplitude": {}}
    # 1. order
    # the strike starts at the anticipation pose (the wind-up extreme); 2 frames earlier so a peak right at it is not an edge
    win = {"strike": (min(ph["anticipation"], c0 - 1) - 2, c1), "recovery": (c1 + 1, faf - 1)}
    for w, (lo, hi) in win.items():
        pk = {k: peak(V[k], lo, hi) for k in ("pelvis", "chest", "head", "limb")}
        out["order"][w] = {k: round(f, 2) for k, (f, _) in pk.items()}
        chain = ["pelvis", "chest", "limb"] if w == "strike" else ["pelvis", "chest"]
        for a, b in zip(chain, chain[1:]):
            if pk[b][0] - pk[a][0] < LAG:
                fails.append({"check": f"{w}: {b} peaks {pk[b][0] - pk[a][0]:+.1f} frames after {a} (need >= {LAG})",
                              "window": [lo, hi], a: round(pk[a][0], 2), b: round(pk[b][0], 2)})
    out["yaw_range_deg"] = {k: round(float(v.max() - v.min()), 1) for k, v in Y.items()}
    out["twist_chest_vs_pelvis_deg"] = [round(float((Y["chest"] - Y["pelvis"]).min()), 1), round(float((Y["chest"] - Y["pelvis"]).max()), 1)]

    # 2. amplitude vs the reference (side view, body lengths)
    def side(p):
        return np.array([p @ kf, p @ up])

    def ours(f):
        mh = 0.5 * (P["thigh_MainPoint_l"][f] + P["thigh_MainPoint_r"][f]); ms = 0.5 * (P["upperarm_MainPoint_l"][f] + P["upperarm_MainPoint_r"][f])
        bl = (np.linalg.norm(side(ms) - side(mh)) + np.linalg.norm(side(P[f"calf_MainPoint_{sup}"][f]) - side(P[f"thigh_MainPoint_{sup}"][f]))
              + np.linalg.norm(side(P[f"foot_MainPoint_{sup}"][f]) - side(P[f"calf_MainPoint_{sup}"][f])))
        return ((P["thigh_MainPoint_l"][f] - P["thigh_MainPoint_r"][f]) @ kf / bl,
                (P["upperarm_MainPoint_l"][f] - P["upperarm_MainPoint_r"][f]) @ kf / bl)
    R = {}
    for p in K["poses"]:
        j = p["joints_hip"]
        R[p["phase"]] = (j["hip_l"][0] - j["hip_r"][0], j["shoulder_l"][0] - j["shoulder_r"][0])
    O = {p: ours(ph[p]) for p in PHASES if p in R}
    for i, part in ((0, "hips"), (1, "shoulders")):
        r = [R[p][i] for p in O]; o = [O[p][i] for p in O]
        rr, ro = max(r) - min(r), max(o) - min(o)
        out["amplitude"][part] = {"ref": [round(x, 3) for x in r], "ours": [round(x, 3) for x in o],
                                  "range_ref": round(rr, 3), "range_ours": round(ro, 3), "ratio": round(ro / rr, 2) if rr > 1e-6 else None}
        if rr > 1e-6 and ro / rr < RATIO:
            fails.append({"check": f"{part} turn range {ro / rr:.2f}x the reference (min {RATIO})", "ref": round(rr, 3), "ours": round(ro, 3)})
    if "first" in O and "active" in O:
        cr = (R["first"][1] - R["active"][1]) / R["first"][1]; co = (O["first"][1] - O["active"][1]) / O["first"][1]
        out["amplitude"]["shoulders_closing_first_to_active"] = {"ref": round(cr, 3), "ours": round(co, 3)}
        if cr * co < 0:
            fails.append({"check": f"shoulders turn the other way from the first to the active pose (ours {co:+.0%}, ref {cr:+.0%})"})
        elif abs(cr) > 1e-6 and abs(co / cr) < RATIO:
            fails.append({"check": f"shoulders close {co:.0%} from the first to the active pose, ref {cr:.0%} ({co / cr:.2f}x, min {RATIO})"})
    out["fails"] = fails; out["fail"] = bool(fails)

    L = [f"{'FAIL' if fails else 'PASS'} twist chain {take}: {len(fails)} failure(s)  (phases {ph})"]
    for f in fails:
        L.append("  - " + f["check"])
    for w, pk in out["order"].items():
        L.append(f"  {w} yaw-speed peaks (frame): " + ", ".join(f"{k} {v}" for k, v in pk.items()))
    L.append(f"  yaw range (deg): {out['yaw_range_deg']}; chest vs pelvis {out['twist_chest_vs_pelvis_deg']}")
    for part, a in out["amplitude"].items():
        L.append(f"  {part}: " + json.dumps(a))
    txt = "\n".join(L)
    os.makedirs(os.path.join(HERE, "sheets"), exist_ok=True)
    open(os.path.join(HERE, "sheets", f"{take}_twist.txt"), "w").write(txt + "\n")
    json.dump(out, open(os.path.join(HERE, "sheets", f"{take}_twist.json"), "w"), indent=1)
    print(L[0] if "quiet" in opt else txt)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
