"""Dropkick K8 = K7 + the Doctor Doom silhouette study (owner 2026-11-23: try ALL the proposals). K7 stays as the fallback.
Owner scope: startup a1-a12 only. Untouched: snap a13-a15 (keys 14, 15), settle K6, smug fall loop K6, legs together, arms crossed
in the fall, timeline and hitboxes. Proposals (thread "Study Doctor Doom attack for dropkick silhouette"):
  1A crossed arms HIGH on the chest with the elbows pushed OUT past the torso width (from behind they are the "pauldrons").
  2A the tuck stays tight (legs = K7), the upper body gets wider (shoulders up and wide, elbows wide).
  3A open "goal-post" pose (upper arms out at shoulder height, forearms up, fists up) HELD a1-a10 as a moving hold, then a sharp
     2-frame close (a10 -> a12, arrive_1_2) into the crossed arms before a13; replaces K7's wings/press/release.
  4  head as a hump centred between the shoulders (shoulders shrugged around it, neck sunk): from a12.
  5  closed silhouette motionless: a12 -> a14 is charge_long (the closed shape barely moves until the snap), no press/relax wobble.
  6  feet toward the target: soles flat (foot_rot -> 0, at most +28 deg vs K7) and the ball rotated back further (pelvis +12,
     82 at the strike anyway), so the soles face the attack direction before the snap.
Writes overlord_dropkick_startup_k8.json. Run (cwd Tools/AnimPipeline): python cards/overlord/dropkick/k8/make_k8_cards.py
"""
import contextlib, copy, io, json, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
DK = os.path.dirname(HERE)
for d in ("k7", "k6", "k5"):
    sys.path.insert(0, os.path.join(DK, d))
sys.path.insert(0, HERE)
sys.path.insert(0, os.getcwd())
with contextlib.redirect_stdout(io.StringIO()):
    import make_k7_cards as K7
K6 = K7.K6
from animpipe import author as A              # noqa: E402
import fold as F                              # noqa: E402
import measure as MS                          # noqa: E402

E = lambda n, d: float(os.environ.get(n, d))
po = A.Poser("along")


def anat(pose):
    """Card arm frame <- anatomical chest frame (x side +left, y forward, z up, origin spine_05), as in fold.fit."""
    P0, _ = po.solve(pose); upw = P0["neck_01"] - P0["spine_03"]; upw /= np.linalg.norm(upw)
    sd = P0["upperarm_l"] - P0["upperarm_r"]; sd -= (sd @ upw) * upw; sd /= np.linalg.norm(sd)
    Dch = A.rv(pose.get("spine", [0, 0, 0])) @ A.rv(pose["pelvis"]["rot"])
    return Dch.T @ np.column_stack([sd, np.cross(upw, sd), upw])


def set_arm(pose, s, hand, elbow):
    """hand / elbow in the anatomical frame; the pole sits ON the wanted elbow (fold.fit convention)."""
    M = anat(pose)
    pose[f"arm_{s}"] = {"hand": [round(float(v), 2) for v in M @ np.array(hand, float)],
                        "pole": [round(float(v), 2) for v in M @ np.array(elbow, float)]}
    return pose


# --- 3A goal-post: upper arms out at shoulder height, forearms up, fists up. Left fist a touch higher, right elbow a touch lower
# (no twinning). drift = moving hold (elbows creep up/back ~2 cm), pre = the last open frame, a little wider (anticipation of the close)
GP = dict(ex=E("K8_GP_EX", 41.0), ey=-5.0, ez=1.0, hz=E("K8_GP_HZ", 24.0), hy=-1.0)


def goal_post(pose, drift=0.0, pre=0.0):
    q = copy.deepcopy(pose)
    for s, sg, dz, dh in (("l", 1, 0.0, 3.0), ("r", -1, -2.0, 0.0)):
        ex = GP["ex"] + pre * 2.0
        el = [sg * ex, GP["ey"] - drift - pre, GP["ez"] + dz + drift]
        hd = [sg * (ex + 1.0 + pre), GP["hy"] - drift - pre, GP["ez"] + dz + GP["hz"] + dh + drift]
        set_arm(q, s, hd, el)
        q[f"clavicle_{s}"] = [0.0, round(-sg * (2.0 + pre * 2), 2), round(-sg * 6.0, 2)]   # shoulders a bit up and back (chest open)
    return q


# --- 1A/2A the wide high fold: forearms stacked left over right high on the chest, elbows out past the shoulders.
WIDE = dict(elb_out=E("K8_ELB_OUT", 10.0), elb_z=E("K8_ELB_Z", -6.0))


