"""Jab v7: Mario (SSBU jab 1) timing on our rig. Offline: v7_src_points.json (v6 dump) -> v7_targets.json.

Timing source: the owner's 19-frame SSBU GIF, measured per game frame G (0 = first GIF frame). Our game ticks at
62.5 Hz (NPP 16 ms), the clip is 60 fps, so anim frame k samples G = k / 0.96 (owner decision D1). FAF = G18 = anim 17.
Windows (G): 0 start | 1-2 active (contact, our v6 contact frames kept) | 3-5 snap back | 6-9 spring recoil |
10-13 slow return | 14-18 exit (rises, leans back, fist past its guard) | tail -> guard at anim 37.
Poses: G = guard (frame 0), C = contact (frame 2; the rear arm is re-authored: pulled back toward the shoulder),
E = exit (guard + 60% of the owner's blockout frame 19 - frame 0, feet planted).
Cascadeur space: Y up, the character faces +Z, its right side is -X. Lead (punching) arm = LEFT, rear = RIGHT."""
import sys, os, json, math
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
UP = np.array([0.0, 1.0, 0.0])
K_PER_G = 0.96                     # anim frames per game frame
N = 38                              # clip length (0..37)
FAF_K = 17

# ---- Mario curves per game frame G (0..18) ----------------------------------------------------------------------
# fist forward %, Mario measured; G4-G6 eased up by 1-2 points (inside the 5% tolerance) so the forearm keeps 2 cm off
# the chest in the deepest chamber
EXT = [0, 100, 100, 46, 26, 18, 20, 23, 29, 29, 27, 24, 19, 11, 4, -3, -10, -17, -23]
EXT_MARIO = [0, 100, 100, 46, 24, 17, 19, 23, 29, 29, 27, 24, 19, 11, 4, -3, -10, -17, -23]
# Torso hold, in fractions of OUR contact lunge. Mario's body holds its lunge, but his lunge is ~8-13% of the reach and
# ours is ~30%: holding all of it would put the chambered fist inside the belly. The retracting arm pulls the torso back by
# about a third (G3-G4, a frame after the arm), then it holds at ~62% (head ~80%) with a small forward creep during the
# spring, and returns from G10.
A_PELVIS = [0, 1, 1, 0.90, 0.58, 0.58, 0.63, 0.65, 0.66, 0.63, 0.56, 0.44, 0.29, 0.15, 0.06, 0.02, 0, 0, 0]
A_CHEST = [0, 1, 1, 0.88, 0.56, 0.56, 0.61, 0.63, 0.64, 0.61, 0.54, 0.42, 0.27, 0.14, 0.05, 0.01, 0, 0, 0]
A_HEAD = [0, 1, 1, 0.95, 0.84, 0.79, 0.78, 0.80, 0.81, 0.78, 0.70, 0.55, 0.35, 0.17, 0.06, 0.00, 0, 0, 0]
B_EXIT = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.02, 0.07, 0.18, 0.33, 0.50, 0.67, 0.84, 1.0]
# fist arc below the guard->contact line, cm: Mario's glove drops to belt height after the snap back and rises in the
# exit (measured in px below the line, x0.6 cm/px, x0.55 for our proportions); negative = above the line
DIP = [0, 0, 0, 3.3, 8.0, 12.1, 9.9, 7.2, 6.0, 5.3, 5.5, 6.0, 6.3, 5.3, 2.6, 0.2, -0.3, -2.0, -4.0]
W_SHO = [0, 1, 1, 0.75, 0.42, 0.24, 0.20, 0.21, 0.25, 0.28, 0.28, 0.25, 0.21, 0.14, 0.07, 0, 0, 0, 0]
W_REAR = [0, 1, 1, 1, 0.97, 0.92, 0.86, 0.80, 0.72, 0.62, 0.50, 0.38, 0.25, 0.14, 0.06, 0.02, 0, 0, 0]
FIST_OUT = [0, 0, 0, 2.0, 5.5, 5.5, 5.5, 4.5, 3.8, 3.5, 3.0, 2.5, 2.0, 1.6, 1.4, 1.4, 1.5, 1.7, 2.0]   # cm toward the lead side (+X)
REAR_BOUNCE = [0, 0, 0, 0, 0.25, 0.85, 1.3, 1.5, 1.25, 0.8, 0.3, 0, 0, 0, 0, 0, 0, 0, 0]      # cm forward
FIST_UP_EXIT = 3.0                  # cm, extra rise of the fist with the exit pose (on top of the DIP curve)
EXIT_SCALE = 0.6                    # fraction of the blockout's exit pose
CLAV_PULL = 4.0                     # cm, lead shoulder pulled back along the arm as the fist comes home
CHAMBER_BEND = np.array([1.0, -0.35, -0.6])    # lead elbow direction in the chamber: out (+X = left), back, a bit down
# rear hand at contact: pulled back toward the shoulder and lowered (Mario counter-motion; keeps the punch readable
# from behind, where the projected fist sits at head height)
REAR_PULL = np.array([-2.0, -6.0, -9.0])

