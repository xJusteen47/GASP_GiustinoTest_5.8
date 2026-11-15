"""Jab 1 / jab 2 round 3 (2026-11-15): the owner's Cascadeur poses are the base, completed into a full arc with follow-through.

    python cards/overlord/jab/jab_r3_plans.py          (from Tools/AnimPipeline) -> owner_r3/overlord_jab{1,2}_r3.plan.json
    then: python pose_plan.py cards/overlord/jab/owner_r3/overlord_jab1_r3.plan.json   (same for jab 2), and put the
    timing csvs back (pose_plan writes a reaction csv over them): this script's --restore-csv does it.

Owner base (scene "overlord jab", Step keys, read 2026-11-15 into owner_r3/owner_jab_casc_*.json, converted with
casc_to_author.py into owner_jab_author_*.json, rig ratios 1.0):
  jab 1: O5 = arm straight in front at shoulder height, O7 = backhand swept out to the RIGHT (end of the owner's arc);
  jab 2: O17 = wide turned stance, hand chambered on the right, O19 = palm swept to the LEFT in front, O21 = deeper
  follow-through, hand low on the left.
What this adds (never replaces an owner key): chamber/startup before the contact (jab 1 keeps the v3 startup coil, which
already winds the trunk the same way), the snap into contact (30% / 70%), the arc continuing PAST the owner's last key
and slowing (follow-through), a small recoil in the hold, limb-led recovery, steps with a lift so no foot slides.
Not touched (owner places them): anim_active, faf_game, hitboxes, timelines. Fingers: open hands (owner 2026-11-08).
"""
import copy, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OWN = os.path.join(HERE, "owner_r3")
CASC = json.load(open(os.path.join(OWN, "owner_jab_casc_2026-11-15.json")))
O = json.load(open(os.path.join(OWN, "owner_jab_author_2026-11-15.json")))["poses"]
_v3 = lambda n: json.load(open(os.path.join(HERE, f"{n}.r2b.backup.json" if os.path.exists(os.path.join(HERE, f"{n}.r2b.backup.json")) else f"{n}.json")))
V1, V2 = _v3("overlord_jab1_v3"), _v3("overlord_jab2_v3")   # always the v3 r2b card, never the r3 output
IDLE = V1["author"]["poses"]["22"]           # UE standing idle in card space (pelvis 0, guard hands)
UE_FOOT = {"leg_l": [19.99, 12.54, 9.38], "leg_r": [-14.49, -20.42, 9.37]}   # idle_neutral_legbones frame 0
CF = {"leg_l": "foot_l", "leg_r": "foot_r"}
BODY = ("pelvis", "spine", "head", "arm_l", "arm_r")


def lerp(a, b, t):
    if isinstance(a, dict):
        return {k: lerp(a[k], b[k], t) if k in b else copy.deepcopy(a[k]) for k in a}
    if isinstance(a, list):
        return [round(x + (y - x) * t, 3) for x, y in zip(a, b)]
    return a


def owner(f):
    """Owner pose f in card space; pelvis loc gets the owner's horizontal shift (the converter keeps only the height)."""
    p = {k: copy.deepcopy(O[str(f)][k]) for k in BODY}
    pc, p0 = CASC[str(f)]["pelvis"], CASC["0"]["pelvis"]
    p["pelvis"]["loc"] = [round(pc[0] - p0[0], 2), round(pc[2] - p0[2], 2), p["pelvis"]["loc"][2]]
    return p


def foot_w(f, leg, lift=0.0):
    """World target of the owner's foot at f, moved onto the UE idle stance by the owner's own delta from his idle."""
    c, c0, u = CASC[str(f)][CF[leg]], CASC["0"][CF[leg]], UE_FOOT[leg]
    return [round(u[0] + c[0] - c0[0], 2), round(u[1] + c[2] - c0[2], 2), round(u[2] + c[1] - c0[1] + lift, 2)]


def leg(f, l, lift=0.0, pole_from=None):
    pole = O[str(pole_from if pole_from is not None else f)][l]["pole"]
    d = {"foot_w": foot_w(f, l, lift), "pole": pole}
    if lift == 0:
        d["flat"] = True
    return d


def mid_w(a, b, t, lift):
    return [round(a[i] + (b[i] - a[i]) * t, 2) for i in range(2)] + [round(a[2] + (b[2] - a[2]) * t + lift, 2)]


def idle_foot_w(l):
    return list(UE_FOOT[l])


def body_of(p):
    return {k: copy.deepcopy(p[k]) for k in BODY if k in p}


def extend(a, b, t, head=0.4, arm=None, off="arm_l"):
    """Pose b pushed past b along (b - a) by t (follow-through); the head goes less far (it stabilises first)."""
    out = lerp(b, a, -t)
    out["head"] = lerp(b["head"], a["head"], -t * head)
    if arm is not None:   # the trunk carries the arc on; the arm keeps the owner's extension (preview r3a: full
        out[arm] = lerp(b[arm], a[arm], -t * 0.35)   # extrapolation folded the elbow in) and the off arm settles
        out[off] = lerp(b[off], a[off], -t * 0.2)
    return out


