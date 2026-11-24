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


# round 1 values (superseded below by the round-2 PRESS; RELAX is unused since round 2)
PRESS = dict(clav=float(os.environ.get("K6_PRESS_CLAV", 12)), shrug=float(os.environ.get("K6_PRESS_SHRUG", 0)),
             across=float(os.environ.get("K6_PRESS_ACROSS", 3)), curl=-3.0)
RELAX = dict(clav=-4.0, shrug=-0.0, across=-1.0, curl=1.0)
CLAMP = dict(clav=float(os.environ.get("K6_CLAMP_CLAV", 8)), shrug=0.0, across=float(os.environ.get("K6_CLAMP_ACROSS", 2)), curl=0.0)
LOOSE = dict(clav=-1.5, shrug=0.0, across=-1.0, curl=0.0)


def arms_of(src, dst):
    q = copy.deepcopy(dst)
    for k in ("arm_l", "arm_r", "clavicle_l", "clavicle_r"):
        q[k] = copy.deepcopy(src[k])      # arms in the chest frame: the trunk (dst) carries them
    return q


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
# ROUND 2 (review k6a FAIL posa, 2026-11-21): the round-1 press was a POSE difference (3-4 cm), not a movement. The arms now OPEN
# first (a3 = key 2: elbows flared ~38 cm from the press, left hand high, right low and off the chest so the crossing path keeps
# left over right), SLAM into the press (key 4 = a5, arrive_1_2: fast out of the open, soft arrival), hold it (key 5) and RELEASE at
# a8 (key 7: elbows out ~8 cm, shoulders down, forearms off the chest). Key 8 is gone (its head/knee overshoot moved to key 7).
# Clearance: the press compresses the chest 6 deg (curl -6) and shrugs 4, bought back by riding the block 2 cm off the chest.
del st[8]
PRESS = dict(clav=12.0, shrug=4.0, across=3.0, curl=-6.0)
PRESS_OFF = 2.0                        # chest-frame +y of hands and poles in the press (clearance for the curl and shrug)
OPEN = dict(t=0.3, out=15.0, hout=8.0, up=5.0, hl_up=10.0, hr_down=8.0, hr_fwd=4.0)
REL = dict(out=16.0, down=8.0, hout=1.0, off=3.0, hl_off=2.5, clav=-6.0, shrug=-5.0, across=-1.0, curl=2.0)


def elbows(p, out=0.0, down=0.0, hout=0.0, off=0.0):
    """Elbows out/down through the pole targets, hands out (sg: left +x); off = whole arm block off the chest (+y)."""
    q = copy.deepcopy(p)
    for s, sg in (("l", 1), ("r", -1)):
        a = q[f"arm_{s}"]
        a["pole"] = add(a["pole"], [sg * out, off, -down]); a["hand"] = add(a["hand"], [sg * hout, off, 0])
    return q


def hands(p, l=(0, 0, 0), r=(0, 0, 0)):
    q = copy.deepcopy(p)
    q["arm_l"]["hand"] = add(q["arm_l"]["hand"], list(l)); q["arm_r"]["hand"] = add(q["arm_r"]["hand"], list(r))
    return q


press_arms = lambda p, k=1.0: elbows(arms(p, **{kk: v * k for kk, v in PRESS.items()}), off=k * PRESS_OFF)
# open v2 (after the first r2 build: at a3 the v1 open read as the guard, the elbows were already near their reach): the arms of
# the guard pose WINGED - elbows lifted ~12 cm to shoulder height and back, left fist up and out (+15 cm), right fist out and
# forward, shoulders back (clav -10), chest open (curl +4); the body is the 0->4 tuck at 30 %.
open_arms = hands(arms(elbows(st[0], out=15.0, down=-20.0, hout=16.0), clav=-10.0, curl=4.0), l=(0, 0, 14.0), r=(0, 6.0, 2.0))
st[2] = arms_of(open_arms, lerp(st[0], st[4], OPEN["t"]))
# a5 PRESS: the head LAGS the tuck by ~a frame (6 deg less tucked), so the following segment carries it past
st[4] = head(press_arms(st[4]), (6.0, 0, 0))
st[5] = head(press_arms(S[5], 0.9), (3.0, 0, 0))             # a6: held, still squeezing
# a8 RELEASE + the head overshoots the tuck by 6 deg, the knees overshoot toward the chest by 2 cm (weight of the legs)
st[7] = head(hands(arms(elbows(S[7], out=REL["out"], down=REL["down"], hout=REL["hout"], off=REL["off"]),
                        clav=REL["clav"], shrug=REL["shrug"], across=REL["across"], curl=REL["curl"]), l=(0, REL["hl_off"], 0)), (-6.0, 0, 0))
