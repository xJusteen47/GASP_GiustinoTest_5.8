"""Posing rules from the coordinator's checklist (owner, 31/10: "did you research general posing principles?"), the ones a
script can measure. Balance and counterbalance live in balance_check.py; joint ranges in joints.py. Here:
  liftoff     the centre of mass is over the future support foot BEFORE the other foot leaves the ground (first
              single-support frame inside that foot's base, 2 cm tolerance)
  support     on one foot, the support knee bends 10-45 deg (carries the body, not a near-kneel, not locked)
  chain       kicks: velocity peaks run pelvis -> thigh -> shin -> foot over the drive (contact - 2 .. contact + 1; no
              joint peaks before the one that drives it, 0.25 frame tolerance)
  twinning    at the contact and mid-hold poses, left and right limbs are not mirrored (elbow, knee, upper-arm angles all
              within 6 deg of each other)
  head        through the strike and hold, the face points within 35 deg of the attack direction (yaw) and 30 deg of level
  slide       a planted foot does not slide: its ball stays within 0.5 cm horizontally while planted (pivoting on the ball
              is allowed; the ball must also stay within 1 cm of its frame-0 height, so a lift-off is not a slide)
UE component space: X left, Y forward, Z up."""
import math
import numpy as np
import legibility as L

def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def ang(a, b):
    return math.degrees(math.acos(float(np.clip(np.dot(unit(a), unit(b)), -1, 1))))

def flex(fr, a, b, c):
    pa, pb, pc = (L.bone_tf(fr, x)[0] for x in (a, b, c))
    return 180.0 - ang(pa - pb, pc - pb)

def floor_ref(frames, s):
    """Ankle and ball heights of foot s on the floor: frame 0's, unless that foot is already in the air at frame 0 (a move
    whose first frame has the knee up, Ganondorf ftilt v3): then the other foot's frame-0 heights (the same floor)."""
    f0 = frames["0"]; o = "l" if s == "r" else "r"
    an, ba = L.bone_tf(f0, f"foot_{s}")[0][2], L.bone_tf(f0, f"ball_{s}")[0][2]
    oba = L.bone_tf(f0, f"ball_{o}")[0][2]
    if ba > oba + 2.5:
        return L.bone_tf(f0, f"foot_{o}")[0][2], oba
    return an, ba

def planted(frames, f, s, tol=2.5):
    fr = frames[str(f)]; an0, ba0 = floor_ref(frames, s)
    return (L.bone_tf(fr, f"foot_{s}")[0][2] <= an0 + tol and
            L.bone_tf(fr, f"ball_{s}")[0][2] <= ba0 + tol)

def speed_peak(frames, bone, a, b):
    P = np.array([L.bone_tf(frames[str(f)], bone)[0] for f in range(a, b + 1)])
    v = np.linalg.norm(np.diff(P, axis=0), axis=1)
    i = int(np.argmax(v))
    # sub-frame peak by a parabola through the neighbours
    if 0 < i < len(v) - 1:
        y0, y1, y2 = v[i - 1], v[i], v[i + 1]; den = (y0 - 2 * y1 + y2)
        off = 0.5 * (y0 - y2) / den if abs(den) > 1e-9 else 0.0
    else:
        off = 0.0
    return a + i + 0.5 + off