def key(body, legs, fingers, **kw):
    k = body_of(body)
    k.update(legs)
    k["fingers"] = [fingers, 0.0, 0.0]
    k.update(kw)
    return k


def jab1():
    v = V1["author"]["poses"]
    o5, o7 = owner(5), owner(7)
    fr, fl = foot_w(5, "leg_r"), foot_w(5, "leg_l")
    ir, il = idle_foot_w("leg_r"), idle_foot_w("leg_l")
    stand = lambda: {"leg_l": "idle_w", "leg_r": "idle_w"}
    K = {}
    # startup: v3 f0-f4 body (same coil direction as the owner's swing), feet step into the owner's stance
    K[0] = key(v["0"], stand(), 0.4)
    K[1] = key(v["1"], stand(), 0.5)
    K[2] = key(v["2"], {"leg_l": "idle_w", "leg_r": {"foot_w": ir, "pole": O["5"]["leg_r"]["pole"], "flat": True}}, 0.6)
    K[3] = key(v["3"], {"leg_l": {"foot_w": il, "pole": O["5"]["leg_l"]["pole"], "flat": True},
                        "leg_r": {"foot_w": mid_w(ir, fr, 0.5, 6), "pole": O["5"]["leg_r"]["pole"]}}, 0.8)
    K[4] = key(v["4"], {"leg_l": {"foot_w": mid_w(il, fl, 0.5, 5), "pole": O["5"]["leg_l"]["pole"]},
                        "leg_r": leg(5, "leg_r")}, 1.0)
    K[5] = key(lerp(body_of(v["4"]), o5, 0.3), {"leg_l": leg(5, "leg_l"), "leg_r": leg(5, "leg_r")}, 1.0)
    # strike: owner keys, contact on the first active frame, the arc reaches the owner's right at f8
    K[6] = key(o5, {"leg_l": leg(5, "leg_l"), "leg_r": leg(5, "leg_r")}, 1.0)
    K[8] = key(o7, {"leg_l": leg(7, "leg_l"), "leg_r": leg(7, "leg_r")}, 1.0)
    # follow-through: keeps going past the owner's last key, slowing (+30%, +45%, +50% of the last step), then recoil
    for f, t in ((9, 0.3), (10, 0.45), (11, 0.5)):
        K[f] = key(extend(o5, o7, t, arm="arm_r"), {"leg_l": leg(7, "leg_l"), "leg_r": leg(7, "leg_r")}, 1.0)
    K[11]["hold"] = True
    K[13] = key(extend(o5, o7, 0.44, arm="arm_r"), {"leg_l": leg(7, "leg_l"), "leg_r": leg(7, "leg_r")}, 1.0)
    # recovery, limb led: the arm comes home first (60% at f16 while the trunk is at 40%), feet step back with a lift
    rec = lerp(K[13], body_of(IDLE), 0.4)
    rec["arm_r"] = lerp(K[13]["arm_r"], IDLE["arm_r"], 0.6)
    K[16] = key(rec, {"leg_l": leg(7, "leg_l"), "leg_r": {"foot_w": mid_w(fr, ir, 0.5, 6), "pole": O["7"]["leg_r"]["pole"]}}, 0.8)
    K[18] = key(lerp(K[13], body_of(IDLE), 0.75), {"leg_l": {"foot_w": mid_w(fl, il, 0.5, 5), "pole": O["7"]["leg_l"]["pole"]},
                                                    "leg_r": "idle_w"}, 0.5)
    K[20] = key(lerp(K[13], body_of(IDLE), 0.93), stand(), 0.2)
    K[22] = key(IDLE, stand(), 0.05)
    K[28] = key(IDLE, stand(), 0.05)
    return K


