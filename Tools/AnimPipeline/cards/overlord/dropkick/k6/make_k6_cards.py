"""Dropkick K6 (2026-11-21, owner on the K5 round 3 videos): weight, follow-through and force in the arms. K5 stays (variant).
Owner: more follow-through, secondary motion and weight overall (head, limbs); force in the arms: they PRESS in toward the body into
the crossed pose before relaxing a bit; YES to an overshoot after the strike (an extra montage segment).
Identity traits unchanged (review gate): legs together and straight from the strike on, arms crossed (upright fold a4-a13, lying fold
from a14), smug in the fall. Gameplay timing unchanged: startup 16 f, active from f15; the settle clip has the loop's length and
replaces the FIRST loop repetition in the montage, so every timeline frame keeps its time.

Writes, next to the K5 cards (which are not touched):
  overlord_dropkick_startup_k6.json  K5 startup re-keyed on its build frames (0, 4, 8, 11, 14, 15) + press/relax, head lag/overshoot
  overlord_dropkick_settle_k6.json   new 12 f one-shot after the strike: body overshoot -> rebound -> settle into the loop's f11
Curves: ../overlord_dropkick_k5_curves.md, section "K6".  Run (cwd Tools/AnimPipeline): python cards/overlord/dropkick/k6/make_k6_cards.py
Prints the offline fold metrics of every key (fold3.report: arm block over the sternum, elbows, min clearance).
"""
import copy, json, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DK = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(DK, "k5"))
sys.path.insert(0, os.getcwd())
from animpipe import author as A          # noqa: E402
import fold3 as F3                          # noqa: E402  (fold.py needs cwd = Tools/AnimPipeline)

K5S = json.load(open(os.path.join(DK, "overlord_dropkick_startup_k5.json")))
K5L = json.load(open(os.path.join(DK, "overlord_dropkick_loop_k5.json")))
S = A.frames(K5S["author"])                 # what the K5 take holds on every frame (smoothstep between author keys)
L = A.frames(K5L["author"])


def lerp(a, b, t):
    return A._lerp(a, b, t)


def add(v, d):
    return [round(x + y, 3) for x, y in zip(v, d)]


# --- arms: PRESS (tension) and RELAX around the passed folds ------------------------------------------------------------------
# Offline (k6 press_try): clavicle protraction (rz, left +, right -) is what moves the shoulders in; alone it pushes the left forearm
# into the right one (-0.15 cm at +10 deg), so the hands go 2-2.5 cm further across at the same time (the wrap gets deeper, the
# elbows come 3 cm toward the midline and the fold gains 2 cm of clearance back). clavicle ry: + depresses, - shrugs.
# spine: an extra curl hollows the chest around the arms (the trunk takes part in the squeeze).
def arms(p, clav=0.0, shrug=0.0, across=0.0, curl=0.0):
    q = copy.deepcopy(p)
    q["spine"] = add(q["spine"], [curl, 0, 0])
    for s, sg in (("l", 1), ("r", -1)):
        cv = list(q.get(f"clavicle_{s}") or [0.0, 0.0, 0.0])
        q[f"clavicle_{s}"] = add(cv, [0.0, -sg * shrug, sg * clav])
        a = q[f"arm_{s}"]
        a["hand"] = add(a["hand"], [-sg * across, 0, 0])
    return q


PRESS = dict(clav=float(os.environ.get("K6_PRESS_CLAV", 12)), shrug=float(os.environ.get("K6_PRESS_SHRUG", 0)),
             across=float(os.environ.get("K6_PRESS_ACROSS", 3)), curl=-3.0)
RELAX = dict(clav=-4.0, shrug=-0.0, across=-1.0, curl=1.0)
CLAMP = dict(clav=float(os.environ.get("K6_CLAMP_CLAV", 8)), shrug=0.0, across=float(os.environ.get("K6_CLAMP_ACROSS", 2)), curl=0.0)
LOOSE = dict(clav=-1.5, shrug=0.0, across=-1.0, curl=0.0)