def g_curve(c, G):
    """Linear sample of a per-game-frame curve at fractional G (clamped to 0..18)."""
    G = min(max(G, 0.0), len(c) - 1.0)
    i = int(math.floor(G)); a = G - i
    return c[i] if i >= len(c) - 1 else c[i] * (1 - a) + c[i + 1] * a

def G_of(k):
    g = k / K_PER_G
    return 1.0 if 1 <= k <= 1 else (2.0 if k == 2 else g)   # contact frames locked on G1-G2

def unit(v):
    return v / (np.linalg.norm(v) + 1e-9)

def ik_elbow(S, H, lu, lf, pole):
    d = np.linalg.norm(H - S)
    u = unit(H - S)
    w = pole - S; w = unit(w - u * np.dot(w, u))
    if d >= lu + lf - 1e-3:
        return S + u * lu, w
    x = (lu * lu - lf * lf + d * d) / (2 * d)
    h = math.sqrt(max(lu * lu - x * x, 0.0))
    return S + u * x + w * h, w

def main():
    src = json.load(open(os.path.join(HERE, "v7_src_points.json")))["points"]
    P = {n: np.array(v, dtype=float) for n, v in src.items()}
    bl = json.load(open(os.path.join(HERE, "blockout_dump.json")))["points"]
    G = {n: P[n][0].copy() for n in P}
    C = {n: P[n][2].copy() for n in P}
    C1 = {n: P[n][1].copy() for n in P}

    def exit_delta(n, fallback):
        if n in bl and "19" in bl[n] and "0" in bl[n]:
            return EXIT_SCALE * (np.array(bl[n]["19"]) - np.array(bl[n]["0"]))
        return EXIT_SCALE * (np.array(bl[fallback]["19"]) - np.array(bl[fallback]["0"]))
    groups = {
        "pelvis": (["pelvis_MainPoint", "pelvis_AdditionalPoint", "thigh_MainPoint_l", "thigh_MainPoint_r"], A_PELVIS),
        "spine2": (["spine_02_MainPoint", "spine_02_AdditionalPoint"], [0.5 * (a + b) for a, b in zip(A_PELVIS, A_CHEST)]),
        "chest": (["spine_04_MainPoint", "spine_04_AdditionalPoint"], A_CHEST),
        "neck": (["neck_01_MainPoint", "neck_01_AdditionalPoint"], [0.5 * (a + b) for a, b in zip(A_CHEST, A_HEAD)]),
        "head": (["head_MainPoint", "head_DirectionPoint", "head_AdditionalPoint"], A_HEAD),
    }
    E = {}
    # the body goes back with the fist in the exit (Mario's whole figure moves back): extra backward shift on top of the
    # blockout direction, so the fist at -23% does not end up inside the chest
    EXTRA_BACK = {"pelvis": 5.0, "spine2": 6.0, "chest": 7.0, "neck": 6.0, "head": 5.0}
    for gname, (pts, _) in groups.items():
        for n in pts:
            E[n] = G[n] + exit_delta(n, pts[0]) + np.array([0.0, 0.0, -EXTRA_BACK[gname]])
    # feet planted: the exit can't rise more than the legs allow; cap the pelvis rise at 8.5 cm
    for n in groups["pelvis"][0]:
        d = E[n] - G[n]
        if d[1] > 8.5:
            E[n] = G[n] + d * (8.5 / d[1])
    for side in ("l", "r"):
        for n in (f"upperarm_MainPoint_{side}", f"clavicle_MainPoint_{side}", f"clavicle_AdditionalPoint_{side}"):
            E[n] = G[n] + exit_delta(n, f"upperarm_MainPoint_{side}")
        for n in (f"hand_MainPoint_{side}", f"hand_DirectionPoint_{side}", f"hand_AdditionalPoint_{side}"):
            if side == "r":
                E[n] = G[n] + (E["spine_04_MainPoint"] - G["spine_04_MainPoint"]) + np.array([-1.5, 2.0, -2.5])
            else:
                E[n] = G[n] + exit_delta(f"hand_MainPoint_{side}", f"hand_MainPoint_{side}")
    chest = "spine_04_MainPoint"
    knees = ["calf_MainPoint_l", "calf_AdditionalPoint_l", "calf_LimbDir_l", "calf_MainPoint_r", "calf_AdditionalPoint_r", "calf_LimbDir_r"]
    pel_exit = E["pelvis_MainPoint"] - G["pelvis_MainPoint"]
    feet = [n for n in P if n.startswith("foot_") or n.startswith("ball_")]
    lu = {s: np.linalg.norm(G[f"lowerarm_MainPoint_{s}"] - G[f"upperarm_MainPoint_{s}"]) for s in "lr"}
    lf = {s: np.linalg.norm(G[f"hand_MainPoint_{s}"] - G[f"lowerarm_MainPoint_{s}"]) for s in "lr"}
    pole_d = {s: np.linalg.norm(G[f"lowerarm_LimbDir_{s}"] - G[f"lowerarm_MainPoint_{s}"]) for s in "lr"}

    out = {n: [None] * N for n in P}
    curves = {"k": [], "G": [], "ext_target": [], "a_head": [], "b_exit": []}
    for k in range(0, 18):
        Gk = G_of(k)
        T = {}
        for gname, (pts, acurve) in groups.items():
            a = g_curve(acurve, Gk); b = g_curve(B_EXIT, Gk)
            for n in pts:
                T[n] = G[n] + a * (C[n] - G[n]) + b * (E[n] - G[n])
        a_p = g_curve(A_PELVIS, Gk); b = g_curve(B_EXIT, Gk)
        for n in knees:
            T[n] = G[n] + a_p * (C[n] - G[n]) + b * 0.5 * pel_exit
        ch = T[chest]
        ext = g_curve(EXT, Gk) / 100.0
        ec = min(max(ext, 0.0), 1.0)
        # ---- lead arm ----
        a_c = g_curve(A_CHEST, Gk)
        arm_dir = unit(C["hand_MainPoint_l"] - C["upperarm_MainPoint_l"])
        pull = (1.0 - ec) * min(a_c, 1.0) * CLAV_PULL
        for n in ("upperarm_MainPoint_l", "clavicle_MainPoint_l", "clavicle_AdditionalPoint_l"):
            rel = (G[n] - G[chest]) + min(a_c, 1.0) * ((C[n] - C[chest]) - (G[n] - G[chest])) + b * ((E[n] - E[chest]) - (G[n] - G[chest]))
            T[n] = ch + rel - arm_dir * (pull if n == "upperarm_MainPoint_l" else 0.5 * pull)
        fist = np.array([G["hand_MainPoint_l"][0] + min(a_c, 1.0) * (C["hand_MainPoint_l"][0] - G["hand_MainPoint_l"][0]) + g_curve(FIST_OUT, Gk),
                         G["hand_MainPoint_l"][1] + ec * (C["hand_MainPoint_l"][1] - G["hand_MainPoint_l"][1]) - g_curve(DIP, Gk) + b * FIST_UP_EXIT,
                         G["hand_MainPoint_l"][2] + ext * (C["hand_MainPoint_l"][2] - G["hand_MainPoint_l"][2])])
        T["hand_MainPoint_l"] = fist
        ww = min(max(g_curve(EXT, Gk - 1.0) / 100.0, 0.0), 1.0)      # wrist aim lags the fist by one game frame
        for n in ("hand_DirectionPoint_l", "hand_AdditionalPoint_l"):
            rg = G[n] - G["hand_MainPoint_l"]; rc = C[n] - C["hand_MainPoint_l"]
            r = rg + ww * (rc - rg); r = unit(r) * (np.linalg.norm(rg) + ww * (np.linalg.norm(rc) - np.linalg.norm(rg)))
            T[n] = fist + r
        S = T["upperarm_MainPoint_l"]
        u_ax = unit(fist - S)
        bG = G["lowerarm_LimbDir_l"] - G["upperarm_MainPoint_l"]; bC = C["lowerarm_LimbDir_l"] - C["upperarm_MainPoint_l"]
        bG = unit(bG - u_ax * np.dot(bG, u_ax)); bC = unit(bC - u_ax * np.dot(bC, u_ax))
        bD = unit(CHAMBER_BEND); bD = unit(bD - u_ax * np.dot(bD, u_ax))
        sw = min(max((Gk - 2.0) / 3.0, 0.0), 1.0); sw = sw * sw * (3 - 2 * sw)
        w2 = min(1.0 - min(a_c, 1.0), 0.75)     # keep a quarter of the outward chamber bend through the exit
        target = unit((1 - w2) * bD + w2 * bG)
        bend_dir = unit((1 - sw) * bC + sw * target)
        bend_dir = unit(bend_dir - u_ax * np.dot(bend_dir, u_ax))
        el, bend = ik_elbow(S, fist, lu["l"], lf["l"], S + bend_dir * 30.0)
        T["lowerarm_MainPoint_l"] = el
        ra = (G["lowerarm_AdditionalPoint_l"] - G["lowerarm_MainPoint_l"]) * (1 - ec) + (C["lowerarm_AdditionalPoint_l"] - C["lowerarm_MainPoint_l"]) * ec
        T["lowerarm_AdditionalPoint_l"] = el + ra
        T["lowerarm_LimbDir_l"] = el + bend * pole_d["l"]
        # ---- rear arm ----
        wr = g_curve(W_REAR, Gk); bo = g_curve(REAR_BOUNCE, Gk)
        for n in ("upperarm_MainPoint_r", "clavicle_MainPoint_r", "clavicle_AdditionalPoint_r"):
            rel = (G[n] - G[chest]) + min(a_c, 1.0) * ((C[n] - C[chest]) - (G[n] - G[chest])) + b * ((E[n] - E[chest]) - (G[n] - G[chest]))
            T[n] = ch + rel
        hr_g = G["hand_MainPoint_r"] - G[chest]
        hr_c = (C["hand_MainPoint_r"] - C[chest]) + REAR_PULL
        hr = ch + hr_g + wr * (hr_c - hr_g) + np.array([0, 0, bo]) + b * ((E["hand_MainPoint_r"] - E[chest]) - hr_g)
        T["hand_MainPoint_r"] = hr
        for n in ("hand_DirectionPoint_r", "hand_AdditionalPoint_r"):
            rg = G[n] - G["hand_MainPoint_r"]; rc = C[n] - C["hand_MainPoint_r"]
            T[n] = hr + rg + wr * (rc - rg)
        Sr = T["upperarm_MainPoint_r"]
        pole_r = Sr + (G["lowerarm_LimbDir_r"] - G["upperarm_MainPoint_r"])     # guard pole: elbow down and tucked
        elr, bendr = ik_elbow(Sr, hr, lu["r"], lf["r"], pole_r)
        T["lowerarm_MainPoint_r"] = elr
        T["lowerarm_AdditionalPoint_r"] = elr + (G["lowerarm_AdditionalPoint_r"] - G["lowerarm_MainPoint_r"])
        T["lowerarm_LimbDir_r"] = elr + bendr * pole_d["r"]
        for n in feet:
            T[n] = G[n].copy()
        if k in (1, 2):    # contact frames: v6 contact exactly, except the re-authored rear arm
            src_k = C1 if k == 1 else C
            for n in T:
                if not (n.endswith("_r") and (n.startswith("hand_") or n.startswith("lowerarm_") or n.startswith("upperarm_") or n.startswith("clavicle_"))):
                    T[n] = src_k[n].copy()
        if k == 0:
            T = {n: G[n].copy() for n in P}
        for n in P:
            out[n][k] = T.get(n, P[n][k]).copy()
        curves["k"].append(k); curves["G"].append(round(Gk, 3)); curves["ext_target"].append(round(g_curve(EXT_MARIO, Gk), 2))
        curves["a_head"].append(round(g_curve(A_HEAD, Gk), 3)); curves["b_exit"].append(round(g_curve(B_EXIT, Gk), 3))
    # ---- tail 17 -> 37: continue the exit velocity, then ease to the guard (frame 37 == frame 0) ----
    cref = "spine_04_MainPoint"
    dref = out[cref][17] - G[cref]
    s_shared = max(float(np.dot(out[cref][17] - out[cref][16], dref) / max(float(np.dot(dref, dref)), 1e-6)), 0.0)
    for n in P:
        p17, g0 = out[n][17], G[n]
        d = p17 - g0
        s = s_shared
        tp = 3.0; kp = 1.0 + s * tp * 0.5
        for k in range(18, N):
            t = k - 17
            if t <= tp:
                u = t / tp
                h00, h10, h01 = 2 * u**3 - 3 * u**2 + 1, u**3 - 2 * u**2 + u, -2 * u**3 + 3 * u**2
                kk = h00 * 1.0 + h10 * tp * s + h01 * kp
            else:
                u = (t - tp) / (N - 1 - 17 - tp)
                kk = kp * (1 - (u * u * u * (u * (6 * u - 15) + 10)))
            out[n][k] = g0 + d * kk
    def segd(a0, a1, b0, b1):
        best = 1e9
        for t in np.linspace(0, 1, 13):
            p = a0 + (a1 - a0) * t; ab = b1 - b0
            u = float(np.clip(np.dot(p - b0, ab) / max(np.dot(ab, ab), 1e-9), 0, 1))
            best = min(best, float(np.linalg.norm(p - (b0 + ab * u))))
        return best
    torso = ["pelvis_MainPoint", "spine_02_MainPoint", "spine_04_MainPoint", "neck_01_MainPoint"]
    prox = []
    for k in range(N):
        q = {n: out[n][k] for n in P}
        worst = (1e9, "")
        for side, other in (("l", "r"), ("r", "l")):
            segs = {"upper": (q[f"upperarm_MainPoint_{side}"], q[f"lowerarm_MainPoint_{side}"], 5.75),
                    "fore": (q[f"lowerarm_MainPoint_{side}"], q[f"hand_MainPoint_{side}"], 4.75),
                    "hand": (q[f"hand_MainPoint_{side}"], q[f"hand_MainPoint_{side}"] + unit(q[f"hand_DirectionPoint_{side}"] - q[f"hand_MainPoint_{side}"]) * 10, 4.5)}
            for nm, (a0, a1, r) in segs.items():
                if nm == "upper":
                    continue
                for i in range(len(torso) - 1):
                    d = segd(a0, a1, q[torso[i]], q[torso[i + 1]]) - r - 12.5
                    if d < worst[0]:
                        worst = (d, f"{nm}_{side}-{torso[i]}")
        prox.append((round(worst[0], 1), worst[1]))
    print("proxy torso clearance per frame (cm, approx):", [p[0] for p in prox])
    print("worst proxy:", sorted(prox)[:4])
    res = {"frames": list(range(1, N)), "targets": {n: [out[n][k].tolist() for k in range(1, N)] for n in P if n not in feet},
           "feet": {n: G[n].tolist() for n in feet}, "curves": curves}
    json.dump(res, open(os.path.join(HERE, "v7_targets.json"), "w"))
    # report: fist forward % (z) per anim frame vs the Mario target, clearance proxies, rear hand
    dz = C["hand_MainPoint_l"][2] - G["hand_MainPoint_l"][2]
    print(" k     G   ext_target  ext_model  head_fwd%  rear_hand_z  fist-chest(cm)")
    for k in range(0, N):
        e_m = 100 * (out["hand_MainPoint_l"][k][2] - G["hand_MainPoint_l"][2]) / dz
        hz = 100 * np.dot(out["head_MainPoint"][k] - G["head_MainPoint"], unit(C["head_MainPoint"] - G["head_MainPoint"])) / np.linalg.norm(C["head_MainPoint"] - G["head_MainPoint"])
        fc = np.linalg.norm(out["hand_MainPoint_l"][k] - out["spine_04_MainPoint"][k])
        tgt = curves["ext_target"][k] if k < len(curves["ext_target"]) else float("nan")
        print(f"{k:2d} {k / K_PER_G:5.2f}  {tgt:9.1f}  {e_m:9.1f}  {hz:9.1f}  {out['hand_MainPoint_r'][k][2]:10.1f}  {fc:8.1f}")

if __name__ == "__main__":
    main()
