"""Motion curves: how much each body part moves, when, and with or against the strike. Catches what the other checks miss:
a move whose arms barely move or whose legs have no anticipation passes balance/arcs/spacing (Ganondorf v2, rejected 01/11).

Per frame it tracks hands, elbows, feet, knees, head and chest RELATIVE TO THE PELVIS, plus the pelvis and the centre of
mass in the world, and reports per phase (move_sheets.json: startup [0, c0), active [c0, c1], hold, recovery to the FAF):
  amplitude      max distance from the phase's first frame (cm);
  share          each part's % of the total path (and of sum speed^2, "energy") of the limb parts; a hand/foot under
                 `curves_still_share` while the action runs (startup + active) is flagged "near-still";
  peaks          speed-peak frame per part over the action window (sub-frame), order and lag behind the pelvis (kinetic
                 chain, overlap);
  with/counter   correlation between each part's velocity along the attack and the effector's: > +0.3 moves with the
                 strike, < -0.3 counters it (arms usually counter a kick);
  anticipation   the effector's (and body's) displacement opposite the strike before contact, as a ratio of the strike
                 amplitude: back = farthest point behind the frame-0 position along the strike line, strike = contact
                 extension from there.
Outputs: sheets/<take>_curves.png ("body part x frame" speed heatmap, phases marked, speed peaks dotted),
         sheets/<take>_curves.txt (compact table) and .json. With a reference (move sheet "reference": refs/<dir> holding
         reference.json + rtmlib keypoints) it also runs motion_ref.compare (side-camera 2D pose distance, amplitude
         ratio, anticipation and DTW per limb) and the GATE: a limb well below the reference amplitude, or a missing
         anticipation, fails (exit 1) before any other check. Thresholds: motion_thresholds.json `curves_*`.

Sources: --source=bones (default when sheets/raw/<take>_legbones.json exists: the baked Unreal bones) or --source=model
(the model's rig points, <take>_targets_raw.json, Cascadeur space converted: used by the pre-flight gate, before any pass).
Uses pybvh (finite differences, SPARC smoothness) and dtaidistance (DTW) when installed (the animcurves venv); falls back
to numpy otherwise, with the same numbers to rounding.

Usage: python motion_curves.py <take> [--sheet=KEY] [--source=bones|model] [--ref=refs/<dir>|none] [--quiet] [--blind]
       --blind also writes sheets/<take>_curves_blind.png (no title, phase names or wind-up marker) for the blind review.
"""
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))                   # scripts and data together (the working folder), as motion_check
sys.path.insert(0, HERE)

try:
    from pybvh.signal import finite_difference as _fd
    from pybvh.analysis import sparc as _sparc
except Exception:                                                     # numpy fallback (same central-difference stencil)
    _fd = None
    _sparc = None

LIMBS = ["hand_l", "elbow_l", "hand_r", "elbow_r", "knee_l", "foot_l", "knee_r", "foot_r", "head", "chest"]
ROWS = ["head", "chest", "pelvis", "com"] + LIMBS[:8]                 # heatmap order: body, arms, legs
BONE_OF = {"hand_l": "hand_l", "hand_r": "hand_r", "elbow_l": "lowerarm_l", "elbow_r": "lowerarm_r", "shoulder_l": "upperarm_l",
           "shoulder_r": "upperarm_r", "foot_l": "foot_l", "foot_r": "foot_r", "knee_l": "calf_l", "knee_r": "calf_r",
           "hip_l": "thigh_l", "hip_r": "thigh_r", "head": "head", "chest": "spine_05", "pelvis": "pelvis", "ball_l": "ball_l",
           "ball_r": "ball_r"}
POINT_OF = {"hand_l": "hand_MainPoint_l", "hand_r": "hand_MainPoint_r", "elbow_l": "lowerarm_MainPoint_l",
            "elbow_r": "lowerarm_MainPoint_r", "shoulder_l": "upperarm_MainPoint_l", "shoulder_r": "upperarm_MainPoint_r",
            "foot_l": "foot_MainPoint_l", "foot_r": "foot_MainPoint_r", "knee_l": "calf_MainPoint_l", "knee_r": "calf_MainPoint_r",
            "hip_l": "thigh_MainPoint_l", "hip_r": "thigh_MainPoint_r", "head": "head_MainPoint", "chest": "spine_04_MainPoint",
            "pelvis": "pelvis_MainPoint", "ball_l": "ball_MainPoint_l", "ball_r": "ball_MainPoint_r"}
TH = {"curves_still_share": 0.04, "curves_corr_min": 0.3}


def load_th():
    p = os.path.join(HERE, "motion_thresholds.json")
    if os.path.exists(p):
        TH.update({k: v for k, v in json.load(open(p)).items() if k.startswith("curves_")})