def head(p, d=(0, 0, 0), neck=None, chin=None):
    q = copy.deepcopy(p)
    q["head"] = add(q["head"], list(d))
    if neck is not None or chin is not None:
        nc = copy.deepcopy(q.get("neck_chain") or {"neck_01": [0.0, 0.0, 0.0], "head": [0.0, 0.0, 0.0]})
        nc["neck_01"] = add(nc["neck_01"], [neck or 0.0, 0, 0])
        nc["head"] = add(nc["head"], [chin or 0.0, 0, 0])
        q["neck_chain"] = nc
    return q


# --- owner 2026-11-21 18:39Z (scope: the strike / extended pose, startup f14-f15, settle, fall loop): "torso pushed BACK and bent,
# legs FORWARD, so the legs are never lower than the body's plane, a bit like Big Band's j.HK". K5's strike was a straight plank with
# the legs 13.6 deg BELOW the horizontal (feet 18 cm under the hips). K6: pelvis +8 deg further back, hips flexed 12 deg (legs rotate
# up about the hips, still locked straight and together), trunk curled 8 deg up toward the legs: a shallow V with the hips lowest,
# legs ~7 deg ABOVE the horizontal (balls ~33 cm higher than K5 relative to the hips), chest and head raised. Offline numbers: k6/README
# (plank_try). Toe point unchanged (foot_rot is relative to the shin: 25 deg either way).
PLANK = dict(pitch=float(os.environ.get("K6_PLANK_PITCH", 8)), flex=float(os.environ.get("K6_PLANK_FLEX", 12)),
             curl=float(os.environ.get("K6_PLANK_CURL", -8)))


ARM_OFF = float(os.environ.get("K6_ARM_OFF", 2.0))


def _rotx(v, d):
    s_, c_ = math.sin(math.radians(d)), math.cos(math.radians(d)); x, y, z = v
    return [round(x, 3), round(y * c_ - z * s_, 3), round(y * s_ + z * c_, 3)]


def plank(p, t=1.0):
    q = copy.deepcopy(p)
    q["pelvis"]["rot"] = add(q["pelvis"]["rot"], [t * PLANK["pitch"], 0, 0])
    q["spine"] = add(q["spine"], [t * PLANK["curl"], 0, 0])
    for s in "lr":
        lg = q[f"leg_{s}"]
        lg["foot"] = _rotx(lg["foot"], t * PLANK["flex"]); lg["pole"] = _rotx(lg["pole"], t * PLANK["flex"])
        # the curl brings spine_03/04 up into the lying fold (first build: lowerarm_l|spine_03 -0.10 cm): the fold rides 2 cm
        # further off the chest (chest frame +y), keeping its shape
        a = q[f"arm_{s}"]; a["hand"] = add(a["hand"], [0, t * ARM_OFF, 0]); a["pole"] = add(a["pole"], [0, t * ARM_OFF, 0])
    return q


# --- startup: K5 sampled on its build frames, then the K6 layers -------------------------------------------------------------
# Re-keying on the build frames (0, 4, 8, 11, 14, 15) makes what the build samples exactly the key: K5's author keys 1, 2, 3, 7, 13
# reached the build only through the take's in-betweens (the build keeps the card's pose frames only, round 3 trap).
st = {f: copy.deepcopy(S[f]) for f in (0, 4, 8, 11, 14, 15)}
# a4 PRESS: the arms arrive in the fold harder than they stay (overshoot into the body); the head LAGS the tuck by ~a frame (6 deg
# less tucked than the body asks), so the 4 -> 8 segment carries it past.
st[4] = head(arms(st[4], **PRESS), (6.0, 0, 0))
# a8 RELAX: the arms let go a little (shoulders back, wrap 1 cm looser, chest opens 1 deg); the head overshoots the tuck by 6 deg
# (follow-through of the lag), the knees overshoot toward the chest by 2 cm (the weight of the legs arriving in the ball).
st[8] = head(arms(st[8], **RELAX), (-6.0, 0, 0))
for s in "lr":
    st[8][f"leg_{s}"]["foot"] = add(st[8][f"leg_{s}"]["foot"], [0, 0, 2.0])
