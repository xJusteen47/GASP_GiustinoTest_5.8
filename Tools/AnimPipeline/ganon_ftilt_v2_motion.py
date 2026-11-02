"""Ganondorf forward tilt v2. Entry rule (owner, 31/10, corrected): the move is entered FROM the upright standing idle (the
pose before frame 0, idle_points.json); frame 0 already shows a strong change into the move's own load (weight going onto
the support leg, hips turning, front heel up), and the recovery rises back to the upright idle by the FAF (anim 37).
Changes from v1 (owner, 31/10: "why is the rear leg bent like that?", "completely unbalanced
and weightless", "the guard you start from is always an idle where the character stands upright"):
  - frame 0 and the FAF target are the project's standing idle (M_Neutral_Stand_Idle_Loop frame 0, idle_points.json);
    the move's own sink, load and lean develop in the startup and recover by the FAF;
  - balance by construction: every frame the body (not the planted feet) shifts so the centre of mass sits on a target
    inside the support base; the weight goes over the support foot BEFORE the kicking foot leaves the ground (frame 4);
    on one foot it travels heel side (load) -> ball side (kick) inside that foot;
  - support knee 30-45 deg, pelvis 82-86 cm (v1: 61-65 cm, knee 89-99 deg: a near-kneel);
  - torso leans back further at contact (28 deg) against the kick, arms counter.
v1 notes follow.
Ganondorf forward tilt v1 (pilot), built from scratch on our rig (no blockout), from the jab v11 guard (the owner's guard:
wide low stance, torso toward profile, fists up, elbows down).

Reference: SSBU Ganondorf forward tilt (UFD: startup 10, active 10-12, total 39; refs/ufd/GanondorfFTilt.gif measured in
calibration_metrics.csv): a front-leg push kick. Weight rocks back onto the rear leg while the knee chambers high (G1-G7, the
foot goes back to about -30% of its path), a 1-frame snap to full extension (G8 -4% -> G9 100%), the leg stays out (~60-80%)
for about 9 frames, then the knee re-chambers and the foot comes down. Our windows (anim = G x 0.96): contact 9-11, hold to
19, re-chamber 23, landing 27, FAF 37 (0-based game frame 39), tail to the guard at 43.

3D adaptation (owner's notes for this move): kick line 20 deg outward of the facing so it leaves the silhouette from behind;
pelvis and torso turned toward profile and leaning back (counterbalance); wide low stance; elbows down; both arms
counterbalance away from the torso (lead arm down-back, rear arm forward-out); heavy weight from the impact spacing and the
hold; support foot planted, pivoting on its ball with the hips; kicking knee over the toes in the chamber; ankle natural
(heel leading, foot about perpendicular to the shin).

Keys are SPARSE (owner, 31/10): the poses below are keyed on KEYS on every layer, clamped Bezier between them (no
overshoot), linear on the 1-frame snap. The dense per-frame targets written next to them are a monotone-cubic preview used
for the clearance proxy only. Cascadeur space: X = character's left, Y up, Z forward. Kicking leg = LEFT (the lead leg)."""
import os, json, math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
N = 44                       # 0..43: FAF at 37, tail back to the guard at 43
FAF_K = 37
CONTACT = (9, 11)
# Keys: the same frames on EVERY layer. The rig's IK chains span layers (shoulder on Torso, elbow on Arm, fist on Hand;
# hip on Pelvis, knee on Leg, ankle on Foot), so a layer keyed on other frames than its chain partners zig-zags the joint
# between them. Sparse in the strike and the hold, every 2 frames where the body turns (wind-up, recovery).
KEYS = [0, 2, 4, 6, 8, 9, 10, 11, 13, 16, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 40, 43]
LINEAR = [[8, 9]]            # the snap: one frame, straight
CLAMPED = [[9, 10], [10, 11], [11, 13], [13, 16], [16, 19], [37, 40], [40, 43]]   # contact, hold, final settle: no overshoot
BEZIER = [[a, b] for a, b in zip(KEYS[:-1], KEYS[1:]) if [a, b] not in LINEAR + CLAMPED]
WEIGHTS = {}
LEG_WEIGHTS = {19: {"rw": 0.1}, 23: {"lw": 0.6}, 25: {"rw": 0.1}, 27: {"lw": 0.9}}   # the kicking leg's retraction and landing ease in
LAYERS = {"Foot_L": {"clamped": [[25, 27], [27, 29], [29, 31]], "weights": LEG_WEIGHTS}, "Leg_L": {"weights": LEG_WEIGHTS}}
if os.environ.get("DENSE"):   # dense variant: a key on every frame, linear (what Unreal receives anyway: the 60 fps bake)
    KEYS = list(range(44)); LINEAR = [[a, b] for a, b in zip(KEYS[:-1], KEYS[1:])]; CLAMPED = []; BEZIER = []; LAYERS = {}
