"""Our move against a reference's 2D keypoints, from the same side camera (called by motion_curves.py).

Reference folder refs/<name>/:
  keypoints_auto.json   rtmlib COCO-17 keypoints per image (ref_keypoints.py), pixels, y down;
  keypoints_auto_fix.json (optional) manual corrections: {"<image>": {"swap": ["arms"|"legs"], "set": {"wrist_l": [x, y]}}};
  reference.json        {"keypoints": "keypoints_auto.json", "facing": "right",
                         "stills": [{"image": "1_first.png", "phase": "first"}, ...]}        (key poses: first, anticipation,
                                                                                              active, recovery, last)
                     or {"keypoints": ..., "dense": {"prefix": "MarioJab1.gif#", "count": 19, "anim_per_ref": 0.96},
                         "phases": {"first": 0, "active": 1}}                                 (every frame of a GIF)
Both sides become the same 2D side view: x along the attack (horizontal), y up; our bones are projected, the reference's
labels are used as they are (left = left: check the overlays once). Each pose is scaled by its own body length (torso +
support thigh + support shin, measured in that pose: the stills are cropped and scaled differently).

Our frame for each still: first = 0, last = FAF, active = the effector's farthest point in the contact window,
anticipation = the frame before contact with the most wind-up (effector and body opposite the strike), recovery = the best
pose match between the hold and the FAF. A dense reference maps GIF frame i to anim frame round(i * anim_per_ref).

Measures, per limb group (arm_l, arm_r, leg_l, leg_r, torso):
  pose      mean angle between matching 2D segments (upper arm, forearm, thigh, shin, torso), per still and overall;
  amp       how far the limb's joints travel from the first pose (relative to the mid-hip, body lengths): ours / ref;
  anticip   the effector's and the body's wind-up opposite the strike, as a ratio of the strike (ours vs ref);
  arm dir   each wrist's travel from the anticipation to the active pose along the strike: with (+) or counter (-);
  dtw       DTW distance (dtaidistance when installed) between our joint curve (every frame to the FAF) and the
            reference's sequence, mean step in body lengths.
Gate (motion_thresholds.json curves_ref_*): a limb whose amplitude is under curves_ref_amp_ratio_min of a reference
limb that moves, a missing anticipation (under curves_ref_anticip_frac of the reference's), or a wrist going the other
way from the reference's (when both travel at least curves_ref_dir_min) fails.
"""
import json
import os

import numpy as np

SEGS = {"ua_l": ("shoulder_l", "elbow_l"), "fa_l": ("elbow_l", "wrist_l"), "ua_r": ("shoulder_r", "elbow_r"),
        "fa_r": ("elbow_r", "wrist_r"), "th_l": ("hip_l", "knee_l"), "sh_l": ("knee_l", "ankle_l"),
        "th_r": ("hip_r", "knee_r"), "sh_r": ("knee_r", "ankle_r"), "torso": ("mid_hip", "mid_sh")}
GROUPS = {"arm_l": ["ua_l", "fa_l"], "arm_r": ["ua_r", "fa_r"], "leg_l": ["th_l", "sh_l"], "leg_r": ["th_r", "sh_r"], "torso": ["torso"]}
ENDS = {"arm_l": ["wrist_l", "elbow_l"], "arm_r": ["wrist_r", "elbow_r"], "leg_l": ["ankle_l", "knee_l"],
        "leg_r": ["ankle_r", "knee_r"], "torso": ["nose", "mid_sh"]}
OURS = {"shoulder_l": "shoulder_l", "shoulder_r": "shoulder_r", "elbow_l": "elbow_l", "elbow_r": "elbow_r", "wrist_l": "hand_l",
        "wrist_r": "hand_r", "hip_l": "hip_l", "hip_r": "hip_r", "knee_l": "knee_l", "knee_r": "knee_r", "ankle_l": "foot_l",
        "ankle_r": "foot_r", "nose": "head"}
MIN_SCORE = 0.3


def _complete(p):
    if all(k in p for k in ("hip_l", "hip_r")):
        p["mid_hip"] = 0.5 * (p["hip_l"] + p["hip_r"])
    if all(k in p for k in ("shoulder_l", "shoulder_r")):
        p["mid_sh"] = 0.5 * (p["shoulder_l"] + p["shoulder_r"])
    return p


