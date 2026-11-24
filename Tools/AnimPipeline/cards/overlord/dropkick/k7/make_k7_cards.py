"""Dropkick K7 = K6 round 3 (2026-11-23, owner decision 1B): arms + a startup driven by the BODY + a tighter tuck/charge.
Owner scope: startup keys a0-a12 only. Untouched (identity / approved in the K6 reviews): snap a13-a15 (keys 14, 15), settle,
smug fall loop, legs together and straight from the strike, Big Band line, timeline and hitboxes.
Sources: review k6r2b "Cosa correggere" 1-3 and the Opus critique 2026-11-23 points 4-5 (docs/notes/reviews/dropkick-kick-k6/).
  1 wings HELD a1-a3 straight from the jump's wide arms (no gather into the guard first), press a4-a5.
  2 left fist beside the head, not in front of the face.
  3 release a8 with the elbows >= 6 cm outside the torso silhouette from behind (az180; measured by anim_silhouette_check).
  4 the pelvis leads the tuck from a1 (knees rising, trunk tipping): breakdown 70 % at a2, a3 is an arrival, not a pop.
  5 tight ball already at a5 (heels at the glutes, knees ~22 cm from the chest, toes pointed), compressing further to the maximum
    at a12, one frame before the snap.
Writes overlord_dropkick_startup_k7.json (settle and loop stay K6: setup_dropkick_kick_variant.py DROPKICK_KICK_SETTLE=K6).
Run (cwd Tools/AnimPipeline): python cards/overlord/dropkick/k7/make_k7_cards.py  -> prints the tuck and fold metrics per key.
"""
import contextlib, copy, io, json, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
DK = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(DK, "k6")); sys.path.insert(0, os.path.join(DK, "k5")); sys.path.insert(0, os.getcwd())
with contextlib.redirect_stdout(io.StringIO()):
    import make_k6_cards as K6                 # builds the K6 keys (no files written outside __main__)
from animpipe import author as A               # noqa: E402
import fold3 as F3                             # noqa: E402

ARM_KEYS = ("arm_l", "arm_r", "clavicle_l", "clavicle_r")
BODY = lambda p: {k: copy.deepcopy(v) for k, v in p.items() if k not in ARM_KEYS}


def with_arms(arms_src, body_src):
    return K6.arms_of(arms_src, body_src)


def legs(p, foot, pole, foot_rot):
    q = copy.deepcopy(p)
    for s in "lr":
        lg = q[f"leg_{s}"]
        lg["foot"] = [lg["foot"][0], foot[1], foot[2]]          # x (stance width) kept: legs stay together
        lg["pole"] = [lg["pole"][0], pole[1], pole[2]]
        lg["foot_rot"] = [foot_rot, 0.0, 0.0]
    return q


# --- 5: the ball. Legs in the pelvis frame (y forward, z up); K6 a12 had foot [5, 12, -13] (knees 29 cm from the chest).
BALL_A5 = dict(foot=(0, 10.0, -14.0), pole=(0, 61.0, 28.0), foot_rot=-6.0)       # tight already at a5
BALL_A12 = dict(foot=(0, 2.0, -6.0), pole=(0, 56.0, 46.0), foot_rot=-22.0)     # max compression one frame before the snap
CURL_A12 = -3.0                                                                 # trunk curls over the knees (spine x)
st = copy.deepcopy(K6.st)                       # keys 0 2 4 5 7 11 14 15; 14 and 15 are kept as they are
body = {k: BODY(st[k]) for k in st}
body[4] = legs(body[4], **BALL_A5)
body[5] = legs(body[5], **{k: (v if k != "foot_rot" else -9.0) for k, v in BALL_A5.items()})
# round-1 build: legs glued to the pelvis a7-a12 (co_motion): the charge compresses on every key (a5 ball, a8 tighter, a12 max)
mid = {k: (tuple((a + b) / 2 for a, b in zip(BALL_A5[k], BALL_A12[k])) if k != "foot_rot" else -15.0) for k in BALL_A5}
body[7] = legs(body[7], **mid)
body[11] = legs(body[11], **BALL_A12)
body[11]["spine"] = K6.add(body[11]["spine"], [CURL_A12, 0, 0])

# --- 4: the pelvis leads. a1/a2/a3 body = jump pose -> a5 ball at 25 / 70 / 92 % (breakdown 70 % at a2, a3 = arrival).
B0 = BODY(K6.S[0])
for k, t in ((0, 0.25), (1, 0.70), (2, 0.92)):
    body[k] = A._lerp(B0, body[4], t)

