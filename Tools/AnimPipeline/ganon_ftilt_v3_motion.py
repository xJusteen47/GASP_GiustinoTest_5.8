"""Ganondorf forward tilt v3: built from the owner's 5 reference frames (refs/owner_ganon_ftilt, keyposes.json), KEY POSES
FIRST (README "Replicating a reference"). v2 was rejected (01/11: "the arm movement makes no sense and the legs don't have
the right anticipation"): it took only the extension/timing curve from the reference, never its poses.

How the poses are made: the kicking leg and both arms are keyed as SIDE-VIEW ANGLES (deg in the kick plane: 0 = along the
attack, 90 = up, -90 = down) read from keyposes.json, plus an out-of-plane angle; so the 2D side projection the gate compares
is the reference's by construction, and our rules (balance, support knee, counter-lean) only adjust it.
  reference (deg)         first   anticipation   active   recovery   last
  thigh / shin (kick)     -6/-89    83/-37        7/-4     5/-115    -6/-112
  front arm ua / fa       -48/-8    16/9         -68/ -    -87/-13   -78/-48
  rear arm ua / fa       -117/-72  -65/-31      -120/-92  -147/-87  -146/-101
  torso                   107      122           135       82        74
  support hip height (body lengths over the support ankle): 0.64, 0.70, 0.57, 0.40 (deep wide crouch), 0.51 (low guard)

Timing (UFD: startup 10, active 10-12, total 39; anim = G x 0.96; refs/ufd/GanondorfFTilt_sheet.png): frame 0 is the
reference's first pose (knee already lifting, arms out); the knee chambers to the chest and the support leg extends 0-6
(the wind-up), hold 6-7, 8 starts the throw-back, 8->9 the 1-frame snap to full extension with the torso thrown back,
contact 9-11, hold to 20 with the knee giving a little, retract 21-22, the foot lands at 24 into the deep wide crouch (25),
rises to the low guard and is IN it at the FAF (37): the owner's 5th frame is the functional last frame (owner, 01/11 14:24Z),
so every part arrives by 37 (pelvis 34, chest ~35, arms 36, head 37 through their lags) and nothing rises toward the idle
before the FAF. The clip ends ON the FAF (frames 0-37): the return to the idle is the game's generic blend, which depends on the idle
being played (owner, 01/11 14:41Z), so no authored tail.

Support leg: its pelvis height is SOLVED each frame from a support-knee curve (SUP_KNEE, flexion deg) and the hips' place,
so the knee stays in range on one foot whatever the balance shift does. Balance: the body shifts so the centre of mass sits
on a target: over the support foot in the wind-up, ahead of it in the strike (the reference is off balance there: declared
lunge frames), between the feet after the landing (BAL_F = front-foot share).
v3.1 (owner 01/11 15:37Z "Vai", after "the neck detaches from the head" and "how do we stand on the torso rotation"):
- trunk as a chain (each segment turns about its parent's joint; the lagged balance shift becomes a capped aim per segment,
  LAG_AIM_*, faded into the settle by AIM_FADE): every length stays the idle's (bone_lengths.py);
- twist chain: pelvis yaw 5 -> 33 deg peaking at 6.7, chest 1.2 frames behind (CHEST_LAG) with a counter-twist to -17 deg
  (chest vs pelvis down to -23), the kicking leg's snap 1 frame after the chest; the shoulders close 31% first -> active
  (reference 31%) and reopen in the recovery (twist_chain.py);
- pronation on both forearms (PRON_L/R, up to 40 deg on the lead arm in the strike);
- deeper, wider crouch: support hip 0.41 body lengths at the recovery pose and 0.49 at the FAF (reference 0.40 / 0.51),
  feet 62 cm apart (LAND_SPOT 12 cm out), support knee to 120 deg;
- rear arm swing a little wider (curves gate: 0.57x -> 0.6x+ the reference).
Cascadeur space: X = character's left, Y up, Z forward. Kicking leg = LEFT."""
import os, json, math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
N = 38                       # 0..37: the clip ends ON the FAF pose; the game's generic blend takes it to whichever idle
FAF_K = 37
CONTACT = (9, 11)
LAND = 24                    # the kicking foot is planted from here
KEYS = list(range(N)); LINEAR = [[a, b] for a, b in zip(KEYS[:-1], KEYS[1:])]; CLAMPED = []; BEZIER = []; LAYERS = {}; WEIGHTS = {}
KICK_YAW = 20.0              # deg, kick plane outward (toward +X, the lead side) from the facing