def _scale(p, sup):
    """Body length of one pose: torso + support thigh + support shin; 0 when any of them is missing (the caller then uses
    the reference's median, never a partial sum, which would blow the pose up)."""
    parts = [("mid_hip", "mid_sh"), (f"hip_{sup}", f"knee_{sup}"), (f"knee_{sup}", f"ankle_{sup}")]
    if not all(a in p and b in p for a, b in parts):
        return 0.0
    return sum(float(np.linalg.norm(p[a] - p[b])) for a, b in parts)


def load_ref(ref_dir):
    R = json.load(open(os.path.join(ref_dir, "reference.json")))
    K = json.load(open(os.path.join(ref_dir, R.get("keypoints", "keypoints_auto.json"))))
    fixp = os.path.join(ref_dir, os.path.splitext(R.get("keypoints", "keypoints_auto.json"))[0] + "_fix.json")
    fix = json.load(open(fixp)) if os.path.exists(fixp) else {}
    flip = -1.0 if R.get("facing", "right") == "left" else 1.0

    def pose(img):
        d = K["images"].get(img)
        if not d:
            return None
        pts = {n: np.array([flip * v[0], -v[1]], float) for n, v in d["points"].items() if d["score"][n] >= MIN_SCORE}
        f = fix.get(img, {})
        for grp in f.get("swap", []):
            for j in (("shoulder", "elbow", "wrist") if grp == "arms" else ("hip", "knee", "ankle")):
                a, b = pts.pop(f"{j}_l", None), pts.pop(f"{j}_r", None)
                if b is not None: pts[f"{j}_l"] = b
                if a is not None: pts[f"{j}_r"] = a
        for n, v in f.get("set", {}).items():
            pts[n] = np.array([flip * v[0], -v[1]], float)
        return _complete(pts)

    if "stills" in R:
        seq = [(s["phase"], s["image"], pose(s["image"])) for s in R["stills"]]
    else:
        dn = R["dense"]
        seq = [(None, f"{dn['prefix']}{i}", pose(f"{dn['prefix']}{i}")) for i in range(dn["count"])]
        for ph, i in R.get("phases", {}).items():
            seq[i] = (ph, seq[i][1], seq[i][2])
    return R, seq


def our_pose(P, f, a):
    pts = {k: np.array([float(P[v][f] @ a), float(P[v][f][2])]) for k, v in OURS.items()}
    return _complete(pts)


def norm(p, origin, sc):
    return {k: (v - origin) / sc for k, v in p.items()}


def seg_dir(p, s):
    a, b = SEGS[s]
    if a not in p or b not in p:
        return None
    d = p[b] - p[a]
    n = np.linalg.norm(d)
    return d / n if n > 1e-9 else None


def pose_dist(p, q):
    """Mean segment angle per group (deg), None where a segment is missing on either side."""
    out = {}
    for g, ss in GROUPS.items():
        angs = []
        for s in ss:
            u, v = seg_dir(p, s), seg_dir(q, s)
            if u is not None and v is not None:
                angs.append(float(np.degrees(np.arccos(np.clip(u @ v, -1, 1)))))
        out[g] = round(float(np.mean(angs)), 1) if angs else None
    return out


def _median3(refs, i, col):
    """Joint-wise median over frames i-1..i+1 (only where the joint was seen): the far-side arm of a side view flickers."""
    if refs[i][col] is None:
        return None
    out = {}
    for k in refs[i][col]:
        vals = [refs[j][col][k] for j in (i - 1, i, i + 1) if 0 <= j < len(refs) and refs[j][col] is not None and k in refs[j][col]]
        out[k] = np.median(np.array(vals), axis=0)
    return out


def _dtw(s1, s2):
    """Mean step cost along the DTW path (body lengths)."""
    try:
        from dtaidistance import dtw_ndim
        path = dtw_ndim.warping_path(np.ascontiguousarray(s1, dtype=np.double), np.ascontiguousarray(s2, dtype=np.double))
    except Exception:
        n, m = len(s1), len(s2)
        C = np.full((n + 1, m + 1), np.inf); C[0, 0] = 0
        for i in range(1, n + 1):
            for j in range(1, m + 1):
                C[i, j] = np.linalg.norm(s1[i - 1] - s2[j - 1]) + min(C[i - 1, j], C[i, j - 1], C[i - 1, j - 1])
        i, j, path = n, m, []
        while i > 0 and j > 0:
            path.append((i - 1, j - 1))
            k = int(np.argmin([C[i - 1, j - 1], C[i - 1, j], C[i, j - 1]]))
            i, j = (i - 1, j - 1) if k == 0 else (i - 1, j) if k == 1 else (i, j - 1)
        path.reverse()
    return float(np.mean([np.linalg.norm(s1[i] - s2[j]) for i, j in path]))