def sheet_of(key):
    return dict(json.load(open(os.path.join(HERE, "move_sheets.json"))).get(key) or {})


# ---------------------------------------------------------------- loading: every source -> {part: (N, 3)} in UE component space
# (X left, Y forward, Z up, cm)
def load_bones(take):
    import motion_check as MC                                         # COM from the physics-asset shapes, as motion_check
    D = json.load(open(os.path.join(HERE, "sheets", "raw", f"{take}_legbones.json")))["frames"]
    N = len(D)
    fr = [D[str(f)] for f in range(N)]
    P = {p: np.array([fr[f][b][:3] for f in range(N)], float) for p, b in BONE_OF.items()}
    P["com"] = np.array([MC.com(fr[f]) for f in range(N)])
    return P


def load_model(take, sheet_key=None):
    import preflight as PF
    m = PF.Move(os.path.join(HERE, f"{take}_targets_raw.json"), sheet_key)
    cv = lambda a: np.stack([a[:, 0], a[:, 2], a[:, 1]], 1)           # Cascadeur (X left, Y up, Z fwd) -> UE (X left, Y fwd, Z up)
    P = {p: cv(m.P[n]) for p, n in POINT_OF.items()}
    P["com"] = cv(np.array([m.com(f) for f in range(m.N)]))
    return P


def effector_of(sheet):
    kick = str(sheet.get("strike_limb", "")).startswith("leg") or str(sheet.get("effector", "")).startswith("ball")
    side = str(sheet.get("strike_side", "l"))[-1] if kick else ("r" if str(sheet.get("effector", "")).endswith("_r") else "l")
    return (f"ball_{side}" if kick else f"hand_{side}"), side, kick


def phases(sheet, N):
    c0, c1 = sheet["contact"]
    faf = min(sheet.get("faf", N - 1), N - 1)
    hw = sheet.get("hold_window", [c0, c1])
    out = [("startup", 0, c0 - 1), ("active", c0, c1)]
    if hw[1] > c1:
        out.append(("hold", c1 + 1, hw[1]))
    out.append(("recovery", max(c1, hw[1]) + 1, faf))
    return [(n, a, b) for n, a, b in out if b >= a]


def diff(x):
    return _fd(x, 1.0) if _fd is not None else np.gradient(x, axis=0)


def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v


def peak(s, lo, hi):
    """Sub-frame speed peak in [lo, hi] (parabola through the max and its neighbours)."""
    i = lo + int(np.argmax(s[lo:hi + 1]))
    if lo < i < hi:
        a, b, c = s[i - 1], s[i], s[i + 1]
        d = a - 2 * b + c
        if abs(d) > 1e-9:
            return i + 0.5 * (a - c) / d
    return float(i)