import sys as _sys
LEGACY_TRUNK = "--legacy-trunk" in _sys.argv     # the first v3 delivery's trunk mechanics (stretched), for comparison only;
                                                 # its exact points are kept as ganon_v3leg_targets_raw.json in the working folder
OUT = next((a.split("=", 1)[1] for a in _sys.argv if a.startswith("--out=")), "ganon_v3_targets_raw.json")
CHEST_LAG = 1.2
LAG_AIM_GAIN = 0.7   # share of the lagged balance shift turned into each trunk segment's aim (hierarchical trunk)
LAG_AIM_GAIN_REC = 1.5   # the same after the landing (the crouch's drop), tuned on its own
LAG_AIM_MAX_REC = 14
AIM_FADE = (30, 33)   # the aims fade out into the settle: fed back through the balance solve they pop the last frames
LAG_AIM_MAX_DEG = 10   # cap per segment and frame: the strike's shift is ~20 cm a frame, a full aim tips the trunk 60 deg
HEAD_LAG = 3.0
ARM_LAG = 1.0
FA_LAG = 0.0                 # the forearm trails the upper arm (overlap down the arm chain)
ARM_AXIS_BLEND = {"l": 0.5, "r": 0.85}        # arms swing in a plane between the kick plane (0) and the chest's own forward (1): the chest is
                             # turned ~40 deg toward profile, so a swing in the kick plane goes out SIDEWAYS from the chest
                             # (flared, raised elbow); halfway keeps cos(19 deg) of it in the side view

# ---- kicking leg: (thigh, shin) side-view angles, deg; until the landing ----
LEG = {0: (-8, -88), 3: (45, -56), 6: (80, -37), 7: (81, -36), 8: (72, -16),
       9: (7, -4), 10: (6, -6), 11: (5, -8), 13: (2, -18), 16: (1, -22), 18: (5, -30), 19: (9, -40), 20: (-5, -45), 21: (-15, -62), 22: (-28, -72), 23: (-42, -78)}
LEG_OUT = {0: 4, 9: 2, 20: 2, 23: 4}                       # knee out of the kick plane (deg, outward)
FOOT_REL = {0: 52, 8: 60, 9: 72, 20: 70, 22: 58, 23: 62}   # foot axis (ankle -> ball) minus shin angle; idle 66 (neutral)
LAND_BLEND = {21: 0.1, 22: 0.4, 23: 0.8}   # share of the landing spot reached before the touchdown (fast out of the hold, slow in to the floor)
LAND_SPOT = (12.0, 0.0, 22.0)   # cm from the idle left ankle: ~0.55 body lengths ahead of the support

# ---- arms: (upper arm, forearm) side-view angles, and out-of-plane angles (outward +) ----
ARM_L = {0: (-48, -8), 2: (-30, -18), 4: (-2, 4), 6: (16, 16), 8: (8, 12), 9: (-68, -52), 11: (-70, -54), 20: (-72, -56), 23: (-82, -40), 25: (-87, -13), 33: (-81, -42), 35: (-78, -48)}
ARM_L_OUT = {0: (15, 10), 6: (18, 4), 9: (20, 15), 25: (25, 15), 35: (20, 10)}
# rear arm: the reference's is ~1.5x longer in 2D than ours (the near-side arm, the gauntlet), so its swing is a little
# exaggerated (forward in the wind-up, back in the strike) to keep the amplitude near the reference, and kept close to the
# side plane (out-of-plane shortens it further)
ARM_R = {0: (-117, -72), 3: (-82, -42), 6: (-42, -6), 8: (-54, -20), 9: (-130, -104), 11: (-130, -104), 20: (-128, -98), 23: (-142, -94), 25: (-152, -90), 33: (-150, -98), 35: (-148, -103)}
ARM_R_OUT = {0: (6, 10), 6: (5, 8), 9: (14, 16), 20: (12, 16), 25: (8, 12), 35: (6, 10)}
# forearm roll (deg about the forearm axis, + = pronation: the thumb side turns in and down), sampled with the forearm. The
# solver puts it in the hand bone and the twist bones spread it along the forearm. v3 had none (the hand only followed the
# forearm's swing by the smallest rotation)
PRON_L = {0: 0, 6: -10, 9: 35, 11: 40, 20: 35, 25: 15, 34: 10}
PRON_R = {0: 0, 9: 20, 20: 20, 34: 5}