# a11 the load: the arms gather again (half a press) and the head settles back on the target (K5's -40): tension before the shot.
st[11] = arms(st[11], **{k: v * 0.5 for k, v in PRESS.items()})
# a14 smear: K5 (fold3.fit_path key, clear over its blends). a15 STRIKE: legs, pitch and the smug flick exactly K5 (crisp, no delay);
# the lying fold CLAMPS (shoulders in, wrap deeper): the arms brace on the impact and let go in the settle clip.
st[14] = plank(st[14], 0.66)           # smear: the legs are 2/3 out, so 2/3 of the new line
st[15] = arms(plank(st[15]), **CLAMP)
STRIKE_K6 = st[15]

# --- settle: 12 f one-shot after the strike (replaces loop repetition 1) -----------------------------------------------------
# Order of the follow-through: pelvis and legs peak first (f2), the head 2 frames later (f4), the arms let go last (f4 -> f7).
# f11 = the K5 loop's f11 so settle f11 -> loop f0 is the loop's own wrap step.
LOOP_K6 = {int(k): plank(v) for k, v in K5L["author"]["poses"].items()}   # fall loop K6: K5's smug hold on the new line
LOOP11 = copy.deepcopy(A.frames({"frames": 12, "poses": {str(k): v for k, v in LOOP_K6.items()}})[11])
PEAK = copy.deepcopy(STRIKE_K6)
PEAK["pelvis"]["rot"] = add(PEAK["pelvis"]["rot"], [float(os.environ.get("K6_PEAK_PITCH", 5.0)), 0, 0])   # 82 -> 87: carried past
PEAK["pelvis"]["loc"] = add(PEAK["pelvis"]["loc"], [0, 2.0, 0])                                          # the hips drive on 2 cm
PEAK["spine"] = add(PEAK["spine"], [-4.0, 0, 0])                                                         # chest out (whip of the trunk)
for s in "lr":
    PEAK[f"leg_{s}"]["foot_rot"] = add(PEAK[f"leg_{s}"]["foot_rot"], [-10.0, 0, 0])                      # toes snap further (-60)
UNDER = copy.deepcopy(LOOP11)
UNDER["pelvis"]["rot"] = add(UNDER["pelvis"]["rot"], [-2.5, 0, 0])                                       # rebound under the hold (79.5)
UNDER["spine"] = add(UNDER["spine"], [2.0, 0, 0])
for s in "lr":
    UNDER[f"leg_{s}"]["foot_rot"] = add(UNDER[f"leg_{s}"]["foot_rot"], [4.0, 0, 0])                      # toes relax a touch (-46)
HEAD_PEAK = dict(d=(0, -6.0, 3.0), neck=-3.0, chin=6.0)     # chin flicks 6 deg past the smug attitude, turns 6 more, tilts 3 more
HEAD_DIP = dict(d=(0, 2.0, -1.0), neck=1.0, chin=-3.0)      # and comes back 3 deg under it before it settles (a nod of satisfaction)


def head_of(src, dst):
    q = copy.deepcopy(dst)
    q["head"] = copy.deepcopy(src["head"]); q["neck_chain"] = copy.deepcopy(src["neck_chain"])
    return q


def arms_of(src, dst):
    q = copy.deepcopy(dst)
    for k in ("arm_l", "arm_r", "clavicle_l", "clavicle_r"):
        q[k] = copy.deepcopy(src[k])      # arms in the chest frame: the trunk (dst) carries them
    return q


st_loose = arms(LOOP11, **LOOSE)
se = {}
se[0] = head_of(STRIKE_K6, arms_of(STRIKE_K6, lerp(STRIKE_K6, PEAK, 0.55)))                  # body on its way past; head held
se[2] = head_of(lerp(STRIKE_K6, head(STRIKE_K6, **HEAD_PEAK), 0.5), arms_of(STRIKE_K6, PEAK))  # body peak; head halfway; arms clamped
se[4] = head_of(head(STRIKE_K6, **HEAD_PEAK), arms_of(lerp(STRIKE_K6, st_loose, 0.35), lerp(PEAK, UNDER, 0.7)))  # head peak
se[7] = head_of(head(LOOP11, **HEAD_DIP), arms_of(st_loose, UNDER))                            # body under; head dips; arms loose
se[11] = LOOP11                                                                                 # = loop f11: the wrap into loop f0