# ---------------------------------------------------------------- analysis
def analyse(P, sheet):
    N = len(P["pelvis"])
    eff, side, kick = effector_of(sheet)
    c0, c1 = sheet["contact"]
    rel = {p: P[p] - P["pelvis"] for p in LIMBS}                       # limbs, head, chest relative to the pelvis
    rel["pelvis"] = P["pelvis"]; rel["com"] = P["com"]
    V = {p: diff(x) for p, x in rel.items()}
    S = {p: np.linalg.norm(v, axis=1) for p, v in V.items()}
    E = P[eff]
    a = unit(np.array([*(E[c0] - E[0])[:2], 0.0]))                   # attack direction, horizontal
    u = unit(E[c0] - E[0])                                             # strike line (3D) for the anticipation

    out = {"effector": eff, "kick": kick, "attack_dir": [round(float(x), 3) for x in a], "phases": {}, "N": N,
           "speed": {p: [round(float(x), 3) for x in S[p]] for p in ROWS}}
    for name, s, e in phases(sheet, N):
        ph = {}
        tot_path = tot_en = 0.0
        for p in rel:
            x = rel[p][s:e + 1]
            amp = float(np.max(np.linalg.norm(x - x[0], axis=1)))
            path = float(np.sum(np.linalg.norm(np.diff(x, axis=0), axis=1))) if e > s else float(S[p][s])
            en = float(np.sum(S[p][s:e + 1] ** 2))
            ph[p] = {"amp_cm": round(amp, 1), "path_cm": round(path, 1), "_en": en}
            if p in LIMBS:
                tot_path += path; tot_en += en
        for p in ph:
            ph[p]["share"] = round(ph[p]["path_cm"] / tot_path, 3) if tot_path > 1e-9 and p in LIMBS else None
            ph[p]["energy_share"] = round(ph[p].pop("_en") / tot_en, 3) if tot_en > 1e-9 and p in LIMBS else None
        out["phases"][name] = {"frames": [s, e], "parts": ph}

    # action window: startup + active
    lo, hi = 0, max(c1, min(4, N - 1))                                # a jab's 1-frame startup: look a few frames on
    act = {}
    tot = sum(float(np.sum(np.linalg.norm(np.diff(rel[p][lo:hi + 1], axis=0), axis=1))) for p in LIMBS)
    for p in LIMBS:
        act[p] = float(np.sum(np.linalg.norm(np.diff(rel[p][lo:hi + 1], axis=0), axis=1))) / tot if tot > 1e-9 else 0.0
    out["action_share"] = {p: round(v, 3) for p, v in act.items()}
    out["near_still"] = [p for p in LIMBS[:8] if act[p] < TH["curves_still_share"]]

    pk = {p: peak(S[p], lo, hi) for p in ROWS}
    out["peaks"] = {p: round(v, 2) for p, v in sorted(pk.items(), key=lambda kv: kv[1])}
    out["lag_vs_pelvis"] = {p: round(v - pk["pelvis"], 2) for p, v in out["peaks"].items()}

    ve = V["pelvis"] + V[eff.replace("ball", "foot")] if eff.startswith("ball") else V["pelvis"] + V[eff]   # effector, world
    fe = ve[lo:hi + 1] @ a
    corr = {}
    for p in LIMBS + ["pelvis", "com"]:
        fp = V[p][lo:hi + 1] @ a
        c = float(np.corrcoef(fp, fe)[0, 1]) if np.std(fp) > 1e-6 and np.std(fe) > 1e-6 else 0.0
        corr[p] = {"corr": round(c, 2), "dir": "with" if c > TH["curves_corr_min"] else "counter" if c < -TH["curves_corr_min"] else "neutral"}
    out["vs_strike"] = corr

    d = (E - E[0]) @ u
    k_ant = int(np.argmin(d[:c0])) if c0 > 0 else 0
    back = max(0.0, -float(d[k_ant]))
    strike = float(np.max(d[c0:c1 + 1]) - d[k_ant])
    body = {}
    for p in ("pelvis", "com", "chest", "head"):
        x = P[p] - P[p][0]                                             # world: the body's own wind-up
        body[p] = round(max(0.0, -float(np.min((x[:c0] if c0 > 0 else x[:1]) @ a))) / strike, 3) if strike > 1e-6 else 0.0
    out["anticipation"] = {"frame": k_ant, "back_cm": round(back, 1), "strike_cm": round(strike, 1),
                           "ratio": round(back / strike, 3) if strike > 1e-6 else 0.0, "body_back_ratio": body}
    if _sparc is not None:
        out["sparc"] = {p: round(float(_sparc(S[p][: phases(sheet, N)[-1][2] + 1], 60.0)), 2) for p in ROWS}
    return out


# ---------------------------------------------------------------- outputs
def table(m, take):
    L = [f"motion curves {take}: effector {m['effector']}, {m['N']} frames"]
    names = list(m["phases"])
    L.append("part      " + "".join(f"{n[:8]:>18}" for n in names) + "   action%  peak  lag  vs strike")
    L.append("          " + "".join(f"{'amp cm / share':>18}" for _ in names))
    for p in ROWS:
        row = f"{p:<10}"
        for n in names:
            q = m["phases"][n]["parts"][p]
            sh = f"{100 * q['share']:4.0f}%" if q["share"] is not None else "    -"
            row += f"{q['amp_cm']:>12.1f} {sh}"
        a = m["action_share"].get(p)
        row += f"   {100 * a:5.1f}%" if a is not None else "        -"
        row += f" {m['peaks'][p]:5.1f} {m['lag_vs_pelvis'][p]:+4.1f}"
        vs = m["vs_strike"].get(p)
        row += f"  {vs['dir']} {vs['corr']:+.2f}" if vs else ""
        L.append(row)
    an = m["anticipation"]
    L.append(f"anticipation: effector back {an['back_cm']} cm at frame {an['frame']}, strike {an['strike_cm']} cm, "
             f"ratio {an['ratio']}; body back/strike {an['body_back_ratio']}")
    L.append("peak order: " + " -> ".join(f"{p}@{v}" for p, v in m["peaks"].items()))
    if m["near_still"]:
        L.append(f"NEAR-STILL while the action runs (< {100 * TH['curves_still_share']:.0f}% of the limb path): {', '.join(m['near_still'])}")
    return "\n".join(L)