# Overlap lives in the curves instead (frames of delay per body part, sampled between frames): the hips lead, the chest
# follows, the head and the rear arm trail; the lead arm swings with the hips through the snap and trails after it
CHEST_LAG = 1.3
HEAD_LAG = 3.0
ARM_LAG_R = 2.0
ARM_LAG_L = lambda k: 0.0 if k <= 12 else 2.0     # arms trail the leg-led recovery

ANK_ADJ = {}                          # {frame: (dx, dy, dz)} cm on the kicking ankle keys (auto_fix.py)
HEEL = {0: 10, 2: 11, 4: 10, 5: 15}   # v2 01/11: ankle 141-148 > 140 at f2-5 with 16/24/30 (Unreal bones ~1 deg under the model)   # deg, kicking heel up about the ball before the lift-off (6)
KICK_YAW = 20.0              # deg, kick line outward (toward +X, the lead side) from the facing
CONTACT_ANKLE_Y = 92.0       # cm, ankle height at contact (belly height of an opponent)
REACH = 0.985                # share of the leg's full length at contact (knee ~13 deg short of straight)

# ---- curves: {frame: value} ----
# pelvis HEIGHT only (cm from the idle); its horizontal place is solved per frame for balance (BAL_*)
PEL_Y = {0: -2, 2: -2.5, 4: -2.5, 6: -3, 8: -3, 9: -1, 11: -1, 19: -1.5, 23: -1.5, 26: -2.2, 28: -3.5, 30: -5, 32: -4, 34: -1.5, 37: 0, 43: 0}   # dip as the foot lands (two feet), rise after: the head's recovery arcs   # pre-flight: knee 46-50 on 24-26 with -3/-5   # first run: knee 46-47 deg
PEL_OFF = {k: (0, v, 0) for k, v in PEL_Y.items()}
PEL_YAW = {0: 6, 4: 10, 6: 14, 8: 18, 9: 26, 11: 26, 13: 25, 19: 22, 23: 14, 27: 6, 32: 0.5, 37: 0, 43: 0}
CHEST_YAW = {0: 0, 6: -3, 9: -6, 19: -5, 27: -1, 32: 0, 43: 0}          # shoulders a little behind the hips (separation)
LEAN = {0: -4, 4: -6, 6: -9, 8: -15, 9: -30, 11: -31, 13: -27, 19: -24, 23: -18, 27: -9, 32: -1, 35: 0, 43: 0}   # heavier lean back against the kick
SIDE = {0: 4, 4: 5, 9: 9, 19: 8, 23: 5, 27: 1.5, 32: 0, 43: 0}               # over the support leg (+ = toward -X, the right)
PIVOT = {0: 3, 4: 5, 6: 8, 9: 18, 19: 16, 23: 10, 27: 4, 32: 0, 43: 0}     # support foot about its ball, with the hips
# balance target: along the attack axis from the support foot's centre (cm, + = toward the target); weight blend from the
# idle's centre of mass (0) to the support foot (1) while both feet are down
BAL_S = {0: -5, 4: -5, 6: -6, 8: -4, 9: 4, 11: 4.5, 19: 3, 23: 0, 26: -1, 43: -1}
BAL_W = {0: 0.55, 1: 0.7, 2: 0.82, 3: 0.92, 4: 0.97, 5: 1, 6: 1, 26: 1, 27: 0.85, 29: 0.45, 32: 0.1, 34: 0, 43: 0}   # weight over the back foot before the toes leave (6)
W_AIR = {0: 0, 4: 0, 6: 1, 23: 1, 27: 0, 43: 0}                                   # the kicking foot rides the hips while up
# arms: from the idle's hanging arms; rise a little and open for balance in the load, counter at the snap, home by the FAF
ARM_L = {0: ("D", (2, 5, 3)), 4: ("D", (2, 6, 3)), 7: ("D", (2, 8, 4)), 8: ("D", (-2, 2, 6)), 9: ("A", (-10, -36, 24)), 11: ("A", (-11, -39, 25)),
         19: ("A", (-17, -41, 17)), 23: ("A", (-15, -39, 22)), 26: ("A", (-10, -36, 28)), 30: ("A", (-2, -40, 20)), 35: ("D", (0, 0, 0)), 43: ("D", (0, 0, 0))}
ARM_R = {0: ("D", (2, 3, -3)), 3: ("D", (3, 6, -4)), 6: ("D", (4, 10, -6)), 9: ("A", (12, -15, -43.5)), 11: ("A", (14, -15, -43)), 19: ("A", (13, -16, -43)),
         23: ("A", (12, -17, -43)), 25: ("A", (9.5, -26.5, -37)), 27: ("A", (7, -32, -33)), 30: ("A", (5.1, -37.8, -26.1)), 32: ("A", (3, -42, -18)), 36: ("D", (0, 0, 0)), 43: ("D", (0, 0, 0))}   # 23-31 ~46 cm from the shoulder: an arc