def wide_cost(m, tight=False, terms=None):
    cl = m["cl"]; P = m["P"]; T = {}
    add = lambda k, v: T.__setitem__(k, T.get(k, 0.0) + float(v))
    for k, v in cl.items():
        if v < 1.0:
            add("clear", 40 * (1.0 - v) ** 2)
    shx = (abs(P["upperarm_l"][0]) + abs(P["upperarm_r"][0])) / 2
    for s, o, sg, lv in (("l", "r", 1, 3.0), ("r", "l", -1, -3.0)):
        e, h = P[f"lowerarm_{s}"], P[f"hand_{s}"]
        add("elb_out", 4.0 * max(shx + WIDE["elb_out"] - sg * e[0], 0) ** 2)          # 1A: elbows past the torso width
        add("elb_h", 1.0 * (e[2] - (WIDE["elb_z"] + lv)) ** 2)                         # high on the chest, left above right
        add("level", 1.0 * (h[2] - e[2]) ** 2)                                         # forearms across, level
        add("cross", 2.0 * max(sg * h[0] + E("K8_CROSS", 0.0), 0) ** 2)                               # hand past the midline (crossed)
        fa = min(cl[f"lowerarm_{s}|spine_04"], cl[f"lowerarm_{s}|spine_05"], cl[f"lowerarm_{s}|spine_03"],
                 cl.get(f"lowerarm_{s}|lowerarm_{o}", cl.get(f"lowerarm_{o}|lowerarm_{s}", 99)))
        add("on_chest", 1.0 * max(fa - 2.5, 0) ** 2)                                  # forearm on the chest or on the other arm
        add("compact", 0.3 * max(e[1] - 22, 0) ** 2)
        fist = m[f"fist_{s}"]
        add("fist_up", 1.0 * max(fist[:, 2].max() - 4.0, 0) ** 2)                    # no chin guard
    if terms is not None:
        terms.update({k: round(v, 1) for k, v in T.items()})
    return sum(T.values())


def wide_fold(pose):
    F.cost = wide_cost
    init = {"arm_l": {"hand": [-9.0, 22.0, -4.0], "pole": [30.0, 16.0, -4.0]},
            "arm_r": {"hand": [9.0, 20.0, -10.0], "pole": [-30.0, 14.0, -10.0]}}
    with contextlib.redirect_stdout(io.StringIO()):
        p, J = F.fit(po, pose, init=init, iters=int(E("K8_ITERS", 80)))
    tm = {}; wide_cost(F.metrics(po, p)[0], False, tm)
    return p, J, tm, F.fit.last_x


def hump(p, shrug=None, neck=None):
    shrug = E("K8_SHRUG", 6.0) if shrug is None else shrug
    neck = E("K8_NECK", 7.0) if neck is None else neck
    q = K6.arms(p, shrug=shrug)
    q["head"] = [v if i == 0 else 0.0 for i, v in enumerate(q["head"])]       # centred: no yaw/roll
    return K6.head(q, neck=neck) if neck else q


# --- body: K7's (legs, pelvis-led tuck); key 5 removed (no press hold), key 9 added (last open frame)
body = {k: K7.BODY(K7.body[k]) for k in (0, 1, 2, 4, 7, 11)}
body[9] = A._lerp(body[7], body[11], 0.45)
FOOT = {4: 0.0, 7: E("K8_FOOT7", -4.0), 9: E("K8_FOOT", 0.0), 11: E("K8_FOOT", 0.0)}    # K7: -6 / -15 / -22 (pointed toes)
# round 2 (cloud review 1 FAIL posa, 2026-11-23): a11-a12 the shins fell straight under the tipped-back pelvis and the snap then swung
# them forward. The extra ball tilt is gone (pelvis = K7) and the feet come 6 cm forward in the pelvis frame at a12 (3 at a10): the
# shins point at the target like the a15 smear (shin fwd 0.83 vs K7 0.48) and the knees come 5 cm closer to the chest (tighter tuck).
PEL = {9: E("K8_PEL", 0.0) * 0.4, 11: E("K8_PEL", 0.0)}
LEGF = {9: E("K8_LEGF", 6.0) * 0.5, 11: E("K8_LEGF", 6.0)}
for k, d in LEGF.items():
    for s in "lr":
        f_ = body[k][f"leg_{s}"]["foot"]
        body[k][f"leg_{s}"]["foot"] = [f_[0], round(f_[1] + d, 3), f_[2]]
for k, fr in FOOT.items():
    for s in "lr":
        body[k][f"leg_{s}"]["foot_rot"] = [fr, 0.0, 0.0]
for k, d in PEL.items():
    body[k]["pelvis"]["rot"] = K6.add(body[k]["pelvis"]["rot"], [d, 0, 0])

k8 = {}
for k, (drift, pre) in {0: (0.0, 0.0), 1: (1.5, 0.0), 2: (3.0, 0.0), 4: (5.0, 0.0), 7: (7.5, 0.0), 9: (8.0, 1.5)}.items():
    k8[k] = goal_post(body[k], drift, pre)
