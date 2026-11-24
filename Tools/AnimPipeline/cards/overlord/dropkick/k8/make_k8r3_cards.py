"""Dropkick K8 round 3 (2026-11-24): the Doom poses of K8 r2 with the motion fixed. Owner: K7 and K8 r2 are jerky and unnatural.
Cause (dropkick-k8-3-diagnosi.md, slowmo look dropkick-k7-k8-sguardo-slowmo.md): every card key is a stop (flat tangents of the
weighted spacing profiles), K8's hold had near-identical keys (5 still frames a1-a5), the take-off and the close happened in ONE frame,
and all parts arrived on the same frame. K7 stays locked; this is a new variant K8R3 (its own assets), K8 r2 stays as it is.
Changes, identity and silhouette unchanged (goal-post open, wide high fold, shrug + centred head, flat soles, snap/settle/loop K7):
  1 take-off over 3 frames: a1 35 %, a2 75 % from the jump's wide arms (K7 key 0) to the goal-post, a3 arrival.
  2 a LIVING hold: goal-post keys that really differ (>= 6 cm on the fists): a3 arrive (fists at head height), a5 puff up (elbows up and
    back, fists high and out), a8 tension (elbows highest, fists turn in), a9 pull back (elbows and fists back = anticipation).
  3 close over 3 frames: a10 breakdown (elbows drop forward, fists in front of the face), a11 overshoot past the fold (forearms cross
    2 cm deeper), a12 the fold. Legs = K8 r2 (feet forward at a10-a12).
  4 overlap: card part_delays head 1, forearm 1, hand 1 frame (the arms and head arrive after the trunk).
  5 spacing: every key up to a11 passes through (limb_led_recovery = automatic tangents, no stop); a12 charge_long into the snap;
    a14-a15 unchanged. The build must sample every frame (build thread) before this is captured.
Writes overlord_dropkick_startup_k8r3.json. Run (cwd Tools/AnimPipeline): python cards/overlord/dropkick/k8/make_k8r3_cards.py
"""
import contextlib, copy, io, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DK = os.path.dirname(HERE)
sys.path.insert(0, HERE)
with contextlib.redirect_stdout(io.StringIO()):
    import make_k8_cards as M8               # r2 poses: body (legs fix), fold (k8[11]), helpers
A, K6, K7, F, MS = M8.A, M8.K6, M8.K7, M8.F, M8.MS
po = M8.po
lerp = A._lerp

# goal-post family (anatomical chest frame, left side; the right side mirrors x with its own offsets: no twinning)
GPS = {  # key: elbow (x, y, z), fist offset over the elbow (dx, dy, dz), clavicle (shrug, back)
    2: dict(el=(38.0, -4.0, -2.0), fist=(1.0, 3.0, 20.0), clav=(1.0, 4.0)),     # a3 arrive: fists at head height
    4: dict(el=(43.0, -9.0, 4.0), fist=(4.0, 2.0, 26.0), clav=(4.0, 6.0)),      # a5 puff up
    7: dict(el=(40.0, -12.0, 8.0), fist=(-4.0, 4.0, 24.0), clav=(6.0, 7.0)),    # a8 tension: elbows highest, fists turn in
    8: dict(el=(44.0, -15.0, 5.0), fist=(2.0, -8.0, 22.0), clav=(5.0, 9.0)),    # a9 pull back: anticipation of the close
}
RIGHT = dict(el=(1.5, 1.0, -2.5), fist=(-1.0, 0.0, -3.0))                     # right side: a bit lower and in


def gp(pose, k):
    q = copy.deepcopy(pose); g = GPS[k]
    for s, sg in (("l", 1), ("r", -1)):
        el = [g["el"][0] + (RIGHT["el"][0] if s == "r" else 0), g["el"][1] + (RIGHT["el"][1] if s == "r" else 0),
              g["el"][2] + (RIGHT["el"][2] if s == "r" else 0)]
        fi = [g["fist"][i] + (RIGHT["fist"][i] if s == "r" else 0) for i in range(3)]
        M8.set_arm(q, s, [sg * (el[0] + fi[0]), el[1] + fi[1], el[2] + fi[2]], [sg * el[0], el[1], el[2]])
        q[f"clavicle_{s}"] = [0.0, round(-sg * g["clav"][0], 2), round(-sg * g["clav"][1], 2)]
    return q