# --- 1-2: arms. Wings straight from the jump (no guard), held a1-a3 with a small drift; left fist BESIDE the head
# (out and back, chest frame: +x out for the left, +y forward), right fist out and low.
WING = K6.hands(K6.open_arms, l=(8.0, -7.0, 0.0), r=(-2.0, -2.0, 0.0))
WING_OPEN = K6.elbows(WING, out=2.5, down=-1.5)                  # a2: the wings keep opening a little (moving hold)
arms = {0: WING, 1: WING_OPEN, 2: WING_OPEN, 4: st[4], 5: st[5], 11: st[11]}
# --- 3: release a8 wider than K6 (out 16 -> 27, hands out 1 -> 5) so the elbows clear the torso from behind.
REL = dict(K6.REL, out=27.0, hout=5.0)
arms[7] = K6.hands(K6.arms(K6.elbows(K6.S[7], out=REL["out"], down=REL["down"], hout=REL["hout"], off=REL["off"]),
                           clav=REL["clav"], shrug=REL["shrug"], across=REL["across"], curl=REL["curl"]), l=(0, REL["hl_off"], 0))
# first UE dump (anim_silhouette_check az180): elbow_l 5.2 cm outside at a8 (target 6), the right one 8.5. The pole alone does not
# move it (swivel saturated): the left hand slides out along the right forearm and the shoulder drops back
arms[7] = K6.hands(arms[7], l=(float(os.environ.get("K7_REL_HL", 6.0)), 0, 0))
k7 = {k: with_arms(arms[k], body[k]) for k in arms}
k7[14] = st[14]; k7[15] = st[15]
SPACING = {1: "arrive_1_2", 2: "hold_recoil", 4: "arrive_1_2", 5: "hold_recoil", 7: "charge_long", 11: "charge_long"}

if __name__ == "__main__":
    K6S = json.load(open(os.path.join(DK, "overlord_dropkick_startup_k6.json")))
    sp = {q["frame"]: q.get("spacing") for q in K6S["poses"]}
    c = copy.deepcopy(K6S)
    c["move"] = "overlord_dropkick_startup_k7"
    for key in ("anim", "source_anim"):
        c[key] = c[key].replace("K6", "K7")
    c["author"]["take"] = c["author"]["take"].replace("K6", "K7")
    c["author"]["poses"] = {str(f): k7[f] for f in sorted(k7)}
    names = {q["frame"]: q["name"] for q in K6S["poses"]}
    c["poses"] = [dict({"name": names.get(f, f"k7_{f}"), "frame": f, "source": None},
                       **({"spacing": SPACING.get(f, sp.get(f))} if SPACING.get(f, sp.get(f)) else {})) for f in sorted(k7)]
    c["_changes"] = ("k7 (2026-11-23, owner 1B): wings held a1-a3 from the jump, fist beside the head, press a5, wider release a8; "
                     "pelvis leads the tuck (25/70/92 % at a1-a3), tight ball from a5, max compression a12, toes pointed. "
                     "Snap a14-a15, settle, loop untouched. Previous: " + str(K6S.get("_changes", ""))[:160])
    out = os.path.join(DK, "overlord_dropkick_startup_k7.json")
    json.dump(c, open(out, "w"), indent=1)
    po = A.Poser("along")
    rows = []
    for f in sorted(k7):
        P = po.solve(k7[f])[0]
        up = P["spine_05"] - P["pelvis"]; th = P["calf_l"] - P["thigh_l"]
        ang = np.degrees(np.arccos(np.dot(up, th) / np.linalg.norm(up) / np.linalg.norm(th)))
        rows.append(f"k{f} knee_chest {np.linalg.norm(P['calf_l'] - P['spine_05']):.1f} heel_glute "
                    f"{np.linalg.norm(P['foot_l'] - P['pelvis']):.1f} thigh_torso {ang:.0f} | " + F3.report(po, k7[f], f))
    ks = sorted(k7)
    for a, b in zip(ks, ks[1:]):
        for t in (0.25, 0.5, 0.75):
            rows.append(f"path{a}-{b}@{t} min_cl {min(F3.metrics(po, A._lerp(k7[a], k7[b], t))[0]['cl'].values()):.2f}")
    rep = os.path.join(os.environ.get("TEMP", "."), "k7", "k7_fold_report.txt")
    os.makedirs(os.path.dirname(rep), exist_ok=True)
    open(rep, "w").write("\n".join(rows) + "\n")
    print(out); print("worst clearance: " + min(rows, key=lambda r: float(r.rsplit(" ", 1)[1]))); print(rep)