def compare(P, sheet, ref_dir, TH):
    R, seq = load_ref(ref_dir)
    N = len(P["pelvis"])
    c0, c1 = sheet["contact"]
    faf = min(sheet.get("faf", N - 1), N - 1)
    hw = sheet.get("hold_window", [c0, c1])
    kick = str(sheet.get("strike_limb", "")).startswith("leg")
    side = str(sheet.get("strike_side", "l"))[-1] if kick else ("r" if str(sheet.get("effector", "")).endswith("_r") else "l")
    sup = "r" if side == "l" else "l"
    eff = f"ankle_{side}" if kick else f"wrist_{side}"
    E = P["ball_" + side] if kick else P["hand_" + side]
    a = E[c0] - E[0]; a[2] = 0; a = a / max(np.linalg.norm(a), 1e-9)

    ours_raw = [our_pose(P, f, a) for f in range(N)]
    sc_o = [_scale(p, sup) for p in ours_raw]
    O_hip = [norm(p, p["mid_hip"], s) for p, s in zip(ours_raw, sc_o)]      # limb motion relative to the body
    O_gnd = [norm(p, p[f"ankle_{sup}"], s) for p, s in zip(ours_raw, sc_o)]  # body motion over the support foot

    valid = [_scale(p, sup) for _, _, p in seq if p]
    med = float(np.median([v for v in valid if v > 0])) if any(v > 0 for v in valid) else 0.0

    def ref_norms(p):
        s = _scale(p, sup) or med                                          # a GIF keeps its crop: the median is safe there
        if s < 1e-6 or "mid_hip" not in p or eff not in p:                 # too few confident points: skip this pose
            return None, None
        return norm(p, p["mid_hip"], s), (norm(p, p[f"ankle_{sup}"], s) if f"ankle_{sup}" in p else None)

    refs = [(ph, img, *ref_norms(p)) if p else (ph, img, None, None) for ph, img, p in seq]
    if "dense" in R:                                                       # GIF: 3-frame median per joint against estimator
        refs = [(ph, img, _median3(refs, i, 2), _median3(refs, i, 3)) for i, (ph, img, *_ ) in enumerate(refs)]   # jitter
    phase_idx = {ph: i for i, (ph, *_ ) in enumerate(refs) if ph}

    # ----- our frame for each reference pose
    def wind(pg, pg0, S):
        """Wind-up opposite the strike direction S: effector + body (mid-hip, mid-shoulder, head) over the support foot."""
        tot = 0.0
        for k in (eff, "mid_hip", "mid_sh", "nose"):
            if k in pg and k in pg0:
                tot += max(0.0, -float((pg[k] - pg0[k]) @ S))
        return tot

    k_act = c0 + int(np.argmax([(E[f] - E[0]) @ a for f in range(c0, c1 + 1)]))
    S_o = O_gnd[k_act][eff] - O_gnd[0][eff]; S_o = S_o / max(np.linalg.norm(S_o), 1e-9)
    k_ant = max(range(0, max(c0, 1)), key=lambda f: wind(O_gnd[f], O_gnd[0], S_o))
    mapping = []
    if "dense" in R:
        for i in range(len(refs)):
            mapping.append(min(N - 1, int(round(i * R["dense"].get("anim_per_ref", 0.96)))))
    else:
        for ph, img, rh, rg in refs:
            if ph == "first": mapping.append(0)
            elif ph == "last": mapping.append(faf)
            elif ph == "active": mapping.append(k_act)
            elif ph == "anticipation": mapping.append(k_ant)
            else:                                                                # recovery: best match after the hold
                lo = min(max(c1, hw[1]) + 1, faf)
                mapping.append(min(range(lo, faf + 1), key=lambda f: np.nanmean(
                    [v for v in pose_dist(O_hip[f], rh).values() if v is not None]) if rh else 0))

    # ----- pose distance
    per = []
    for (ph, img, rh, rg), f in zip(refs, mapping):
        if rh is None:
            continue
        per.append({"ref": img, "phase": ph, "frame": f, "deg": pose_dist(O_hip[f], rh)})
    pose_mean = {g: round(float(np.mean([x["deg"][g] for x in per if x["deg"][g] is not None])), 1)
                 if any(x["deg"][g] is not None for x in per) else None for g in GROUPS}

    # ----- amplitude (relative to the mid-hip, from the first pose)
    i0 = next(i for i, r in enumerate(refs) if r[2] is not None)
    amp = {}
    for g, ends in ENDS.items():
        rv, ov = [], []
        for k in ends:
            ra = [float(np.linalg.norm(r[2][k] - refs[i0][2][k])) for r in refs if r[2] is not None and k in r[2] and k in refs[i0][2]]
            oa = [float(np.linalg.norm(O_hip[f][k] - O_hip[mapping[i0]][k])) for r, f in zip(refs, mapping) if r[2] is not None]
            if ra:
                rv.append(max(ra)); ov.append(max(oa))
        if rv:
            r_, o_ = float(np.mean(rv)), float(np.mean(ov))
            amp[g] = {"ref": round(r_, 3), "ours": round(o_, 3), "ratio": round(o_ / r_, 2) if r_ > 1e-6 else None}

    # ----- anticipation and arm direction (needs first, anticipation and active poses; a dense ref uses its phases)
    ant = {}
    armdir = {}
    if all(p in phase_idx and refs[phase_idx[p]][3] is not None for p in ("first", "active")):
        def anticip(g0, ga, gx):
            S = gx[eff] - g0[eff]; sl = float(np.linalg.norm(S)); S = S / max(sl, 1e-9)
            strike = float(np.linalg.norm(gx[eff] - ga[eff])) or 1e-9
            body = max([max(0.0, -float((ga[k] - g0[k]) @ S)) for k in ("mid_hip", "mid_sh", "nose") if k in ga and k in g0] + [0.0])
            return ({"effector": round(max(0.0, -float((ga[eff] - g0[eff]) @ S)) / strike, 3), "body": round(body / strike, 3)}, S)
        r0, rx = refs[phase_idx["first"]][3], refs[phase_idx["active"]][3]
        ra = refs[phase_idx["anticipation"]][3] if "anticipation" in phase_idx else None
        if ra is None:                                                     # dense ref: its own most-wound frame before active
            cands = [r[3] for r in refs[:phase_idx["active"]] if r[3] is not None and eff in r[3]]
            S_r = rx[eff] - r0[eff]; S_r = S_r / max(np.linalg.norm(S_r), 1e-9)
            ra = max(cands, key=lambda g: wind(g, r0, S_r)) if cands else r0
        ant_r, S_r = anticip(r0, ra, rx)
        ant_o, S_o2 = anticip(O_gnd[0], O_gnd[k_ant], O_gnd[k_act])
        ant = {"ref": ant_r, "ours": ant_o, "our_frame": k_ant}
        for s in "lr":
            k = f"wrist_{s}"
            if k == eff or k not in ra or k not in rx:
                continue
            rh_a = refs[phase_idx.get("anticipation", phase_idx["first"])][2]; rh_x = refs[phase_idx["active"]][2]
            if rh_a is None or rh_x is None or k not in rh_a or k not in rh_x:
                continue
            dr = float((rh_x[k] - rh_a[k]) @ S_r)
            do = float((O_hip[k_act][k] - O_hip[k_ant][k]) @ S_o2)
            armdir[k] = {"ref": round(dr, 3), "ours": round(do, 3)}

    # ----- DTW per limb end (our curve to the FAF vs the reference sequence, mid-hip frame)
    dtw = {}
    for g, ends in ENDS.items():
        k = ends[0]
        rs = np.array([r[2][k] for r in refs if r[2] is not None and k in r[2]])
        if len(rs) >= 2:
            os_ = np.array([O_hip[f][k] for f in range(0, faf + 1)])
            dtw[g] = round(_dtw(os_, rs), 3)

    # ----- gate
    fails = []
    for g in ("arm_l", "arm_r", "leg_l", "leg_r"):
        x = amp.get(g)
        if x and x["ref"] >= TH.get("curves_ref_amp_min_abs", 0.15) and x["ratio"] is not None \
                and x["ratio"] < TH.get("curves_ref_amp_ratio_min", 0.6):
            fails.append({"check": f"{g} amplitude {x['ratio']}x the reference", "group": g, **x})
    if ant:
        for k in ("effector", "body"):
            r_, o_ = ant["ref"][k], ant["ours"][k]
            if r_ >= TH.get("curves_ref_anticip_min", 0.1) and o_ < TH.get("curves_ref_anticip_frac", 0.5) * r_:
                fails.append({"check": f"missing anticipation ({k} {o_} vs ref {r_})", "part": k, "ref": r_, "ours": o_})
    dmin = TH.get("curves_ref_dir_min", 0.15)
    for k, d in armdir.items():
        if abs(d["ref"]) >= dmin and abs(d["ours"]) >= dmin and np.sign(d["ref"]) != np.sign(d["ours"]):
            fails.append({"check": f"{k} goes {'with' if d['ours'] > 0 else 'against'} the strike, reference "
                                   f"{'with' if d['ref'] > 0 else 'against'}", "part": k, **d})
        elif abs(d["ref"]) >= dmin and abs(d["ours"]) < dmin / 3:
            fails.append({"check": f"{k} still between anticipation and strike (ref travels {d['ref']})", "part": k, **d})
    pmax = TH.get("curves_ref_pose_max_deg")
    if pmax:
        for g, v in pose_mean.items():
            if v is not None and v > pmax:
                fails.append({"check": f"{g} pose {v} deg from the reference (max {pmax})", "group": g, "deg": v})
    return {"ref_dir": os.path.basename(ref_dir.rstrip("/\\")), "mapping": mapping, "per_pose": per, "pose_mean_deg": pose_mean,
            "amplitude": amp, "anticipation": ant, "arm_dir": armdir, "dtw": dtw, "fails": fails, "fail": bool(fails),
            "_draw": {"ours": [{k: v.tolist() for k, v in O_hip[f].items()} for f in mapping],
                      "ref": [{k: v.tolist() for k, v in r[2].items()} if r[2] else None for r in refs],
                      "labels": [r[1] for r in refs]}}


