"""Joint naturalness (owner: "an odd joint pose is fine when it exaggerates the movement; the bent-in wrists of v8 are a defect
because they exaggerate nothing"). Per frame, from the UE bone samples (component space: X left, Y forward, Z up):
  wrist  flexion / deviation of the fist against the forearm (wrist_check.py)
  knee   bend direction vs the foot direction (knee over the toes; inward collapse), hyperextension
  ankle  angle between shin (knee -> ankle) and foot (ankle -> ball): dorsi / plantar flexion
  foot   roll about its long axis against the guard (inversion / eversion), ball of the foot through the floor
A value outside its natural range is "exaggeration, accepted" when it happens inside the move's action window (contact to
contact + action_window_frames) AND points along the motion (wrist cocked toward the attack, foot pivoting with the hips, heel
lifted to push into the strike); otherwise it is "unnatural". Thresholds: motion_thresholds.json (joint_*).
On one foot (the other foot's toes off the floor) the support knee bends at most joint_support_knee_max_flex_deg
unless the move sheet declares crouch (Ganondorf v1's 89-99 deg near-kneel). A kicking foot in the air is not judged
for roll or floor, and knee-over-toes only where both directions are horizontal."""
import math
import numpy as np
import legibility as L
import wrist_check

def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def hz(v):
    v = np.array(v, dtype=float); v[2] = 0
    return unit(v)

def signed_angle_z(a, b):
    """Signed angle a -> b about +Z, degrees (+ = toward +X... counterclockwise seen from above)."""
    return math.degrees(math.atan2(a[0] * b[1] - a[1] * b[0], a[0] * b[0] + a[1] * b[1]))