# rear arm almost straight out to the right in the hold (reach ~47.5 of 49 cm; it was 30-35, bent ~100 deg), then it drops
# before it bends back in (pre-flight 31/10: elbow up/out and chicken wing on 7-10 and 27-34)
POLE_L = {0: None, 8: None, 9: (-0.6, -0.75, 0.15), 11: (-0.7, -0.7, 0.15), 19: (-0.7, -0.7, 0.15), 26: (-0.85, -0.15, -0.45), 30: None, 43: None}   # None = the idle's
POLE_R = {0: None, 2: None, 5: (-0.3, -0.95, -0.05), 8: (-0.6, -0.8, 0.0), 19: (-0.7, -0.7, 0.0), 23: (0.0, -1.0, -0.25), 25: (-0.5, -0.85, -0.15), 31: (-0.8, -0.6, 0.0), 33: (-0.85, -0.5, 0.0), 36: None, 43: None}   # v2 01/11: hold back-down 45 deg (relaxed arm out to the side), down at 23 keeps the upper arm <= 50 deg on the drop   # sampled ARM_LAG_R late; back-down while it hangs   # sampled ARM_LAG_R late   # elbow down whenever the arm is raised past 40 deg (pre-flight 31/10)


# parameter overrides (auto_fix.py writes them; ganon_iter.sh passes MODEL_OVERRIDES): {name: value} replaces the
# module-level curve; frame keys come back from JSON as strings
_ovr = os.environ.get("MODEL_OVERRIDES")
if _ovr and os.path.exists(_ovr):
    for _k, _v in json.load(open(_ovr)).items():
        globals()[_k] = {int(f): v for f, v in _v.items()} if isinstance(_v, dict) else _v
    PEL_OFF = {k: (0, v, 0) for k, v in PEL_Y.items()}


def unit(v):
    v = np.asarray(v, dtype=float); n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def Rx(d):
    a = math.radians(d); c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])

def Ry(d):
    a = math.radians(d); c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]])      # +d turns +X toward +Z (left side forward = toward profile)

def Rz(d):
    a = math.radians(d); c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])      # +d tips +Y toward -X

def rot_between(a, b):
    a, b = unit(a), unit(b)
    v = np.cross(a, b); c = float(np.dot(a, b))
    if c < -0.9999:
        ax = unit(np.cross(a, [1.0, 0, 0]) if abs(a[0]) < 0.9 else np.cross(a, [0, 1.0, 0]))
        return 2 * np.outer(ax, ax) - np.eye(3)
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx * (1.0 / (1.0 + c))

def ik(S, H, l1, l2, pole_pt):
    d = np.linalg.norm(H - S); u = unit(H - S)
    w = pole_pt - S; w = unit(w - u * np.dot(w, u))
    if d >= l1 + l2 - 1e-3:
        return S + u * l1, w
    x = (l1 * l1 - l2 * l2 + d * d) / (2 * d); h = math.sqrt(max(l1 * l1 - x * x, 0.0))
    return S + u * x + w * h, w