def report(r):
    L = [f"reference {r['ref_dir']}: {'FAIL' if r['fail'] else 'pass'} (our frames {r['mapping']})",
         "  pose distance, mean deg: " + ", ".join(f"{g} {v}" for g, v in r["pose_mean_deg"].items())]
    for x in r["per_pose"]:
        if x["phase"]:
            L.append(f"    {x['phase']:<13} f{x['frame']:<3} " + ", ".join(f"{g} {v}" for g, v in x["deg"].items()))
    L.append("  amplitude ours/ref (body lengths): " + ", ".join(f"{g} {x['ours']}/{x['ref']} = {x['ratio']}" for g, x in r["amplitude"].items()))
    if r["anticipation"]:
        an = r["anticipation"]
        L.append(f"  anticipation / strike: ours {an['ours']} (frame {an['our_frame']}) vs ref {an['ref']}")
    if r["arm_dir"]:
        L.append("  wrist travel along the strike, anticipation -> active (+ with, - counter): "
                 + ", ".join(f"{k} ours {d['ours']:+.2f} ref {d['ref']:+.2f}" for k, d in r["arm_dir"].items()))
    L.append("  DTW mean step (body lengths): " + ", ".join(f"{g} {v}" for g, v in r["dtw"].items()))
    for f in r["fails"]:
        L.append(f"  FAIL {f['check']}")
    return "\n".join(L)