for s in "lr":
    st[7][f"leg_{s}"]["foot"] = add(st[7][f"leg_{s}"]["foot"], [0, 0, 2.0])
# a11 the load: the arms gather again (half a press) and the head settles back on the target (K5's -40): tension before the shot.
st[11] = press_arms(st[11], 0.5)
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
PEAK["pelvis"]["rot"] = add(PEAK["pelvis"]["rot"], [float(os.environ.get("K6_PEAK_PITCH", 11.0)), 0, 0])   # 82 -> 87: carried past
PEAK["pelvis"]["loc"] = add(PEAK["pelvis"]["loc"], [0, 3.5, 0])                                          # the hips drive on 3.5 cm
PEAK["spine"] = add(PEAK["spine"], [-7.0, 0, 0])                                                         # chest out (whip of the trunk)
for s in "lr":
    PEAK[f"leg_{s}"]["foot_rot"] = add(PEAK[f"leg_{s}"]["foot_rot"], [-14.0, 0, 0])                      # toes snap further (-60)
UNDER = copy.deepcopy(LOOP11)
UNDER["pelvis"]["rot"] = add(UNDER["pelvis"]["rot"], [-3.5, 0, 0])                                       # rebound under the hold (79.5)
UNDER["spine"] = add(UNDER["spine"], [3.5, 0, 0])
for s in "lr":
    UNDER[f"leg_{s}"]["foot_rot"] = add(UNDER[f"leg_{s}"]["foot_rot"], [4.0, 0, 0])                      # toes relax a touch (-46)
HEAD_PEAK = dict(d=(0, -9.0, 5.0), neck=-5.0, chin=11.0)  # round 2: >= 10 past the smug     # chin flicks 6 deg past the smug attitude, turns 6 more, tilts 3 more
HEAD_DIP = dict(d=(0, 3.0, -2.0), neck=2.0, chin=-5.0)      # and comes back 3 deg under it before it settles (a nod of satisfaction)


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
lift = lambda p, off: hands(elbows(p, off=off), l=(0, 0.5 * off, 0))   # the top (left) forearm rides higher: clears the right one
LIFT = (2.5, 5.0, 2.0)     # round 2: the forearms come 4-5 cm off the chest after the impact and fall back LAST (chest frame +y)
se[0] = head_of(STRIKE_K6, arms_of(STRIKE_K6, lerp(STRIKE_K6, PEAK, 0.55)))                  # body on its way past; head held
se[2] = head_of(lerp(STRIKE_K6, head(STRIKE_K6, **HEAD_PEAK), 0.5), arms_of(lift(STRIKE_K6, LIFT[0]), PEAK))  # body peak
se[4] = head_of(head(STRIKE_K6, **HEAD_PEAK), arms_of(lift(lerp(STRIKE_K6, st_loose, 0.35), LIFT[1]), lerp(PEAK, UNDER, 0.7)))
se[7] = head_of(head(LOOP11, **HEAD_DIP), arms_of(lift(st_loose, LIFT[2]), UNDER))     # body under; head dips; arms coming down
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
    SPACING = {2: "arrive_1_2", 4: "hold_recoil", 5: "hold_recoil", 7: "charge_long"}   # open -> slam -> hold -> release -> load
    o1 = write("overlord_dropkick_startup_k6", K5S, st, [(f, SPACING.get(f, sp.get(f))) for f in sorted(st)],
               "ROUND 2: arms OPEN a3 (key 2), SLAM into the press a5 (key 4, chest curls 6, shoulders up), hold a6, RELEASE a8 (key 7, "
               "elbows out, shoulders down); key 8 removed. Round 1: re-keyed on the build frames; a4 arms PRESS into the fold (shoulders in, wrap deeper, chest curls), a8 relax a little, "
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