def jab2():
    o17, o19, o21 = owner(17), owner(19), owner(21)
    fr, fl = foot_w(17, "leg_r"), foot_w(17, "leg_l")
    ir, il = idle_foot_w("leg_r"), idle_foot_w("leg_l")
    stand = lambda: {"leg_l": "idle_w", "leg_r": "idle_w"}
    st = lambda f: {"leg_l": leg(f, "leg_l"), "leg_r": leg(f, "leg_r")}
    ib = body_of(IDLE)
    K = {}
    # startup: f0 already reads as the move (feel rule); right foot steps forward f0-f3, left foot crosses back f2-f6
    K[0] = key(lerp(ib, o17, 0.3), stand(), 0.5)
    K[2] = key(lerp(ib, o17, 0.55), {"leg_l": "idle_w", "leg_r": {"foot_w": mid_w(ir, fr, 0.6, 7), "pole": O["17"]["leg_r"]["pole"]}}, 0.7)
    K[3] = key(lerp(ib, o17, 0.68), {"leg_l": {"foot_w": il, "pole": O["17"]["leg_l"]["pole"], "flat": True}, "leg_r": leg(17, "leg_r")}, 0.8)
    K[5] = key(lerp(ib, o17, 0.88), {"leg_l": {"foot_w": mid_w(il, fl, 0.6, 7), "pole": O["17"]["leg_l"]["pole"]}, "leg_r": leg(17, "leg_r")}, 1.0)
    K[7] = key(o17, st(17), 1.0)                      # owner chamber, hand on the right
    K[8] = key(lerp(o17, o19, 0.3), st(17), 1.0)      # pre-snap
    K[9] = key(o19, st(19), 1.0)                      # owner contact, palm to the left (first active frame)
    K[11] = key(o21, st(21), 1.0)                     # owner follow-through key
    for f, t in ((12, 0.25), (13, 0.35), (14, 0.38)):
        K[f] = key(extend(o19, o21, t), st(21), 1.0)
    K[14]["hold"] = True
    K[16] = key(extend(o19, o21, 0.33), st(21), 1.0)
    # recovery, arm first, feet back: left foot f17-f20, right foot f20-f23
    rec = lerp(K[16], ib, 0.35)
    rec["arm_r"] = lerp(K[16]["arm_r"], IDLE["arm_r"], 0.55)
    K[18] = key(rec, {"leg_l": {"foot_w": mid_w(fl, il, 0.5, 7), "pole": O["21"]["leg_l"]["pole"]}, "leg_r": leg(21, "leg_r")}, 0.8)
    K[20] = key(lerp(K[16], ib, 0.65), {"leg_l": "idle_w", "leg_r": leg(21, "leg_r")}, 0.5)
    K[22] = key(lerp(K[16], ib, 0.85), {"leg_l": "idle_w", "leg_r": {"foot_w": mid_w(fr, ir, 0.5, 6), "pole": O["21"]["leg_r"]["pole"]}}, 0.25)
    K[24] = key(lerp(K[16], ib, 0.97), stand(), 0.1)
    K[26] = key(IDLE, stand(), 0.05)
    K[52] = key(IDLE, stand(), 0.05)
    return K


def plan(v, keys, name):
    card = {k: copy.deepcopy(val) for k, val in v.items() if k not in ("author", "_plan")}
    n = v["author"]["frames"]
    card["poses"] = [{"name": f"f{f}", "frame": f, "source": None, **({"origin": "claude", "why": "owner's Cascadeur Step key (owner_r3/owner_jab_casc_2026-11-15.json) converted by "
                                  "casc_to_author.py; no Invia request exists, so it is not an animpipe owner key"}
                                if f in owner_frames(name) else {})}
                     for f in range(n)]
    card["_changes"] = (v.get("_changes", "") + " r3 (2026-11-15): owner's Cascadeur poses are the base (owner_r3/), full arc "
                        "with follow-through past the owner's last key, built by jab_r3_plans.py + pose_plan.py. v3 r2b card "
                        f"kept as {name}.r2b.backup.json.").strip()
    card["planted"] = []
    card["_planted_why"] = "r3: feet go through world targets (foot_w / idle_w) on every key, steps lifted; nothing to snap."
    return {"card_path": f"cards/overlord/jab/{name}.json", "take": v["author"]["take"], "frames": n, "thumb": "along",
            "card": card, "keys": {str(f): k for f, k in sorted(keys.items())}}


def owner_frames(name):
    return {6, 8} if "jab1" in name else {7, 9, 11}


def main():
    if "--restore-csv" in sys.argv:
        import shutil
        for n in ("OverlordJab1V3_timing.csv", "OverlordJab2V3_timing.csv"):
            src = os.path.join(OWN, "timing_keep", n)
            for d in (os.path.join(HERE, "timing"), os.path.join(HERE, "..", "..", "..", "..", "..", "Saved", "AnimPipeline", "refs", "ufd")):
                shutil.copy(src, os.path.join(d, n))
        print("ok: timing csvs restored")
        return
    os.makedirs(os.path.join(OWN, "timing_keep"), exist_ok=True)
    import shutil
    for n in ("OverlordJab1V3_timing.csv", "OverlordJab2V3_timing.csv"):
        dst = os.path.join(OWN, "timing_keep", n)
        if not os.path.exists(dst):
            shutil.copy(os.path.join(HERE, "timing", n), dst)
    for v, k, name in ((V1, jab1(), "overlord_jab1_v3"), (V2, jab2(), "overlord_jab2_v3")):
        bk = os.path.join(HERE, f"{name}.r2b.backup.json")
        if not os.path.exists(bk):
            shutil.copy(os.path.join(HERE, f"{name}.json"), bk)
        json.dump(plan(v, k, name), open(os.path.join(OWN, name.replace("_v3", "_r3") + ".plan.json"), "w"), indent=1)
    print("ok: plans written in owner_r3/")


if __name__ == "__main__":
    main()