def pchip(keys, n=N):
    """Monotone cubic (Fritsch-Carlson) through {frame: value}, sampled on 0..n-1: no overshoot between keys, like
    Cascadeur's clamped Bezier. Values may be scalars or vectors."""
    xs = np.array(sorted(keys), dtype=float)
    Y = np.array([np.atleast_1d(np.asarray(keys[int(x)], dtype=float)) for x in xs])
    out = np.zeros((n, Y.shape[1]))
    h = np.diff(xs); dlt = np.diff(Y, axis=0) / h[:, None]
    m = np.zeros_like(Y)
    for j in range(Y.shape[1]):
        d = dlt[:, j]
        mm = np.zeros(len(xs))
        for i in range(1, len(xs) - 1):
            if d[i - 1] * d[i] > 0:
                w1, w2 = 2 * h[i] + h[i - 1], h[i] + 2 * h[i - 1]
                mm[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
        m[:, j] = mm
    for f in range(n):
        if f <= xs[0]:
            out[f] = Y[0]; continue
        if f >= xs[-1]:
            out[f] = Y[-1]; continue
        i = int(np.searchsorted(xs, f) - 1)
        t = (f - xs[i]) / h[i]
        h00, h10, h01, h11 = 2 * t ** 3 - 3 * t ** 2 + 1, t ** 3 - 2 * t ** 2 + t, -2 * t ** 3 + 3 * t ** 2, t ** 3 - t ** 2
        out[f] = h00 * Y[i] + h10 * h[i] * m[i] + h01 * Y[i + 1] + h11 * h[i] * m[i + 1]
    return out


def main():
    idle = json.load(open(os.path.join(HERE, "idle_points.json")))["points"]
    G = {n: np.array(v, dtype=float) for n, v in idle.items()}
    names = sorted(G)
    piv = G["pelvis_MainPoint"].copy()
    PELV = ["pelvis_MainPoint", "pelvis_AdditionalPoint", "thigh_MainPoint_l", "thigh_MainPoint_r"]
    SP2 = ["spine_02_MainPoint", "spine_02_AdditionalPoint"]
    CHEST = ["spine_04_MainPoint", "spine_04_AdditionalPoint", "neck_01_MainPoint", "neck_01_AdditionalPoint",
             "clavicle_MainPoint_l", "clavicle_AdditionalPoint_l", "clavicle_MainPoint_r", "clavicle_AdditionalPoint_r",
             "upperarm_MainPoint_l", "upperarm_MainPoint_r"]
    HEAD = ["head_MainPoint", "head_DirectionPoint", "head_AdditionalPoint"]
    FOOT = {s: [f"foot_MainPoint_{s}", f"foot_Self0Point_{s}", f"ball_MainPoint_{s}", f"ball_DirectionPoint_{s}", f"ball_AdditionalPoint_{s}"] for s in "lr"}
    L_TH = {s: np.linalg.norm(G[f"calf_MainPoint_{s}"] - G[f"thigh_MainPoint_{s}"]) for s in "lr"}
    L_SH = {s: np.linalg.norm(G[f"foot_MainPoint_{s}"] - G[f"calf_MainPoint_{s}"]) for s in "lr"}
    L_UA = {s: np.linalg.norm(G[f"lowerarm_MainPoint_{s}"] - G[f"upperarm_MainPoint_{s}"]) for s in "lr"}
    L_FA = {s: np.linalg.norm(G[f"hand_MainPoint_{s}"] - G[f"lowerarm_MainPoint_{s}"]) for s in "lr"}

    def torso(D):
        sd = D["upperarm_MainPoint_l"] - D["upperarm_MainPoint_r"]; sd[1] = 0.0; sd = unit(sd)
        return np.cross(sd, [0.0, 1.0, 0.0]), np.array([0.0, 1.0, 0.0]), sd        # forward, up, left

    def to_t(D, v):
        f, u, l = torso(D); return np.array([np.dot(v, f), np.dot(v, u), np.dot(v, l)])

    def from_t(D, r):
        f, u, l = torso(D); return f * r[0] + u * r[1] + l * r[2]

    rel0 = {s: to_t(G, G[f"hand_MainPoint_{s}"] - G[f"upperarm_MainPoint_{s}"]) for s in "lr"}
    def guard_pole(s):
        S, E, H = G[f"upperarm_MainPoint_{s}"], G[f"lowerarm_MainPoint_{s}"], G[f"hand_MainPoint_{s}"]
        u = unit(H - S); p = E - S; return to_t(G, unit(p - u * np.dot(p, u)))
    pole0 = {s: guard_pole(s) for s in "lr"}

    def arm_curve(spec, s):
        out = {}
        for f, (kind, v) in spec.items():
            if kind == "D":
                out[f] = rel0[s] + np.array(v, dtype=float)
            elif kind == "A":
                out[f] = np.array(v, dtype=float)
        for f, (kind, v) in spec.items():
            if kind == "M":                                   # part of the way back from the previous key to the guard
                prev = max(k for k in out if k < f)
                out[f] = out[prev] + v * (rel0[s] - out[prev])
        return out

    C = {"off": pchip(PEL_OFF), "yaw": pchip(PEL_YAW)[:, 0], "cyaw": pchip(CHEST_YAW)[:, 0], "lean": pchip(LEAN)[:, 0],
         "side": pchip(SIDE)[:, 0], "pivot": pchip(PIVOT)[:, 0],
         "arm_l": pchip(arm_curve(ARM_L, "l")), "arm_r": pchip(arm_curve(ARM_R, "r")),
         "pole_l": pchip({f: (pole0["l"] if v is None else unit(v)) for f, v in POLE_L.items()}),
         "pole_r": pchip({f: (pole0["r"] if v is None else unit(v)) for f, v in POLE_R.items()})}

    def at(curve, t):
        """Curve value at a fractional frame (the per-part delays sample between frames)."""
        t = min(max(float(t), 0.0), N - 1.0); i = int(math.floor(t)); a = t - i
        return curve[i] if i >= N - 1 else curve[i] * (1 - a) + curve[i + 1] * a

    def body(k):
        """Pelvis, spine, chest, head at frame k (no limbs)."""
        D = {n: G[n].copy() for n in names}
        off = C["off"][k]; yaw = C["yaw"][k]
        tc, ts, th = k - CHEST_LAG, k - 0.5 * CHEST_LAG, k - HEAD_LAG        # chest, mid-spine and head trail the hips
        lean, side, cy, yaw_c = at(C["lean"], tc), at(C["side"], tc), at(C["cyaw"], tc), at(C["yaw"], tc)
        Rp = Rx(0.25 * at(C["lean"], k)) @ Ry(yaw)
        Rs = Rx(0.55 * at(C["lean"], ts)) @ Rz(0.5 * at(C["side"], ts)) @ Ry(at(C["yaw"], ts) + 0.5 * at(C["cyaw"], ts))
        Rc = Rx(lean) @ Rz(side) @ Ry(yaw_c + cy)
        for n in PELV:
            D[n] = piv + off + Rp @ (G[n] - piv)
        # the upper body's anchor trails the hips too (the spine bends: hips first, chest after), not only its rotation
        off_s, off_c = at(C["off"], k - 0.35 * CHEST_LAG), at(C["off"], k - 0.7 * CHEST_LAG)
        for n in SP2:
            D[n] = piv + off_s + Rs @ (G[n] - piv)
        for n in CHEST:
            D[n] = piv + off_c + Rc @ (G[n] - piv)
        th2 = k - 0.5 * (CHEST_LAG + HEAD_LAG)
        Rc2 = Rx(at(C["lean"], th2)) @ Rz(at(C["side"], th2)) @ Ry(at(C["yaw"], th2) + at(C["cyaw"], th2))
        hm = piv + at(C["off"], k - 0.9 * HEAD_LAG) + Rc2 @ (G["head_MainPoint"] - piv)
        Rh = Rx(0.4 * at(C["lean"], th)) @ Rz(0.4 * at(C["side"], th)) @ Ry(0.3 * (at(C["yaw"], th) + at(C["cyaw"], th)))   # upright, eyes on the target
        for n in HEAD:
            D[n] = hm + Rh @ (G[n] - G["head_MainPoint"])
        return D

    # ---- kicking leg: ankle targets at the curve keys (absolute), from the hip at that frame ----
    hip_at = {k: body(k)["thigh_MainPoint_l"] for k in range(N)}
    ga = G["foot_MainPoint_l"]
    H9 = hip_at[9]
    Lmax = L_TH["l"] + L_SH["l"]
    pitch = math.asin(max(-0.5, min(0.5, (CONTACT_ANKLE_Y - H9[1]) / (Lmax * REACH))))
    kdir = np.array([math.sin(math.radians(KICK_YAW)) * math.cos(pitch), math.sin(pitch), math.cos(math.radians(KICK_YAW)) * math.cos(pitch)])
    ANK = {0: ga + np.array([0, 2.0, -1]), 2: ga + np.array([0, 3, -1.5]), 4: ga + np.array([0, 4.5, -2.5]), 5: ga + np.array([0, 7, -4]), 6: hip_at[6] + np.array([12, -48, 8]), 7: hip_at[7] + np.array([15, -32, 20]), 8: hip_at[8] + np.array([17, -22, 32]),   # in line with the stance foot and the kick
           9: hip_at[9] + kdir * Lmax * REACH, 11: hip_at[11] + kdir * (Lmax * REACH - 1) + np.array([0, -0.5, 0]),
           13: hip_at[13] + kdir * (Lmax * REACH - 3) + np.array([0, -1.5, 0]), 19: hip_at[19] + kdir * (Lmax * REACH - 5) + np.array([0, -3, 0]),
           23: hip_at[23] + np.array([17, -30, 20]), 27: ga + np.array([0, 1.0, 0]),
           29: ga, 32: ga, 43: ga}
    ANK[20] = ANK[19] + np.array([0.0, -9.0, -3.0])                      # mostly down first (the shin swings about the knee)
    ANK[21] = 0.5 * (ANK[19] + ANK[23]) + np.array([0.0, -7.0, 2.0])     # the knee folds: the foot sweeps down, not a straight line
    ANK[25] = 0.5 * (ANK[23] + ANK[27]) + np.array([0.0, -1.5, -1.5])   # v2 01/11: rounds the 23-27 drop (radial wobble 4.1 cm on 22-28)
    ank = pchip(ANK)
    # knee bend direction (from the hip-ankle line): along the foot on the ground, up-forward in the air (knee over the toes)
    kb0 = unit(G["calf_LimbDir_l"] - G["calf_MainPoint_l"])
    KPOLE = {0: kb0, 4: unit(kb0 + np.array([0, 0.3, 0.15])), 5: unit(kb0 + np.array([0, 0.6, 0.3])), 6: unit([0.2, 0.8, 0.55]), 9: unit(0.15 * kb0 * np.array([1.0, 0.0, 1.0]) + np.array([0, 1.0, 0])), 19: unit(0.3 * kb0 * np.array([1.0, 0.0, 1.0]) + np.array([0, 1.0, 0])),   # one plane with the return
             23: unit(kb0 * np.array([1.0, 0.0, 1.0]) + np.array([0, 0.9, 0])), 27: kb0, 43: kb0}   # the leg turns back in on the way down: knee over the toes
    kpole = pchip(KPOLE)
    # foot frame: axis ankle -> ball and the outward lateral axis
    gax = unit(G["ball_MainPoint_l"] - G["foot_MainPoint_l"])
    glat = G["ball_AdditionalPoint_l"] - G["ball_MainPoint_l"]; glat = unit(glat - gax * np.dot(glat, gax))
    up = np.array([0.0, 1.0, 0.0])
    perp = unit(up - kdir * np.dot(up, kdir))
    a_hit = unit(math.cos(math.radians(15)) * perp + math.sin(math.radians(15)) * kdir)     # heel leading, foot ~perpendicular to the shin
    a_hold = unit(math.cos(math.radians(25)) * perp + math.sin(math.radians(25)) * kdir)
    lat_air = np.array([math.cos(math.radians(KICK_YAW)), 0.0, -math.sin(math.radians(KICK_YAW))])
    FAX = {0: unit([gax[0], -0.3, gax[2]]), 2: unit([gax[0], -0.4, gax[2]]), 4: unit([gax[0], -0.5, gax[2]]), 5: unit([gax[0], -0.7, gax[2]]), 6: unit([0.05, -0.35, 1.0]), 7: unit([0.06, -0.05, 1.0]), 8: unit([0.08, 0.1, 1.0]), 9: a_hit, 11: a_hit,
           13: a_hit, 19: a_hold, 23: unit([0.8 * gax[0], -0.25, gax[2]]), 27: gax, 43: gax}
    FLAT = {0: glat, 3: glat, 7: lat_air, 9: lat_air, 19: lat_air, 23: lat_air, 25: glat, 27: glat, 43: glat}
    # heel up about the ball (toes on the floor until the lift-off at 6). The FAX y values above were flatter than the
    # idle's foot (-0.41), so the ball rose 3-5 cm and the foot hovered (pre-flight, 31/10): roll the idle foot instead.
    gb = G["ball_MainPoint_l"]
    def roll(axis, d):
        a_ = unit(axis); c_, s_ = math.cos(math.radians(d)), math.sin(math.radians(d))
        K = np.array([[0, -a_[2], a_[1]], [a_[2], 0, -a_[0]], [-a_[1], a_[0], 0]])
        return np.eye(3) + s_ * K + (1 - c_) * K @ K
    sgn = 1.0 if (roll(glat, 10) @ (ga - gb))[1] > (ga - gb)[1] else -1.0
    for k_, th_ in HEEL.items():
        R_ = roll(glat, sgn * th_)
        ANK[k_] = gb + R_ @ (ga - gb); FAX[k_] = R_ @ gax
    # offsets from auto_fix.py / fix_rules (cm, Cascadeur axes) on existing or new ankle keys; empty by default
    ank0 = pchip(ANK)
    for k_, d_ in ANK_ADJ.items():
        ANK[int(k_)] = (ANK[int(k_)] if int(k_) in ANK else ank0[int(k_)]) + np.array(d_, dtype=float)
    ANK[25] = 0.5 * (ANK[23] + ANK[27]) + np.array([0.0, -1.5, -1.5])   # v2 01/11: rounds the 23-27 drop (radial wobble 4.1 cm on 22-28)
    ank = pchip(ANK)
    fax, flat = pchip(FAX), pchip(FLAT)

    out = {n: [None] * N for n in names}
    prox = []
    w_air = pchip(W_AIR)[:, 0]; bal_s = pchip(BAL_S)[:, 0]; bal_w = pchip(BAL_W)[:, 0]
    BODY = PELV + SP2 + CHEST + HEAD
    MASS = [("head", 0.081), ("trunk", 0.497), ("ua_l", 0.028), ("ua_r", 0.028), ("fa_l", 0.016), ("fa_r", 0.016), ("hd_l", 0.006),
            ("hd_r", 0.006), ("th_l", 0.10), ("th_r", 0.10), ("sh_l", 0.0465), ("sh_r", 0.0465), ("ft_l", 0.0145), ("ft_r", 0.0145)]

    def com(D):
        seg = {"head": D["head_MainPoint"] + np.array([0, 9.0, 0]), "trunk": 0.45 * D["pelvis_MainPoint"] + 0.55 * D["spine_04_MainPoint"]}
        for s_ in "lr":
            seg[f"ua_{s_}"] = 0.5 * (D[f"upperarm_MainPoint_{s_}"] + D[f"lowerarm_MainPoint_{s_}"])
            seg[f"fa_{s_}"] = 0.5 * (D[f"lowerarm_MainPoint_{s_}"] + D[f"hand_MainPoint_{s_}"])
            seg[f"hd_{s_}"] = D[f"hand_MainPoint_{s_}"]
            seg[f"th_{s_}"] = 0.57 * D[f"thigh_MainPoint_{s_}"] + 0.43 * D[f"calf_MainPoint_{s_}"]
            seg[f"sh_{s_}"] = 0.57 * D[f"calf_MainPoint_{s_}"] + 0.43 * D[f"foot_MainPoint_{s_}"]
            seg[f"ft_{s_}"] = 0.5 * (D[f"foot_MainPoint_{s_}"] + D[f"ball_MainPoint_{s_}"])
        return sum(m * seg[n] for n, m in MASS) / sum(m for _, m in MASS)

    def foot_centre(D):
        an, ba = D["foot_MainPoint_r"], D["ball_MainPoint_r"]
        d = ba - an; d[1] = 0.0
        return an + 0.30 * d                 # between the heel (0.45 behind the ankle) and the toe (0.35 past the ball)

    corr_hist = {}

    def corr_at(t):
        if t <= 0:                           # before frame 0 the lagged parts hold frame 0's shift (not the idle's)
            return corr_hist.get(0, np.zeros(3))
        i = int(math.floor(t)); a_ = t - i
        c_i = corr_hist.get(i, np.zeros(3)); c_j = corr_hist.get(i + 1, c_i)
        return c_i * (1 - a_) + c_j * a_

    def compose(k, corr):
        D = body(k)
        lagged = {**{n: corr for n in PELV}, **{n: corr_at(k - 0.35 * CHEST_LAG) if k > 0 else corr for n in SP2},
                  **{n: corr_at(k - 0.7 * CHEST_LAG) if k > 0 else corr for n in CHEST}, **{n: corr_at(k - 0.9 * HEAD_LAG) if k > 0 else corr for n in HEAD}}
        for n in BODY:
            D[n] = D[n] + lagged[n]           # always lagged (a switch at k = 0.9 x HEAD_LAG made the head jump on 2-3)
        # ---- arms (hand from its shoulder in the torso frame, elbow by IK with a pole in the torso frame) ----
        for s in "lr":
            S = D[f"upperarm_MainPoint_{s}"]
            ta = k - (ARM_LAG_L(k) if s == "l" else ARM_LAG_R)
            hand = S + from_t(D, at(C[f"arm_{s}"], ta))
            pole = from_t(D, unit(at(C[f"pole_{s}"], ta)))
            el, bend = ik(S, hand, L_UA[s], L_FA[s], S + pole * 30.0)
            Rua = rot_between(G[f"lowerarm_MainPoint_{s}"] - G[f"upperarm_MainPoint_{s}"], el - S)
            Rfa = rot_between(G[f"hand_MainPoint_{s}"] - G[f"lowerarm_MainPoint_{s}"], hand - el)
            D[f"lowerarm_MainPoint_{s}"] = el
            D[f"lowerarm_AdditionalPoint_{s}"] = el + Rua @ (G[f"lowerarm_AdditionalPoint_{s}"] - G[f"lowerarm_MainPoint_{s}"])
            D[f"lowerarm_LimbDir_{s}"] = el + bend * np.linalg.norm(G[f"lowerarm_LimbDir_{s}"] - G[f"lowerarm_MainPoint_{s}"])
            D[f"hand_MainPoint_{s}"] = hand
            for n in (f"hand_DirectionPoint_{s}", f"hand_AdditionalPoint_{s}"):
                D[n] = hand + Rfa @ (G[n] - G[f"hand_MainPoint_{s}"])
        # ---- kicking leg (left) ----
        Hh = D["thigh_MainPoint_l"]; A = ank[k].copy() + w_air[k] * corr
        if np.linalg.norm(A - Hh) > Lmax * 0.995:
            A = Hh + unit(A - Hh) * Lmax * 0.995
        mid = 0.5 * (Hh + A)
        kn, bend = ik(Hh, A, L_TH["l"], L_SH["l"], mid + unit(kpole[k]) * 40.0)
        Rsh = rot_between(G["foot_MainPoint_l"] - G["calf_MainPoint_l"], A - kn)
        D["calf_MainPoint_l"] = kn
        D["calf_AdditionalPoint_l"] = kn + Rsh @ (G["calf_AdditionalPoint_l"] - G["calf_MainPoint_l"])
        D["calf_LimbDir_l"] = kn + bend * np.linalg.norm(G["calf_LimbDir_l"] - G["calf_MainPoint_l"])
        a1 = unit(fax[k]); l1 = flat[k] - a1 * np.dot(flat[k], a1); l1 = unit(l1)
        F0 = np.stack([gax, glat, np.cross(gax, glat)], axis=1); F1 = np.stack([a1, l1, np.cross(a1, l1)], axis=1)
        Rf = F1 @ F0.T
        for n in FOOT["l"]:
            D[n] = A + Rf @ (G[n] - G["foot_MainPoint_l"])
        # ---- support leg (right): foot pivots on its ball with the hips, knee over the toes ----
        bp = G["ball_MainPoint_r"]
        for n in FOOT["r"]:
            D[n] = bp + Ry(C["pivot"][k]) @ (G[n] - bp)
        Hr = D["thigh_MainPoint_r"]; Ar = D["foot_MainPoint_r"]
        fdir = D["ball_MainPoint_r"] - Ar; fdir[1] = 0.0; fdir = unit(fdir)
        knr, bendr = ik(Hr, Ar, L_TH["r"], L_SH["r"], 0.5 * (Hr + Ar) + fdir * 40.0 + np.array([0.0, 5.0, 0.0]))
        Rshr = rot_between(G["foot_MainPoint_r"] - G["calf_MainPoint_r"], Ar - knr)
        D["calf_MainPoint_r"] = knr
        D["calf_AdditionalPoint_r"] = knr + Rshr @ (G["calf_AdditionalPoint_r"] - G["calf_MainPoint_r"])
        D["calf_LimbDir_r"] = knr + bendr * np.linalg.norm(G["calf_LimbDir_r"] - G["calf_MainPoint_r"])
        return D

    com0 = com(compose(0, np.zeros(3)))
    bal_log = []
    for k in range(N):
        corr = np.zeros(3)
        for it in range(8):
            D = compose(k, corr)
            c = com(D)
            fc = foot_centre(D) + np.array([0.0, 0.0, bal_s[k]])
            tgt = (1 - bal_w[k]) * com0 + bal_w[k] * fc
            d = tgt - c; d[1] = 0.0
            corr = corr + d
            if np.linalg.norm(d) < 0.05:
                break
        corr_hist[k] = corr.copy()
    # the per-frame solve wobbles a little through the delayed chest/head terms: smooth the shift (keeping the snap 8->9
    # and the contact frames as solved), then compose every frame once more with the smoothed shift
    raw = {k: corr_hist[k].copy() for k in range(N)}
    for _ in range(2):
        prev = {k: corr_hist[k].copy() for k in range(N)}
        for k in range(1, N - 1):
            if k in (8, 9, 10):
                continue
            corr_hist[k] = 0.25 * prev[k - 1] + 0.5 * prev[k] + 0.25 * prev[k + 1]
    for k in range(N):
        D = compose(k, corr_hist[k])
        bal_log.append((k, round(float(corr_hist[k][0]), 1), round(float(corr_hist[k][2]), 1)))
        for n in names:
            out[n][k] = D[n]
        # ---- clearance proxy: forearms/hands against the torso, kicking shin/foot against the support leg ----
        def segd(a0, a1_, b0, b1):
            best = 1e9
            for t in np.linspace(0, 1, 13):
                p = a0 + (a1_ - a0) * t; ab = b1 - b0
                u = float(np.clip(np.dot(p - b0, ab) / max(np.dot(ab, ab), 1e-9), 0, 1))
                best = min(best, float(np.linalg.norm(p - (b0 + ab * u))))
            return best
        tor = [D["pelvis_MainPoint"], D["spine_02_MainPoint"], D["spine_04_MainPoint"], D["neck_01_MainPoint"]]
        worst = (1e9, "")
        for s in "lr":
            for i in range(3):
                d = segd(D[f"lowerarm_MainPoint_{s}"], D[f"hand_MainPoint_{s}"], tor[i], tor[i + 1]) - 4.75 - 12.5
                if d < worst[0]:
                    worst = (d, f"fore_{s}-torso{i}")
        d = segd(D["calf_MainPoint_l"], D["foot_MainPoint_l"], D["thigh_MainPoint_r"], D["calf_MainPoint_r"]) - 6 - 7
        if d < worst[0]:
            worst = (d, "shin_l-thigh_r")
        d = segd(D["calf_MainPoint_l"], D["foot_MainPoint_l"], D["calf_MainPoint_r"], D["foot_MainPoint_r"]) - 6 - 6
        if d < worst[0]:
            worst = (d, "shin_l-shin_r")
        prox.append((round(worst[0], 1), worst[1]))
    print("proxy clearance per frame (cm, approx):", [p[0] for p in prox])
    print("worst proxy:", sorted(prox)[:4])
    print("balance shift per frame (x, z cm):", bal_log)
    # report: kicking ankle progress along guard -> contact, its height, pelvis and lean
    d = ank[9] - ga
    print(" k  ankle%  ankle_y  ball_y  pel_off              yaw   lean")
    for k in range(N):
        e = 100 * np.dot(out["foot_MainPoint_l"][k] - ga, d) / np.dot(d, d)
        print(f"{k:2d} {e:7.1f} {out['foot_MainPoint_l'][k][1]:7.1f} {out['ball_MainPoint_l'][k][1]:7.1f}  {np.round(C['off'][k], 1)}  {C['yaw'][k]:5.1f} {C['lean'][k]:6.1f}")
    res = {"frames": list(range(N)), "targets": {n: [out[n][k].tolist() for k in range(N)] for n in names}, "feet": {},
           "keyplan": {"keys": KEYS, "linear": LINEAR, "bezier": BEZIER, "layers": LAYERS, "weights": {str(k): v for k, v in WEIGHTS.items()}, "kind": "clamped"},
           "move": {"contact": list(CONTACT), "faf": FAF_K, "kick_dir_cascadeur": kdir.tolist()}}
    json.dump(res, open(os.path.join(HERE, "ganon_v2_targets_raw.json"), "w"))
    print("written ganon_v2_targets_raw.json")

if __name__ == "__main__":
    main()