# round 2 (review 1, minor): a0 -> a1 put the whole goal-post in one frame; a1 (key 0) is now halfway from the jump's wide arms (K7 key 0)
k8[0] = K6.arms_of(A._lerp(K7.k7[0], k8[0], E("K8_A1", 0.5)), k8[0])
fold_pose, FJ, FTERMS, FX = wide_fold(hump(K6.arms_of(K7.k7[11], body[11]), shrug=0.0))
k8[11] = hump(fold_pose, neck=0.0)          # shrug after the fit (the fit owns the clavicle yaw); the neck is already sunk
# a11 = key 10, the breakdown of the 2-frame close: elbows drop and come forward first, the fists swing down IN FRONT of the face
# (an arc, not the straight line through the head and chest that the plain blend 9 -> 11 takes)
BD = dict(ex=E("K8_BD_EX", 31.0), ey=E("K8_BD_EY", 10.0), ez=E("K8_BD_EZ", -2.0), hx=E("K8_BD_HX", 15.0), hy=E("K8_BD_HY", 30.0),
          hz=E("K8_BD_HZ", 4.0))
k8[10] = hump(A._lerp(goal_post(body[9]), k8[11], 0.5), shrug=0.0, neck=0.0)
for s, sg, dz in (("l", 1, E("K8_BD_DZ", 6.0)), ("r", -1, -E("K8_BD_DZ", 6.0))):
    set_arm(k8[10], s, [sg * BD["hx"], BD["hy"], BD["hz"] + dz], [sg * BD["ex"], BD["ey"], BD["ez"] + dz])
k8[14] = K7.k7[14]; k8[15] = K7.k7[15]
SPACING = {0: None, 1: "arrive_1_2", 2: "limb_led_recovery", 4: "limb_led_recovery", 7: "limb_led_recovery", 9: "hold_recoil", 10: "arrive_1_2",
           11: "charge_long", 14: "hold_recoil", 15: "hold_recoil"}
NAMES = {0: "goalpost_in", 1: "goalpost", 2: "goalpost_hold", 4: "goalpost_ball", 7: "goalpost_charge", 9: "goalpost_last",
         10: "close_breakdown", 11: "closed_doom", 14: "smear", 15: "thrust_end"}

if __name__ == "__main__":
    K7S = json.load(open(os.path.join(DK, "overlord_dropkick_startup_k7.json")))
    c = copy.deepcopy(K7S)
    c["move"] = "overlord_dropkick_startup_k8"
    for key in ("anim", "source_anim"):
        c[key] = c[key].replace("K7", "K8")
    c["author"]["take"] = c["author"]["take"].replace("K7", "K8")
    c["author"]["poses"] = {str(f): k8[f] for f in sorted(k8)}
    c["poses"] = [dict({"name": NAMES[f], "frame": f, "source": None}, **({"spacing": SPACING[f]} if SPACING[f] else {}))
                  for f in sorted(k8)]
    c["_changes"] = ("k8 (2026-11-23, owner: all Doom proposals): goal-post held a1-a10, 2-frame close a10-a12 into a HIGH WIDE fold "
                     "(elbows past the torso), shoulders shrugged around a sunk centred head, soles flat toward the target, ball +12 deg "
                     "back; legs = K7, snap a14-a15/settle/loop untouched. Previous: " + str(K7S.get("_changes", ""))[:160])
    out = os.path.join(DK, "overlord_dropkick_startup_k8.json")
    json.dump(c, open(out, "w"), indent=1)
    rows = [f"fit J={FJ:.2f} terms={FTERMS}"]
    for f in sorted(k8):
        n = MS.numbers(po, k8[f]); m = F.metrics(po, k8[f])[0]
        rows.append(f"k{f} " + " ".join(f"{a}={b:.1f}" for a, b in n.items()) + f" min_cl {min(m['cl'].values()):.2f}")
    ks = sorted(k8)
    for a, b in zip(ks, ks[1:]):
        for t in (0.25, 0.5, 0.75):
            m = F.metrics(po, A._lerp(k8[a], k8[b], t))[0]["cl"]; w = min(m, key=m.get)
            rows.append(f"path{a}-{b}@{t} min_cl {m[w]:.2f} {w}")
    rep = os.path.join(os.environ.get("TEMP", "."), "k8", "k8_report.txt")
    os.makedirs(os.path.dirname(rep), exist_ok=True)
    open(rep, "w").write("\n".join(rows) + "\n")
    print(out); print(rows[0][:300])
    print("worst: " + min(rows[1:], key=lambda r: float(r.split("min_cl ")[1].split()[0])))
    print(rep)