def figure(r, path, take):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    D = r["_draw"]
    idx = [i for i, x in enumerate(D["ref"]) if x]
    if len(idx) > 8:
        idx = [idx[int(round(j))] for j in np.linspace(0, len(idx) - 1, 8)]
    fig, axs = plt.subplots(1, len(idx), figsize=(2.4 * len(idx), 3.4), squeeze=False)
    col = {"l": "tab:orange", "r": "tab:blue", "torso": "k"}
    for ax, i in zip(axs[0], idx):
        for who, ls, alpha in (("ref", "-", 0.35), ("ours", "-", 1.0)):
            p = {k: np.array(v) for k, v in D[who][i].items()}
            for s, (a, b) in SEGS.items():
                if a in p and b in p:
                    c = col["torso"] if s == "torso" else col[s[-1]]
                    ax.plot([p[a][0], p[b][0]], [p[a][1], p[b][1]], ls, color=c, lw=4 if who == "ref" else 1.6, alpha=alpha)
        ax.set_aspect("equal"); ax.set_xlim(-0.9, 0.9); ax.set_ylim(-1.1, 0.8); ax.set_xticks([]); ax.set_yticks([])
        ph = next((x["phase"] for x in r["per_pose"] if x["ref"] == D["labels"][i]), None)
        ax.set_title(f"{ph or D['labels'][i]}\nours f{r['mapping'][i]}", fontsize=8)
    fig.suptitle(f"{take} (thin) vs {r['ref_dir']} (thick, faded); orange = left, blue = right; side view, attack to the right",
                 fontsize=8)
    fig.tight_layout()
    fig.savefig(path, dpi=100)
    plt.close(fig)