# ---- body ----
LEAN = {0: -16, 3: -24, 6: -31, 7: -32, 8: -36, 9: -44, 11: -45, 20: -40, 22: -25, 24: -8, 26: 4, 28: 11, 31: 14, 33: 16}   # deg, - = back
PEL_YAW = {0: 10, 4: 6, 5: 5, 6: 9, 7: 20, 8: 28, 9: 32, 11: 33, 20: 30, 24: 26, 33: 23}   # hips stay open toward profile into the wide crouch (knees out, as the reference)
CHEST_YAW = {0: -2, 6: 0, 8: -8, 9: -16, 11: -17, 20: -15, 23: -14, 27: -8, 31: -4, 34: -3}
SIDE = {0: 5, 4: 6.5, 6: 6, 9: 8, 20: 7, 24: 3, 30: 1}
# support foot about its ball: a little with the hips in the kick, then turned out as the weight goes into the wide crouch
# (the reference's support thigh points at the camera in the recovery and the last frame: the knee goes out to the side)
PIVOT = {0: 6, 6: 8, 9: 16, 20: 16, 23: 22, 26: 42, 29: 45}
SUP_KNEE = {0: 16, 3: 22, 5: 13, 6: 11, 8: 11, 9: 12, 11: 13, 20: 18, 22: 23, 23: 26, 24: 58, 25: 90, 26: 108, 27: 116, 29: 118, 31: 108, 33: 82}
# centre-of-mass target: cm along the attack from the support foot centre while on one foot; the strike (9-12) and the
# step down onto the landing foot (21-23) are ahead of the toes, as the reference (declared exempt in the move sheet)
BAL_S = {0: 0, 3: -1.5, 5: -2, 6: -2, 7: 0, 8: 3, 9: 10, 11: 10, 13: 8, 15: 6.5, 20: 6.5, 22: 5, 23: 3}
BAL_F = {23: 0.0, 24: 0.1, 25: 0.3, 27: 0.5, 33: 0.68}   # the reference ends with the weight on the front foot (COM 0.45 of 0.55)            # front-foot share once both feet are down

_ovr = os.environ.get("MODEL_OVERRIDES")
if _ovr and os.path.exists(_ovr):
    for _k, _v in json.load(open(_ovr)).items():
        globals()[_k] = {int(f): v for f, v in _v.items()} if isinstance(_v, dict) else _v


def unit(v):
    v = np.asarray(v, dtype=float); n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def Rx(d):
    a = math.radians(d); c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])

def Ry(d):
    a = math.radians(d); c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]])

def Rz(d):
    a = math.radians(d); c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])