def write(name, src, poses, frames_spacing, extra_change, anim=None):
    c = copy.deepcopy(src)
    c["move"] = name
    if anim:
        c["anim"], c["source_anim"], c["author"]["take"] = anim
    else:
        c["anim"] = c["anim"].replace("K5", "K6"); c["source_anim"] = c["source_anim"].replace("K5", "K6")
        c["author"]["take"] = c["author"]["take"].replace("K5", "K6")
    c["author"]["poses"] = {str(f): poses[f] for f in sorted(poses)}
    names = {q["frame"]: q["name"] for q in src["poses"]}
    c["poses"] = [dict({"name": names.get(f, f"k6_{f}"), "frame": f, "source": None}, **({"spacing": sp} if sp else {}))
                  for f, sp in frames_spacing]
    c["part_delays"] = {"chest": 0, "head": 0, "forearm": 0, "hand": 0}
    c["_changes"] = "k6 (2026-11-21, owner on the K5 r3 videos): " + extra_change + " Previous: " + str(src.get("_changes", ""))[:200]
    out = os.path.join(DK, name + ".json")
    json.dump(c, open(out, "w"), indent=1)
    return out


if __name__ == "__main__":
    sp = {q["frame"]: q.get("spacing") for q in K5S["poses"]}
    o1 = write("overlord_dropkick_startup_k6", K5S, st, [(f, sp.get(f)) for f in sorted(st)],
               "re-keyed on the build frames; a4 arms PRESS into the fold (shoulders in, wrap deeper, chest curls), a8 relax a little, "
               "a11 gather; head lags the tuck at a4 and overshoots at a8; knees overshoot 2 cm at a8; a15 lying fold clamps.")
    settle = copy.deepcopy(K5L)
    settle["_design"] = ("Overlord Dropkick strike settle (K6): one-shot 12 f after the strike, before the falling loop. Pelvis and legs "
                         "carry past the lock (pitch +5, toes -10) and rebound under it, the head flicks past the smug attitude two "
                         "frames later and nods back, the crossed arms let go of the impact clamp last.")
    settle["sheet_extra"] = {"hold_window": [11, 11], "strike_limb": "leg_l", "strike_side": "leg_l"}
    o2 = write("overlord_dropkick_settle_k6", settle, se,
               [(0, None), (2, "arrive_1_2"), (4, "hold_recoil"), (7, "hold_recoil"), (11, "arrive_1_2")],
               "NEW strike settle: body overshoot f2, head overshoot f4, arms loosen f4-f7, rebound f7, = loop f11 at f11.",
               anim=("/Game/AnimPipeline/Rig/Overlord/Dropkick/Loop/AS_OverlordDropkick_SettleK6",
                     "/Game/AnimPipeline/Pilot/AS_Pilot_OverlordDropkick_SettleK6", "AS_Pilot_OverlordDropkick_SettleK6"))
    loop = copy.deepcopy(K5L)
    o3 = write("overlord_dropkick_loop_k6", loop, LOOP_K6, [(q["frame"], q.get("spacing")) for q in K5L["poses"]],
               "K5 smug hold on the owner's Big Band line (torso back and curled, legs forward above the body plane).")
    print(o3)
    po = A.Poser("along")
    rows = []
    for tag, poses in (("startup", st), ("settle", se), ("loop", LOOP_K6)):
        for f in sorted(poses):
            rows.append(f"{tag} " + F3.report(po, poses[f], f))
        ks = sorted(poses)
        for a, b in zip(ks, ks[1:]):             # the blends between keys (the build's in-betweens, approximately)
            for t in (0.25, 0.5, 0.75):
                rows.append(f"{tag} path{a}-{b}@{t} min_cl {min(F3.metrics(po, A._lerp(poses[a], poses[b], t))[0]['cl'].values()):.2f}")
    rep = os.path.join(os.environ.get("TEMP", "."), "k6", "k6_fold_report.txt")
    os.makedirs(os.path.dirname(rep), exist_ok=True)
    open(rep, "w").write("\n".join(rows) + "\n")
    worst = min(rows, key=lambda r: float(r.rsplit(" ", 1)[1]))
    print(o1); print(o2); print("worst clearance: " + worst); print(rep)