def heatmap(m, sheet, take, path, ref=None, blind=False):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    Sp = np.array([m["speed"][p] for p in ROWS])
    N = Sp.shape[1]
    fig, ax = plt.subplots(figsize=(max(8, N * 0.28), 4.6))
    im = ax.imshow(Sp, aspect="auto", cmap="magma", interpolation="nearest", vmin=0, vmax=max(1e-6, float(np.percentile(Sp, 99))))
    ax.set_xticks(range(0, N, 2)); ax.tick_params(axis="x", labelsize=7)
    ax.set_xlabel("anim frame (60 fps)")
    for n, s, e in phases(sheet, N):
        ax.axvline(s - 0.5, color="cyan", lw=1)
        if not blind:                                                 # boundaries only: the reviewer names the phases
            ax.text((s + e) / 2, -0.8, n, ha="center", va="bottom", color="black", fontsize=8)
    faf = sheet.get("faf", N - 1)
    if faf < N:
        ax.axvline(faf + 0.5, color="lime", lw=1, ls="--")
    for i, p in enumerate(ROWS):
        ax.plot(m["peaks"][p], i, "o", ms=4, mfc="none", mec="cyan")
    an = m["anticipation"]
    if not blind:                                                     # the blind reviewer must find the wind-up alone
        ax.plot(an["frame"], ROWS.index("head") - 0.4, "v", color="yellow", ms=6)
    ax.set_yticks(range(len(ROWS)), [r + (" (world)" if r in ("pelvis", "com") else "") + (" STILL" if r in m["near_still"] and not blind else "")
                                     for r in ROWS], fontsize=8)
    for lab, p in zip(ax.get_yticklabels(), ROWS):
        if p in m["near_still"]:
            lab.set_color("red")
    title = "speed per body part (cm/frame); o = speed peak" if blind else f"{take}: speed per body part (cm/frame; limbs/head/chest relative to the pelvis); o = speed peak, v = wind-up"
    if ref and not blind:
        fl = [f["check"] for f in ref["fails"]]
        title += f"\nvs reference {ref['ref_dir']}: {'FAIL' if ref['fail'] else 'pass'}"
        for i in range(0, len(fl), 3):
            title += ("; " if i == 0 else "\n") + "; ".join(fl[i:i + 3])
    ax.set_title(title, fontsize=9, pad=16)
    fig.colorbar(im, ax=ax, fraction=0.025, pad=0.01)
    fig.tight_layout()
    fig.savefig(path, dpi=110)
    plt.close(fig)


def gate(m, sheet):
    """Without a reference: warnings only (near-still limbs, no anticipation on a strong move)."""
    w = []
    for p in m["near_still"]:
        w.append({"check": "near-still limb while the action runs", "part": p, "share": m["action_share"][p]})
    if sheet.get("move") in ("tilt", "smash") and m["anticipation"]["ratio"] < TH.get("curves_anticip_min_abs", 0.05) \
            and max(m["anticipation"]["body_back_ratio"].values()) < TH.get("curves_anticip_min_abs", 0.05):
        w.append({"check": "no anticipation on a strong move", "ratio": m["anticipation"]["ratio"]})
    return w


def run(take, sheet_key=None, source=None, ref_dir=None, quiet=False, blind=False):
    load_th()
    sheet = sheet_of(sheet_key or take)
    if not sheet:
        raise SystemExit(f"no move sheet '{sheet_key or take}' in move_sheets.json")
    if source is None:
        source = "bones" if os.path.exists(os.path.join(HERE, "sheets", "raw", f"{take}_legbones.json")) else "model"
    P = load_bones(take) if source == "bones" else load_model(take, sheet_key)
    m = analyse(P, sheet)
    m["source"] = source
    m["warnings"] = gate(m, sheet)
    ref_dir = sheet.get("reference") if ref_dir is None else (None if ref_dir == "none" else ref_dir)
    ref = None
    if ref_dir:
        import motion_ref
        ref = motion_ref.compare(P, sheet, os.path.join(HERE, ref_dir), TH)
        m["reference"] = ref
    tag = take if source == "bones" else f"{take}_model"
    out = os.path.join(HERE, "sheets", f"{tag}_curves")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    txt = table(m, take) + ("\n" + "\n".join(f"warning: {w}" for w in m["warnings"]) if m["warnings"] else "")
    if ref:
        import motion_ref
        txt += "\n" + motion_ref.report(ref)
    open(out + ".txt", "w").write(txt + "\n")
    json.dump(m, open(out + ".json", "w"), indent=1)
    heatmap(m, sheet, take, out + ".png", ref)
    if ref:
        motion_ref.figure(ref, out + "_vsref.png", take)
    if blind:                                                         # for the blind review (iter_worker.md): no names
        heatmap(m, sheet, take, out + "_blind.png", None, blind=True)
    if not quiet:
        print(txt)
    print(f"curves {tag}: {'FAIL' if ref and ref['fail'] else 'ok'} -> {out}.png/.txt/.json")
    return m


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[1:] if a.startswith("--"))
    r = run(args[0], opt.get("sheet") or None, opt.get("source") or None, opt.get("ref") or None, "quiet" in opt, "blind" in opt)
    sys.exit(1 if r.get("reference", {}).get("fail") else 0)