def rot_axis(ax, d):
    """Rotation of d degrees about the unit axis ax (Rodrigues)."""
    ax = unit(ax); a = math.radians(d); K = np.array([[0, -ax[2], ax[1]], [ax[2], 0, -ax[0]], [-ax[1], ax[0], 0]])
    return np.eye(3) + math.sin(a) * K + (1 - math.cos(a)) * K @ K

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
    """Monotone cubic (Fritsch-Carlson) through {frame: value} on 0..n-1: no overshoot between keys."""
    xs = np.array(sorted(keys), dtype=float)
    Y = np.array([np.atleast_1d(np.asarray(keys[int(x)], dtype=float)) for x in xs])
    out = np.zeros((n, Y.shape[1]))
    h = np.diff(xs); dlt = np.diff(Y, axis=0) / h[:, None]
    m = np.zeros_like(Y)
    for j in range(Y.shape[1]):
        d = dlt[:, j]
        for i in range(1, len(xs) - 1):
            if d[i - 1] * d[i] > 0:
                w1, w2 = 2 * h[i] + h[i - 1], h[i] + 2 * h[i - 1]
                m[i, j] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
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

    up = np.array([0.0, 1.0, 0.0])
    kf = np.array([math.sin(math.radians(KICK_YAW)), 0.0, math.cos(math.radians(KICK_YAW))])   # attack axis (horizontal)
    lat = np.cross(up, kf)                                                                      # +X side (left) of the plane

    def sdir(theta, out=0.0, side=1.0):
        """Side-view angle theta (deg) in the kick plane, tipped out of it by `out` deg toward side*lat."""
        t, o = math.radians(theta), math.radians(out)
        return math.cos(o) * (math.cos(t) * kf + math.sin(t) * up) + math.sin(o) * side * lat

    C = {"yaw": pchip(PEL_YAW)[:, 0], "cyaw": pchip(CHEST_YAW)[:, 0], "lean": pchip(LEAN)[:, 0], "side": pchip(SIDE)[:, 0],
         "pivot": pchip(PIVOT)[:, 0], "knee": pchip(SUP_KNEE)[:, 0], "leg": pchip(LEG), "leg_out": pchip(LEG_OUT)[:, 0],
         "foot_rel": pchip(FOOT_REL)[:, 0], "arm_l": pchip(ARM_L), "arm_r": pchip(ARM_R), "out_l": pchip(ARM_L_OUT), "out_r": pchip(ARM_R_OUT),
         "bal_s": pchip(BAL_S)[:, 0], "bal_f": pchip(BAL_F)[:, 0],
         "pron_l": pchip(PRON_L)[:, 0], "pron_r": pchip(PRON_R)[:, 0]}

    def at(curve, t):
        t = min(max(float(t), 0.0), N - 1.0); i = int(math.floor(t)); a = t - i
        return curve[i] if i >= N - 1 else curve[i] * (1 - a) + curve[i + 1] * a

    def body(k):
        D = {n: G[n].copy() for n in names}
        yaw = C["yaw"][k]
        tc, ts, th = k - CHEST_LAG, k - 0.5 * CHEST_LAG, k - HEAD_LAG
        lean, side, cy, yaw_c = at(C["lean"], tc), at(C["side"], tc), at(C["cyaw"], tc), at(C["yaw"], tc)
        Rp = Rx(0.25 * at(C["lean"], k)) @ Ry(yaw)
        Rs = Rx(0.55 * at(C["lean"], ts)) @ Rz(0.5 * at(C["side"], ts)) @ Ry(at(C["yaw"], ts) + 0.5 * at(C["cyaw"], ts))
        Rc = Rx(lean) @ Rz(side) @ Ry(yaw_c + cy)
        for n in PELV:
            D[n] = piv + Rp @ (G[n] - piv)
        th2 = k - 0.5 * (CHEST_LAG + HEAD_LAG)
        Rc2 = Rx(at(C["lean"], th2)) @ Rz(at(C["side"], th2)) @ Ry(at(C["yaw"], th2) + at(C["cyaw"], th2))
        if LEGACY_TRUNK:
            # v3 as first delivered: every segment rotated about the PELVIS at its own lagged time, so a lag between two
            # big rotations on a 60 cm lever pulled the head point up to 17 cm off the neck ("the neck detaches")
            for n in SP2:
                D[n] = piv + Rs @ (G[n] - piv)
            for n in CHEST:
                D[n] = piv + Rc @ (G[n] - piv)
            hm = piv + Rc2 @ (G["head_MainPoint"] - piv)
        else:
            # hierarchical trunk: each segment turns about its PARENT's joint (spine_02 about the pelvis, the chest about
            # spine_02, the head about neck_01), so the overlap lags change angles only and every length stays the idle's
            for n in SP2:
                D[n] = piv + Rs @ (G[n] - piv)
            s2 = D["spine_02_MainPoint"]
            for n in CHEST:
                D[n] = s2 + Rc @ (G[n] - G["spine_02_MainPoint"])
            hm = D["neck_01_MainPoint"] + Rc2 @ (G["head_MainPoint"] - G["neck_01_MainPoint"])
        Rh = Rx(0.45 * at(C["lean"], th)) @ Rz(0.4 * at(C["side"], th)) @ Ry(0.3 * (at(C["yaw"], th) + at(C["cyaw"], th)))
        for n in HEAD:
            D[n] = hm + Rh @ (G[n] - G["head_MainPoint"])
        return D

    # foot frames
    gax = unit(G["ball_MainPoint_l"] - G["foot_MainPoint_l"])
    glat = G["ball_AdditionalPoint_l"] - G["ball_MainPoint_l"]; glat = unit(glat - gax * np.dot(glat, gax))
    spot = G["foot_MainPoint_l"] + np.array(LAND_SPOT)
    # the planted landing foot points where its knee bends (knee over the toes): yaw the idle foot to the knee pole
    kp_h = unit(kf + 0.5 * lat); ga_h = unit(np.array([gax[0], 0.0, gax[2]]))
    yaw_l = math.degrees(math.atan2(ga_h[0], ga_h[2]) - math.atan2(kp_h[0], kp_h[2]))
    gax_land, glat_land = Ry(yaw_l) @ gax, Ry(yaw_l) @ glat
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

    def foot_centre(D, s):
        an, ba = D[f"foot_MainPoint_{s}"], D[f"ball_MainPoint_{s}"]
        d = ba - an; d[1] = 0.0
        return an + 0.30 * d

    def knee_dist(flex):
        """Hip-ankle distance for a knee flexion (deg)."""
        a, b = L_TH["r"], L_SH["r"]
        return math.sqrt(a * a + b * b + 2 * a * b * math.cos(math.radians(flex)))

    corr_hist = {}
    bend_prev = {}

    def corr_at(t):
        if t <= 0:
            return corr_hist.get(0, np.zeros(3))
        i = int(math.floor(t)); a_ = t - i
        c_i = corr_hist.get(i, np.zeros(3)); c_j = corr_hist.get(i + 1, c_i)
        return c_i * (1 - a_) + c_j * a_

    def limb(D, S, E, H, pref, s, Gn):
        """Write elbow/knee and end points of a two-bone limb whose joint positions are given."""
        u = unit(H - S); b = (E - S) - u * np.dot(E - S, u)
        bend = unit(b) if np.linalg.norm(b) > 0.3 else bend_prev.get(pref + s, unit(b) if np.linalg.norm(b) > 1e-6 else up)
        bend_prev[pref + s] = bend
        return bend

    def compose(k, corr):
        D = body(k)
        if LEGACY_TRUNK:
            lagged = {**{n: corr for n in PELV}, **{n: corr_at(k - 0.35 * CHEST_LAG) if k > 0 else corr for n in SP2},
                      **{n: corr_at(k - 0.7 * CHEST_LAG) if k > 0 else corr for n in CHEST}, **{n: corr_at(k - 0.9 * HEAD_LAG) if k > 0 else corr for n in HEAD}}
        else:
            # one shift for the whole trunk: a per-segment lagged translation stretches the spine (v3: up to 6 cm); the
            # overlap is in the lagged rotations above
            lagged = {n: corr for n in PELV + SP2 + CHEST + HEAD}
        for n in PELV + SP2 + CHEST + HEAD:
            D[n] = D[n] + lagged[n]
        if not LEGACY_TRUNK and k > 0:
            # positional overlap without stretching: each segment, in order up the chain, turns about its joint so its end
            # points where the lagged shift would have put it (a rigid aim, not a translation); the segments above ride along
            chain = [(SP2, "pelvis_MainPoint", "spine_02_MainPoint", 0.35 * CHEST_LAG),
                     (CHEST, "spine_02_MainPoint", "neck_01_MainPoint", 0.7 * CHEST_LAG),
                     (HEAD, "neck_01_MainPoint", "head_MainPoint", 0.9 * HEAD_LAG)]
            for i, (seg, piv_n, end_n, lag) in enumerate(chain):
                rec = k >= LAND - 1
                if seg is HEAD and not rec:
                    continue          # in the strike the head stays on the target (pre-flight: head toward the target)
                J = D[piv_n].copy(); E = D[end_n]
                fade = min(1.0, max(0.0, (AIM_FADE[1] + i - k) / float(AIM_FADE[1] - AIM_FADE[0])))   # one frame later per segment up the chain
                R = rot_between(E - J, E + fade * (LAG_AIM_GAIN_REC if rec else LAG_AIM_GAIN) * (corr_at(k - lag) - corr) - J)
                ax = np.array([R[2, 1] - R[1, 2], R[0, 2] - R[2, 0], R[1, 0] - R[0, 1]])
                deg = math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(R) - 1) / 2))))
                cap = LAG_AIM_MAX_REC if rec else LAG_AIM_MAX_DEG
                if deg > cap and np.linalg.norm(ax) > 1e-9:
                    R = rot_axis(ax, cap)
                for s_, *_ in chain[i:]:
                    for n in s_:
                        D[n] = J + R @ (D[n] - J)
        # ---- arms: side-view angles, sampled ARM_LAG late ----
        sh_l = D["upperarm_MainPoint_l"] - D["upperarm_MainPoint_r"]; sh_l[1] = 0.0
        fwd_c = np.cross(unit(sh_l), up)
        def adir(theta, out, side):
            b = ARM_AXIS_BLEND["l" if side > 0 else "r"]
            ax = unit((1 - b) * kf + b * fwd_c); lat_a = np.cross(up, ax)
            t, o = math.radians(theta), math.radians(out)
            return math.cos(o) * (math.cos(t) * ax + math.sin(t) * up) + math.sin(o) * side * lat_a
        for s, sd in (("l", 1.0), ("r", -1.0)):
            ta = k - ARM_LAG
            th_ua, o_ua = at(C[f"arm_{s}"], ta)[0], at(C[f"out_{s}"], ta)[0]
            th_fa, o_fa = at(C[f"arm_{s}"], ta - FA_LAG)[1], at(C[f"out_{s}"], ta - FA_LAG)[1]
            S = D[f"upperarm_MainPoint_{s}"]
            el = S + L_UA[s] * adir(th_ua, o_ua, sd)
            hand = el + L_FA[s] * adir(th_fa, o_fa, sd)
            bend = limb(D, S, el, hand, "arm", s, G)
            Rua = rot_between(G[f"lowerarm_MainPoint_{s}"] - G[f"upperarm_MainPoint_{s}"], el - S)
            Rfa = rot_between(G[f"hand_MainPoint_{s}"] - G[f"lowerarm_MainPoint_{s}"], hand - el)
            D[f"lowerarm_MainPoint_{s}"] = el
            D[f"lowerarm_AdditionalPoint_{s}"] = el + Rua @ (G[f"lowerarm_AdditionalPoint_{s}"] - G[f"lowerarm_MainPoint_{s}"])
            D[f"lowerarm_LimbDir_{s}"] = el + bend * np.linalg.norm(G[f"lowerarm_LimbDir_{s}"] - G[f"lowerarm_MainPoint_{s}"])
            D[f"hand_MainPoint_{s}"] = hand
            # pronation: the left (lead) hand rolls one way and the right the other for the same anatomical turn
            Rp = rot_axis(hand - el, (1.0 if s == "l" else -1.0) * at(C[f"pron_{s}"], ta - FA_LAG))
            for n in (f"hand_DirectionPoint_{s}", f"hand_AdditionalPoint_{s}"):
                D[n] = hand + Rp @ Rfa @ (G[n] - G[f"hand_MainPoint_{s}"])
        # ---- kicking leg (left): angles in the air, planted from LAND, blended in between ----
        Hh = D["thigh_MainPoint_l"]
        th_t, th_s = C["leg"][k]; lo = C["leg_out"][k]
        kn_a = Hh + L_TH["l"] * sdir(th_t, lo, 1.0)
        A_a = kn_a + L_SH["l"] * sdir(th_s, 0.0)
        w = 1.0 if k >= LAND else LAND_BLEND.get(k, 0.0)
        A = (1 - w) * A_a + w * spot
        if w == 0.0:
            kn = kn_a
        else:
            if np.linalg.norm(A - Hh) > (L_TH["l"] + L_SH["l"]) * 0.995:
                A = Hh + unit(A - Hh) * (L_TH["l"] + L_SH["l"]) * 0.995
            kn, _ = ik(Hh, A, L_TH["l"], L_SH["l"], kn_a if w < 1 else 0.5 * (Hh + A) + unit(kf + 0.5 * lat) * 40.0 + up * 5.0)
        bend = limb(D, Hh, kn, A, "leg", "l", G)
        Rsh = rot_between(G["foot_MainPoint_l"] - G["calf_MainPoint_l"], A - kn)
        D["calf_MainPoint_l"] = kn
        D["calf_AdditionalPoint_l"] = kn + Rsh @ (G["calf_AdditionalPoint_l"] - G["calf_MainPoint_l"])
        D["calf_LimbDir_l"] = kn + bend * np.linalg.norm(G["calf_LimbDir_l"] - G["calf_MainPoint_l"])
        if w < 1.0:
            a_air = sdir(th_s + C["foot_rel"][k], 0.0)
            l_air = lat
            a1 = unit((1 - w) * a_air + w * gax_land); l1 = unit((1 - w) * l_air + w * glat_land)
        else:
            a1, l1 = gax_land, glat_land
        l1 = unit(l1 - a1 * np.dot(l1, a1))
        F0 = np.stack([gax, glat, np.cross(gax, glat)], axis=1); F1 = np.stack([a1, l1, np.cross(a1, l1)], axis=1)
        Rf = F1 @ F0.T
        for n in FOOT["l"]:
            D[n] = A + Rf @ (G[n] - G["foot_MainPoint_l"])
        # ---- support leg (right): foot pivots on its ball, knee over the toes ----
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

    def target(D, k):
        t1 = foot_centre(D, "r") + kf * C["bal_s"][k]
        f = C["bal_f"][k] if k >= LAND else 0.0
        return (1 - f) * t1 + f * foot_centre(D, "l")

    def solve(k, corr):
        for _ in range(12):
            D = compose(k, corr)
            d = target(D, k) - com(D); d[1] = 0.0
            # support hip height from the support knee
            Hr, Ar = D["thigh_MainPoint_r"], D["foot_MainPoint_r"]
            hz = Hr - Ar; hz[1] = 0.0
            dd = knee_dist(C["knee"][k])
            v = math.sqrt(max(dd * dd - float(hz @ hz), 1.0))
            d[1] = (Ar[1] + v) - Hr[1]
            corr = corr + d
            if np.linalg.norm(d) < 0.03:
                break
        return corr

    for k in range(N):
        corr_hist[k] = solve(k, corr_hist.get(k - 1, np.zeros(3)).copy())
    # second sweep: the lagged chest/head terms read the next frame's shift (now known)
    for k in range(N):
        corr_hist[k] = solve(k, corr_hist[k])
    # the per-frame solve wobbles a little through the delayed chest/head terms (+-0.5 cm, zig-zags in the settle): smooth the
    # shift twice (1-2-1), keeping the snap and the contact as solved
    for _ in range(2):
        prev = {k: corr_hist[k].copy() for k in range(N)}
        for k in range(1, N - 1):
            if k in (8, 9, 10):
                continue
            corr_hist[k] = 0.25 * prev[k - 1] + 0.5 * prev[k] + 0.25 * prev[k + 1]
    out = {n: [None] * N for n in names}
    rep = []
    for k in range(N):
        D = compose(k, corr_hist[k])
        for n in names:
            out[n][k] = D[n]
        c = com(D)
        hipm = 0.5 * (D["thigh_MainPoint_l"] + D["thigh_MainPoint_r"])
        rep.append((k, round(float((hipm - D["foot_MainPoint_r"]) @ kf), 1), round(float(hipm[1] - D["foot_MainPoint_r"][1]), 1),
                    round(float((c - foot_centre(D, "r")) @ kf), 1), round(float(D["foot_MainPoint_l"][1]), 1),
                    round(float(np.linalg.norm(D["foot_MainPoint_l"] - D["thigh_MainPoint_l"])), 1)))
    print(" k  hip_fwd hip_up com_fwd ankle_l_y hip-ankle_l   (cm; fwd along the attack from the support ankle / foot centre)")
    for r in rep:
        print("%2d %7.1f %6.1f %7.1f %8.1f %8.1f" % r)
    res = {"frames": list(range(N)), "targets": {n: [out[n][k].tolist() for k in range(N)] for n in names}, "feet": {},
           "keyplan": {"keys": KEYS, "linear": LINEAR, "bezier": BEZIER, "layers": LAYERS, "weights": {}, "kind": "clamped"},
           "move": {"contact": list(CONTACT), "faf": FAF_K, "kick_dir_cascadeur": kf.tolist()}}
    json.dump(res, open(os.path.join(HERE, OUT), "w"))
    print("written " + OUT)


if __name__ == "__main__":
    main()