def check(frames, sheet, attack_dir, TH):
    n = len(frames)
    c0, c1 = sheet["contact"]
    win = set(range(c0, c1 + 1 + TH.get("joint_action_window_frames", 3)))
    res = []
    W = wrist_check.wrist(frames)
    pel = [L.bone_tf(frames[str(f)], "pelvis")[1] for f in range(n)]
    def hip_yaw(f):
        a = L.bone_tf(frames[str(f)], "thigh_l")[0] - L.bone_tf(frames[str(f)], "thigh_r")[0]
        return math.degrees(math.atan2(a[1], a[0]))
    a_dir = unit(np.array([attack_dir[0], attack_dir[1], 0.0]))
    # ---- wrists
    for s, rows in W.items():
        for r in rows:
            f = r["frame"]
            bad = not (TH["wrist_flex_min_deg"] <= r["flexion"] <= TH["wrist_flex_max_deg"]) or abs(r["deviation"]) > TH["wrist_dev_max_deg"]
            if not bad:
                continue
            fr = frames[str(f)]
            wr = L.bone_tf(fr, f"hand_{s}")[0]; el = L.bone_tf(fr, f"lowerarm_{s}")[0]; kn = L.bone_tf(fr, f"middle_01_{s}")[0]
            # an exaggeration bends the fist along where the hand is going at that moment (cocked along the snap at contact),
            # not toward the target while the arm pulls back (that is the frozen fist of v8 frame 3)
            fp = frames[str(max(f - 1, 0))]; fn = frames[str(min(f + 1, n - 1))]
            vel = L.bone_tf(fn, f"hand_{s}")[0] - L.bone_tf(fp, f"hand_{s}")[0]
            toward = np.linalg.norm(vel) > 1.0 and float(np.dot(unit(kn - wr) - unit(wr - el), unit(vel))) > 0.05
            if f in win and toward and s == "l":
                res.append({"joint": f"wrist_{s}", "frame": f, "value": f"flex {r['flexion']:+.0f} / dev {r['deviation']:+.0f}", "verdict": "exaggeration, accepted",
                            "reason": "inside the strike's action frames and bent along the hand's motion"})
            else:
                res.append({"joint": f"wrist_{s}", "frame": f, "value": f"flex {r['flexion']:+.0f} / dev {r['deviation']:+.0f}", "verdict": "unnatural",
                            "reason": ("outside the action frames" if f not in win else "bent against the hand's motion") + "; the fist should stay in line with the forearm"})
    # ---- legs
    y0 = hip_yaw(0)
    for s in "lr":
        q0 = L.bone_tf(frames["0"], f"foot_{s}")[1]
        ball0 = L.bone_tf(frames["0"], f"ball_{s}")[0][2]
        an0 = L.bone_tf(frames["0"], f"foot_{s}")[0][2]
        # a foot already in the air at frame 0 (a move whose first frame has the knee up, Ganondorf ftilt v3): the floor is
        # the other foot's ball height, and its guard orientation is its first frame back on that floor
        o = "l" if s == "r" else "r"
        floor = L.bone_tf(frames["0"], f"ball_{o}")[0][2]
        if ball0 > floor + 2.5:
            ball0 = floor
            first = next((g for g in range(n) if L.bone_tf(frames[str(g)], f"ball_{s}")[0][2] <= floor + 1.0), None)
            if first is not None:
                q0 = L.bone_tf(frames[str(first)], f"foot_{s}")[1]
                an0 = L.bone_tf(frames[str(first)], f"foot_{s}")[0][2]
        for f in range(n):
            fr = frames[str(f)]
            hip, kn, an, ball = (L.bone_tf(fr, b)[0] for b in (f"thigh_{s}", f"calf_{s}", f"foot_{s}", f"ball_{s}"))
            qf = L.bone_tf(fr, f"foot_{s}")[1]
            line = an - hip; t = float(np.dot(kn - hip, line) / max(np.dot(line, line), 1e-9))
            bend = kn - (hip + t * line)
            knee_flex = 180.0 - math.degrees(math.acos(np.clip(np.dot(unit(hip - kn), unit(an - kn)), -1, 1)))
            foot_dir = hz(ball - an)
            kicking = sheet.get("strike_limb") == f"leg_{s}"
            airborne = kicking and (ball[2] > ball0 + 3.0 or an[2] > an0 + 3.0)
            # in the air a near-vertical foot or a near-straight leg has no meaningful horizontal direction
            flat_ok = np.linalg.norm((ball - an)[:2]) > 0.5 * np.linalg.norm(ball - an) and np.linalg.norm(bend[:2]) > 0.5 * np.linalg.norm(bend)
            knee_toe = signed_angle_z(foot_dir, hz(bend)) if np.linalg.norm(bend[:2]) > 1.0 and (not airborne or flat_ok) else 0.0
            ankle = math.degrees(math.acos(np.clip(np.dot(unit(kn - an), unit(ball - an)), -1, 1)))
            roll = L.qangle(q0, qf)          # whole foot rotation against the guard (feet planted: roll/pivot/lift)
            lifted = ball - ball0 if False else None
            pivot_with_hips = (hip_yaw(f) - y0) * (signed_angle_z(hz(L.bone_tf(frames["0"], f"ball_{s}")[0] - L.bone_tf(frames["0"], f"foot_{s}")[0]), foot_dir)) > 0
            issues = []
            if abs(knee_toe) > TH["joint_knee_toe_max_deg"]:
                issues.append(("knee", f"knee {knee_toe:+.0f} deg off the toes", "knee not over the toes (collapsing in or out)"))
            if knee_flex < TH["joint_knee_min_flex_deg"] and float(np.dot(bend, foot_dir)) < 0:
                issues.append(("knee", f"hyperextended ({knee_flex:.0f} deg flex)", "knee bending backward"))
            other = "l" if s == "r" else "r"
            o_an, o_ba = L.bone_tf(fr, f"foot_{other}")[0], L.bone_tf(fr, f"ball_{other}")[0]
            one_foot = o_ba[2] > L.bone_tf(frames["0"], f"ball_{other}")[0][2] + 2.5 and not airborne     # other foot's toes off the floor
            if one_foot and not sheet.get("crouch") and knee_flex > TH.get("joint_support_knee_max_flex_deg", 60):
                issues.append(("knee", f"support knee bent {knee_flex:.0f} deg on one foot", "support leg near-kneeling on one foot (it should carry the body, knee over the toes, about 30-45 deg)"))
            if not (TH["joint_ankle_min_deg"] <= ankle <= TH["joint_ankle_max_deg"]):
                issues.append(("ankle", f"shin-foot {ankle:.0f} deg", "ankle beyond its natural dorsi/plantar range"))
            if roll > TH["joint_foot_roll_max_deg"] and not airborne:      # a kicking foot in the air turns with the kick
                issues.append(("foot", f"foot turned {roll:.0f} deg from the guard", "foot rolled or pivoted"))
            if ball[2] < ball0 - TH["joint_ball_floor_tol_cm"] and not airborne:
                issues.append(("toes", f"ball {ball[2] - ball0:+.1f} cm", "ball of the foot through the floor"))
            for j, val, why in issues:
                # a planted foot pivoting with the hips is the mechanics of the move at any frame (owner: support foot planted,
                # pivoting with the hip); an ankle beyond range is accepted only inside the action frames and with the hips
                ok = (j == "foot" and pivot_with_hips) or (f in win and j == "ankle" and (pivot_with_hips or airborne))
                res.append({"joint": f"{j}_{s}", "frame": f, "value": val, "verdict": "exaggeration, accepted" if ok else "unnatural",
                            "reason": ("foot pivoting with the hip rotation" if j == "foot" and ok else "inside the action frames, along the kick/hips" if ok else why)})
    return res

def summary(res):
    out = {}
    for r in res:
        k = (r["joint"], r["verdict"])
        out.setdefault(k, {"frames": [], "example": r})["frames"].append(r["frame"])
    return [{"joint": j, "verdict": v, "frames": d["frames"], "example": d["example"]["value"], "reason": d["example"]["reason"]} for (j, v), d in out.items()]