body = copy.deepcopy(M8.body)                       # keys 0 1 2 4 7 9 11 (r2: legs forward at 9 and 11)
B7, B11 = body[7], body[11]
body[8] = lerp(B7, B11, 0.25); body[9] = lerp(B7, B11, 0.5); body[10] = lerp(B7, B11, 0.75)
FOLD = M8.k8[11]
r3 = {}
r3[2] = gp(body[2], 2)
JUMP = K6.arms_of(K7.k7[0], body[0])                # the jump's wide arms (where the montage starts from)
r3[0] = K6.arms_of(lerp(JUMP, gp(body[0], 2), 0.35), body[0])
r3[1] = K6.arms_of(lerp(K6.arms_of(K7.k7[0], body[1]), gp(body[1], 2), 0.75), body[1])
r3[4] = gp(body[4], 4); r3[7] = gp(body[7], 7); r3[8] = gp(body[8], 8)
# a10 breakdown: elbows drop and come forward, fists swing down in front of the face (arc, not through the head)
r3[9] = M8.hump(K6.arms_of(lerp(r3[8], FOLD, 0.4), body[9]), shrug=0.0, neck=0.0)
BD = (float(os.environ.get('K8R3_BD_HX', 30.0)), float(os.environ.get('K8R3_BD_HY', 16.0)), float(os.environ.get('K8R3_BD_HZ', 18.0)))
for s, sg, dz in (("l", 1, 4.0), ("r", -1, -4.0)):
    M8.set_arm(r3[9], s, [sg * BD[0], BD[1], BD[2] + dz], [sg * 38.0, 2.0, 1.0 + dz])   # halfway on the arc: elbows still wide, fists over the shoulders
# a11 overshoot: the fold's arms crossing 2 cm deeper and the shoulders 2 deg further in, then a12 settles into the fold
r3[10] = K6.arms(K6.arms_of(FOLD, body[10]), clav=1.0, across=float(os.environ.get('K8R3_OVER', 1.0)))
r3[11] = FOLD
r3[14] = K7.k7[14]; r3[15] = K7.k7[15]
SP = "limb_led_recovery"
SPACING = {0: SP, 1: SP, 2: SP, 4: SP, 7: SP, 8: SP, 9: SP, 10: SP, 11: "charge_long", 14: "hold_recoil", 15: "hold_recoil"}
NAMES = {0: "takeoff_35", 1: "takeoff_75", 2: "goalpost_arrive", 4: "goalpost_puff", 7: "goalpost_tension", 8: "goalpost_pullback",
         9: "close_breakdown", 10: "close_overshoot", 11: "closed_doom", 14: "smear", 15: "thrust_end"}

if __name__ == "__main__":
    K8S = json.load(open(os.path.join(DK, "overlord_dropkick_startup_k8.json")))
    c = copy.deepcopy(K8S)
    c["move"] = "overlord_dropkick_startup_k8r3"
    for key in ("anim", "source_anim"):
        c[key] = c[key].replace("StartupK8", "StartupK8R3")
    c["author"]["take"] = c["author"]["take"].replace("StartupK8", "StartupK8R3")
    c["author"]["poses"] = {str(f): r3[f] for f in sorted(r3)}
    c["poses"] = [{"name": NAMES[f], "frame": f, "source": None, "spacing": SPACING[f]} for f in sorted(r3)]
    c["part_delays"] = {"chest": 0, "head": 1, "forearm": 1, "hand": 1}
    c["_changes"] = ("k8r3 (2026-11-24, owner: K7/K8 r2 jerky): take-off over 3 frames, living goal-post hold (4 distinct keys), close "
                     "over 3 frames with overshoot, arms/head 1 frame behind, no stop on interior keys. Previous: "
                     + str(K8S.get("_changes", ""))[:160])
    out = os.path.join(DK, "overlord_dropkick_startup_k8r3.json")
    json.dump(c, open(out, "w"), indent=1)
    rows = []
    for f in sorted(r3):
        m = F.metrics(po, r3[f])[0]
        P, _ = po.solve(r3[f])
        rows.append(f"k{f} hand_l {[round(float(x)) for x in P['hand_l']]} min_cl {min(m['cl'].values()):.2f}")
    ks = sorted(r3)
    for a, b in zip(ks, ks[1:]):
        Pa, _ = po.solve(r3[a]); Pb, _ = po.solve(r3[b])
        d = max(float(((Pa[x] - Pa['spine_05']) - (Pb[x] - Pb['spine_05'])) @ ((Pa[x] - Pa['spine_05']) - (Pb[x] - Pb['spine_05']))) ** 0.5
                for x in ("hand_l", "hand_r", "lowerarm_l", "lowerarm_r"))
        cl = min(min(F.metrics(po, lerp(r3[a], r3[b], t))[0]["cl"].values()) for t in (0.25, 0.5, 0.75))
        rows.append(f"seg {a}-{b} arm_travel_cm {d:.1f} per_frame {d / (b - a):.1f} path_min_cl {cl:.2f}")
    rep = os.path.join(os.environ.get("TEMP", "."), "k8", "k8r3_report.txt")
    os.makedirs(os.path.dirname(rep), exist_ok=True)
    open(rep, "w").write("\n".join(rows) + "\n")
    print(out); print(rep)
