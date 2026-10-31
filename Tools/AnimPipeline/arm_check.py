"""Arm naturalness (owner, v9: "the left arm is bent with the elbow pointing outward ... also a geometry/rig defect; how did you
miss the front arm bent the wrong way?"). Per frame and arm, from the UE bone samples (component space X left, Y fwd, Z up):
  pole       elbow direction in the TORSO frame (forward, outward = away from the body on that side, up): where the elbow points
  elevation  upper arm angle from hanging straight down (0 = down, 90 = horizontal)
  flex       elbow flexion (0 = straight)
  reverse    the elbow hinge bending the other way: the forearm's swing axis against the upper arm flips sign vs the
             reference guard (blockout frame 0)
  twist      forearm roll about its own axis vs the upper arm, and hand roll vs the forearm, both relative to the reference
             guard; plus the twist bones' share. Large roll or a sudden roll jump = candy-wrapper risk on the skin.
Usage: python arm_check.py <take> [<take> ...]  (reference = blockout)"""
import sys, os, json, math
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility as L

def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def qconj(q):
    return np.array([-q[0], -q[1], -q[2], q[3]])

def qrel(a, b):
    return L.qmul(qconj(a), b)          # b expressed in a's frame

def twist_about(q, axis):
    """Signed twist angle (deg) of rotation q about a unit axis (swing-twist decomposition)."""
    v = q[:3]; w = q[3]
    p = np.dot(v, axis) * axis
    t = np.array([p[0], p[1], p[2], w])
    n = np.linalg.norm(t)
    if n < 1e-9:
        return 0.0
    t /= n
    ang = 2 * math.degrees(math.atan2(np.dot(t[:3], axis), t[3]))
    return (ang + 180) % 360 - 180

def local_axis(fr, parent, child):
    """Unit vector parent->child in the parent's local frame (the bone's own long axis)."""
    p, qp = L.bone_tf(fr, parent); c = L.bone_tf(fr, child)[0]
    return unit(L.qrot(qconj(qp), c - p))