def check(frames, sheet, TH, attack_dir, balance_rows, anticipation_key):
    n = len(frames); c0, c1 = sheet["contact"]; hw = sheet.get("hold_window", [c0, c1])
    flags = []; info = {}
    a = unit(np.array([attack_dir[0], attack_dir[1], 0.0]))
    # liftoff
    br = {r["frame"]: r for r in balance_rows}
    first_single = next((r for r in balance_rows if len(r["planted"]) == 1), None)
    if first_single and first_single["outside_cm"] is not None:
        info["liftoff"] = {"frame": first_single["frame"], "outside_cm": first_single["outside_cm"]}
        if first_single["outside_cm"] > TH.get("balance_outside_max_cm", 2.0) and first_single["frame"] not in set(sheet.get("balance_exempt_frames", [])):
            flags.append({"check": "weight not shifted before lift-off", "frame": first_single["frame"], "outside_cm": first_single["outside_cm"]})
    # support knee range on one foot
    lo, hi = TH.get("support_knee_min_deg", 10), TH.get("joint_support_knee_max_flex_deg", 45)
    bad = []
    for r in balance_rows:
        if len(r["planted"]) == 1 and not sheet.get("crouch"):
            s = r["planted"][0]; k = flex(frames[str(r["frame"])], f"thigh_{s}", f"calf_{s}", f"foot_{s}")
            if not lo <= k <= hi:
                bad.append((r["frame"], round(k)))
    if bad:
        flags.append({"check": "support knee out of range on one foot", "range_deg": [lo, hi], "frames": [f for f, _ in bad], "example": bad[len(bad) // 2]})
    # kinetic chain (kicks)
    if str(sheet.get("strike_limb", "")).startswith("leg"):
        side = sheet["strike_limb"][-1]
        lo_f, hi_f = max(c0 - 2, 0), min(c0 + 1, n - 1)          # the drive into the kick (pre-snap -> contact)
        chain = ["pelvis", f"thigh_{side}", f"calf_{side}", f"foot_{side}"]
        peaks = [round(speed_peak(frames, b, lo_f, hi_f), 2) for b in chain]
        info["kick_chain_peaks"] = dict(zip(chain, peaks))
        if any(peaks[i + 1] < peaks[i] - 0.25 for i in range(len(peaks) - 1)):
            flags.append({"check": "kinetic chain out of order (pelvis -> thigh -> shin -> foot)", "peaks": dict(zip(chain, peaks))})
    # twinning at contact and mid-hold
    for f in sorted({c0, (hw[0] + hw[1]) // 2}):
        fr = frames[str(f)]
        pairs = [(flex(fr, "upperarm_l", "lowerarm_l", "hand_l"), flex(fr, "upperarm_r", "lowerarm_r", "hand_r")),
                 (flex(fr, "thigh_l", "calf_l", "foot_l"), flex(fr, "thigh_r", "calf_r", "foot_r")),
                 (ang(L.bone_tf(fr, "lowerarm_l")[0] - L.bone_tf(fr, "upperarm_l")[0], [0, 0, -1]),
                  ang(L.bone_tf(fr, "lowerarm_r")[0] - L.bone_tf(fr, "upperarm_r")[0], [0, 0, -1]))]
        if all(abs(x - y) < TH.get("twinning_max_deg", 6) for x, y in pairs):
            flags.append({"check": "twinning (left and right limbs mirrored)", "frame": f, "pairs_deg": [[round(x), round(y)] for x, y in pairs]})
    # head toward the target
    hb = []
    for f in range(c0, hw[1] + 1):
        fr = frames[str(f)]
        h, q = L.bone_tf(fr, "head")
        # head forward = the direction from the head bone toward the nose: take the head's local +Y... measured instead
        # from the neck->head line's perpendicular: use the vector head - neck_01 rotated; robust proxy: chest-independent
        # facing from the head's rotation applied to the guard's facing (head forward at frame 0 = attack direction)
        q0 = L.bone_tf(frames["0"], "head")[1]
        f0 = a  # frame 0 faces the attack direction (idle/guard faces forward)
        rel = L.qmul(q, np.array([-q0[0], -q0[1], -q0[2], q0[3]]))
        fwd = L.qrot(rel, np.array([f0[0], f0[1], 0.0]))
        yaw = math.degrees(math.atan2(fwd[0] * a[1] - fwd[1] * a[0], float(np.dot(fwd[:2], a[:2]))))
        pitch = math.degrees(math.asin(float(np.clip(fwd[2] / max(np.linalg.norm(fwd), 1e-9), -1, 1))))
        if abs(yaw) > TH.get("head_yaw_max_deg", 35) or abs(pitch) > TH.get("head_pitch_max_deg", 30):
            hb.append((f, round(yaw), round(pitch)))
    if hb:
        flags.append({"check": "head not toward the target", "frames": [x[0] for x in hb], "example_yaw_pitch": hb[0][1:]})
    # planted foot slide (ball as pivot). A foot is planted here only while its ball stays within 1 cm of its frame-0
    # height: the 2.5 cm balance tolerance let v1's kicking foot count as "sliding" on its lift-off frame (1.6 cm up,
    # 1 cm forward); animationsight's speed gate caught that (ganon_ftilt_log.md, animationsight comparison).
    sl = []
    for s in "lr":
        run = []
        z0 = floor_ref(frames, s)[1]
        for f in range(n):
            if planted(frames, f, s) and L.bone_tf(frames[str(f)], f"ball_{s}")[0][2] <= z0 + 1.0:
                run.append(L.bone_tf(frames[str(f)], f"ball_{s}")[0][:2])
            else:
                if len(run) > 1:
                    d = max(float(np.linalg.norm(p - run[0])) for p in run)
                    if d > TH.get("foot_slide_max_cm", 0.5):
                        sl.append((s, round(d, 2)))
                run = []
        if len(run) > 1:
            d = max(float(np.linalg.norm(p - run[0])) for p in run)
            if d > TH.get("foot_slide_max_cm", 0.5):
                sl.append((s, round(d, 2)))
    if sl:
        flags.append({"check": "planted foot slides", "feet_cm": sl})
    return flags, info