def arm_rows(frames, ref_frames):
    out = {}
    R = ref_frames["0"]
    for s in "lr":
        ax_fore = local_axis(R, f"lowerarm_{s}", f"hand_{s}")
        ax_hand = local_axis(R, f"hand_{s}", f"middle_01_{s}")
        q_ref_fore = qrel(L.bone_tf(R, f"upperarm_{s}")[1], L.bone_tf(R, f"lowerarm_{s}")[1])
        q_ref_hand = qrel(L.bone_tf(R, f"lowerarm_{s}")[1], L.bone_tf(R, f"hand_{s}")[1])
        sw_ref = None
        rows, prev = [], None
        for f in range(len(frames)):
            fr = frames[str(f)]
            c5, fwd = L.chest_frame(fr)
            left = np.array([fwd[1], -fwd[0], 0.0])
            outward = left if s == "l" else -left
            (sh, qu), (el, qf), (wr, qh) = (L.bone_tf(fr, b) for b in (f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"))
            line = wr - sh; t = float(np.dot(el - sh, line) / max(np.dot(line, line), 1e-9))
            pole = unit(el - (sh + t * line))
            flex = 180.0 - math.degrees(math.acos(np.clip(np.dot(unit(sh - el), unit(wr - el)), -1, 1)))
            elev = math.degrees(math.acos(np.clip(np.dot(unit(el - sh), np.array([0, 0, -1.0])), -1, 1)))
            qr = qrel(qu, qf)
            fore_twist = twist_about(qrel(q_ref_fore, qr), ax_fore)
            hand_twist = twist_about(qrel(q_ref_hand, qrel(qf, qh)), ax_hand)
            # hinge direction: swing part of the elbow rotation, as an axis in the upper arm's frame
            # q and -q are the same rotation: pick w >= 0 first, or a bend that crosses 180 deg of stored angle reads as a
            # flipped hinge (Ganondorf v2, 01/11: elbow stored as 316 deg about -Z at f0-8, 73 deg about +Z at f9)
            v = np.asarray(qr[:3]) * (1.0 if qr[3] >= 0 else -1.0); sw = v - np.dot(v, ax_fore) * ax_fore
            sw = unit(sw) if np.linalg.norm(sw) > 1e-4 else None
            if f == 0 and sw is not None and sw_ref is None:
                sw_ref = sw
            reverse = bool(sw is not None and sw_ref is not None and flex > 20 and float(np.dot(sw, sw_ref)) < -0.2)
            tw_bones = {}
            for tb in (f"lowerarm_twist_01_{s}", f"lowerarm_twist_02_{s}", f"upperarm_twist_01_{s}"):
                if tb in fr:
                    par = f"lowerarm_{s}" if tb.startswith("lowerarm") else f"upperarm_{s}"
                    tw_bones[tb] = round(twist_about(qrel(L.bone_tf(fr, par)[1], L.bone_tf(fr, tb)[1]),
                                                     ax_fore if tb.startswith("lowerarm") else local_axis(R, f"upperarm_{s}", f"lowerarm_{s}")), 1)
            row = {"frame": f, "pole_fwd": round(float(np.dot(pole, fwd)), 2), "pole_out": round(float(np.dot(pole, outward)), 2),
                   "pole_up": round(float(pole[2]), 2), "elevation": round(elev, 1), "flex": round(flex, 1), "reverse": reverse,
                   "fore_twist": round(fore_twist, 1), "hand_twist": round(hand_twist, 1), "twist_bones": tw_bones}
            if prev:
                row["fore_twist_jump"] = round(abs(((fore_twist - prev["fore_twist"]) + 180) % 360 - 180), 1)
                row["hand_twist_jump"] = round(abs(((hand_twist - prev["hand_twist"]) + 180) % 360 - 180), 1)
            rows.append(row); prev = row
        out[s] = rows
    return out

def flags(rows_by_side, sheet, TH, frames):
    """Natural range for guard and recovery: elbow pointing down (pole_up <= max), not flared out (pole_out <= max), upper arm
    not raised past max elevation, no reverse bend, roll within range, no roll jump. Inside the action frames an elbow flare
    that points along the strike's motion is an exaggeration, accepted."""
    c0, c1 = sheet["contact"]
    win = set(range(c0, c1 + 1 + TH.get("joint_action_window_frames", 3)))
    res = []
    for s, rows in rows_by_side.items():
        for r in rows:
            f = r["frame"]
            issues = []
            if r["flex"] > TH["elbow_min_flex_for_pole_deg"]:
                # an upper arm hanging close to vertical has no "down" left for its elbow: a bent hanging arm points the
                # elbow back or in, which is natural (only a flare out, below, is a defect there)
                if r["pole_up"] > TH["elbow_pole_up_max"] and r["elevation"] >= TH.get("elbow_pole_min_elevation_deg", 40):
                    issues.append(("elbow pointing up/sideways", f"pole up {r['pole_up']:+.2f}"))
                if r["pole_out"] > TH["elbow_pole_out_max"]:
                    issues.append(("elbow flared out", f"pole out {r['pole_out']:+.2f}"))
                if r["elevation"] > TH["elbow_upperarm_elev_max_deg"]:
                    issues.append(("upper arm raised (chicken wing)", f"elevation {r['elevation']:.0f} deg"))
            if r["reverse"]:
                issues.append(("elbow bending the wrong way", "hinge axis flipped vs the guard"))
            if abs(r["fore_twist"]) > TH["twist_max_deg"]:
                issues.append(("forearm over-rolled (skin candy-wrapper risk)", f"forearm roll {r['fore_twist']:+.0f} deg"))
            if abs(r["hand_twist"]) > TH["twist_max_deg"]:
                issues.append(("hand over-rolled vs forearm", f"hand roll {r['hand_twist']:+.0f} deg"))
            if r.get("fore_twist_jump", 0) > TH["twist_jump_max_deg"] or r.get("hand_twist_jump", 0) > TH["twist_jump_max_deg"]:
                issues.append(("sudden roll jump", f"forearm {r.get('fore_twist_jump')} / hand {r.get('hand_twist_jump')} deg in one frame"))
            for what, val in issues:
                striking = s == sheet.get("strike_side", "l")
                accepted = f in win and striking and (
                    (what in ("elbow flared out", "upper arm raised (chicken wing)", "elbow pointing up/sideways") and r["elevation"] > 60 and r["flex"] < 60)
                    or what == "hand over-rolled vs forearm"                      # corkscrew of the strike (owner's blockout)
                    or (what == "sudden roll jump" and (c0 <= f <= c1 or max(r.get("fore_twist_jump", 0), r.get("hand_twist_jump", 0)) <= TH["twist_jump_action_max_deg"])))
                res.append({"joint": f"elbow_{s}" if "roll" not in what else f"arm_twist_{s}", "frame": f, "value": val, "issue": what,
                            "verdict": "exaggeration, accepted" if accepted else "unnatural",
                            "reason": ("strike exaggeration inside the action frames (" + what + ")") if accepted else what})
    return res

if __name__ == "__main__":
    ref = json.load(open(os.path.join(L.RAW, "blockout_legbones.json")))["frames"]
    for take in sys.argv[1:]:
        frames = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))["frames"]
        rows = arm_rows(frames, ref)
        print(f"== {take}: pole (fwd/out/up) | elevation | flex | reverse | forearm roll / hand roll")
        for s in "lr":
            for r in rows[s]:
                if r["frame"] in (0, 1, 3, 4, 5, 6, 8, 10, 12, 15, 18, 24, 30):
                    print(f"  {s} f{r['frame']:2d} {r['pole_fwd']:+.2f}/{r['pole_out']:+.2f}/{r['pole_up']:+.2f} | {r['elevation']:5.1f} | {r['flex']:5.1f} | {'REV' if r['reverse'] else '   '} | {r['fore_twist']:+6.1f} / {r['hand_twist']:+6.1f}  jumps {r.get('fore_twist_jump')}/{r.get('hand_twist_jump')}")
