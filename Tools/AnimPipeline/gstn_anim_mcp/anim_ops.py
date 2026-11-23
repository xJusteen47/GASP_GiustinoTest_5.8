"""Core operations for gstn-anim-mcp (inspect, apply_intent, gate_check, capture_ab, rollback).

Pure Python 3.14 stdlib + optional subprocess calls to existing repo scripts
(runstart_pivot_gates.py, clip_check.py, Run-SparseAnimStep.ps1, runstart_ab_video.py).
"""
from __future__ import annotations

import json
import math
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

INTENT_BOUNDS: dict[str, tuple[float, float]] = {
    "com_shift_fwd_cm": (-40.0, 40.0),
    "torso_lean_pivot_deg": (-35.0, 45.0),
    "torso_bank_deg": (-35.0, 35.0),
    "shoulder_level_compensate": (-1.0, 1.0),
    "arm_back_arc": (-45.0, 45.0),
    "foot_pushoff_deg": (-45.0, 45.0),
    "w3_knee_drop": (-25.0, 25.0),
    "pelvis_fwd_cm": (-40.0, 40.0),
    "pelvis_drop_cm": (-15.0, 15.0),
    "spine_lean_pivot_deg": (-35.0, 45.0),
    "spine_lean_run_deg": (-35.0, 45.0),
    "head_lift_deg": (-25.0, 30.0),
    "knee_drop": (0.0, 0.9),
    "swing_lag_cm": (-15.0, 20.0),
    "catch_fwd_cm": (-15.0, 20.0),
    "step_narrow_cm": (-10.0, 15.0),
    "lat_widen_cm": (-10.0, 15.0),
    "pivot_heel_deg": (-30.0, 45.0),
    "clav_st_elev_deg": (-25.0, 35.0),
    "clav_pv_elev_deg": (-25.0, 35.0),
    "clav_open_deg": (-25.0, 30.0),
    "arm_st_tuck_in_deg": (-30.0, 45.0),
    "arm_pv_out_deg": (-30.0, 45.0),
    "arm_st_back_deg": (-45.0, 55.0),
    "arm_st_elbow_deg": (-30.0, 55.0),
    "arm_pv_fwd_deg": (-45.0, 55.0),
    "arm_pv_elbow_deg": (-30.0, 55.0),
    "arm_clr_deg": (-20.0, 30.0),
    "turn_deg": (0.0, 180.0),
}

DEFAULT_VIEWS = "az180,az90,game"


def _repo_root(root: str | Path | None = None) -> Path:
    if root:
        return Path(root).resolve()
    return Path(__file__).resolve().parents[3]


def _joints_path(root: Path, asset: str) -> Path:
    name = asset.rsplit("/", 1)[-1]
    return root / "Saved" / "AnimPipeline" / "runstart" / f"joints_{name}.json"


def _parse_frame_range(frame_range: Any, total: int) -> tuple[int, int]:
    if frame_range is None:
        return 0, max(0, total - 1)
    max_f = max(0, total - 1)
    if isinstance(frame_range, str):
        parts = [p.strip() for p in frame_range.replace("..", "-").split("-") if p.strip()]
        if len(parts) == 2:
            f0, f1 = int(parts[0]), int(parts[1])
            if f0 > f1:
                raise ValueError(f"Inverted frame_range: {frame_range!r}")
            a, b = max(0, f0), min(max_f, f1)
            if a > b:
                raise ValueError(f"Invalid frame_range {frame_range!r} for total={total}")
            return a, b
        if len(parts) == 1:
            f = max(0, min(max_f, int(parts[0])))
            return f, f
    if isinstance(frame_range, (list, tuple)) and len(frame_range) == 2:
        f0, f1 = int(frame_range[0]), int(frame_range[1])
        if f0 > f1:
            raise ValueError(f"Inverted frame_range: {frame_range!r}")
        a, b = max(0, f0), min(max_f, f1)
        if a > b:
            raise ValueError(f"Invalid frame_range {frame_range!r} for total={total}")
        return a, b
    raise ValueError(f"Invalid frame_range: {frame_range!r}")


def _dist(a: list[float], b: list[float]) -> float:
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def _seg_dist(p: list[float], a: list[float], b: list[float]) -> float:
    d = [b[i] - a[i] for i in range(3)]
    dd = sum(x * x for x in d)
    if dd < 1e-6:
        return _dist(p, a)
    t = max(0.0, min(1.0, sum((p[i] - a[i]) * d[i] for i in range(3)) / dd))
    proj = [a[i] + d[i] * t for i in range(3)]
    return _dist(p, proj)


def anim_inspect(
    asset: str,
    frame_range: Any = None,
    root: str | Path | None = None,
    gate_win: int = 18,
) -> dict[str, Any]:
    """Return readable joint metrics, lean/bank estimates, clearance, and gate status."""
    repo = _repo_root(root)
    jpath = _joints_path(repo, asset)
    if not jpath.exists():
        return {
            "ok": False,
            "error": f"joints file not found: {jpath}",
            "hint": "Run runstart_clip_joints_ue.py or anim_apply_intent first.",
        }

    data = json.loads(jpath.read_text(encoding="utf-8"))
    joints: dict[str, list[list[float]]] = data.get("joints", {})
    if not joints:
        return {"ok": False, "error": f"empty joints in {jpath}"}

    total = len(next(iter(joints.values())))
    if total <= 0:
        return {"ok": False, "error": f"zero frames in {jpath}"}
    f0, f1 = _parse_frame_range(frame_range, total)
    frames = range(f0, f1 + 1)

    def _sub3(a: list[float], b: list[float]) -> list[float]:
        return [a[i] - b[i] for i in range(3)]

    def _dot3(a: list[float], b: list[float]) -> float:
        return sum(a[i] * b[i] for i in range(3))

    def _norm3(v: list[float]) -> list[float]:
        n = math.sqrt(sum(x * x for x in v))
        return [x / n for x in v] if n > 1e-9 else [1.0, 0.0, 0.0]

    # Per-frame character axes across ALL frames (joints_*.json is Cascadeur Y-up
    # [x, y_up, z_fwd]).
    lat_vecs: list[list[float]] = []
    mid_hip: list[list[float]] = []
    for f in range(total):
        if all(k in joints and len(joints[k]) > f for k in ("thigh_MainPoint_l", "thigh_MainPoint_r")):
            tl = joints["thigh_MainPoint_l"][f]
            tr = joints["thigh_MainPoint_r"][f]
        else:
            tl = [0.0, 0.0, 0.0]
            tr = [0.0, 0.0, 0.0]
        lat = [tl[0] - tr[0], 0.0, tl[2] - tr[2]]
        lat_vecs.append(_norm3(lat) if math.sqrt(sum(x * x for x in lat)) > 1e-9 else [1.0, 0.0, 0.0])
        mid_hip.append([0.5 * (tl[i] + tr[i]) for i in range(3)])

    i_end = min(24, total - 1)
    i_start = min(14, max(0, i_end - 4))
    run_seg = _sub3(mid_hip[i_end], mid_hip[i_start])
    run_flat = [run_seg[0], 0.0, run_seg[2]]
    if math.sqrt(sum(x * x for x in run_flat)) > 1e-9:
        run_dir = _norm3(run_flat)
    else:
        fb = _sub3(mid_hip[-1], mid_hip[0])
        fb_flat = [fb[0], 0.0, fb[2]]
        run_dir = _norm3(fb_flat) if math.sqrt(sum(x * x for x in fb_flat)) > 1e-9 else [0.0, 0.0, 1.0]

    fwd_vecs: list[list[float]] = []
    for f in range(total):
        lat = lat_vecs[f]
        fwd_vecs.append(_norm3([lat[2], 0.0, -lat[0]]))

    # Backward continuity pass anchored against run_dir.
    ref_fwd = run_dir
    for f in range(total - 1, -1, -1):
        if _dot3(fwd_vecs[f], ref_fwd) < 0.0:
            fwd_vecs[f] = [-x for x in fwd_vecs[f]]
        ref_fwd = fwd_vecs[f]

    def _zig_max(wa: int, wb: int) -> dict[str, Any]:
        if wb - wa + 1 < 4:
            return {"value": 0.0, "joint": None, "frame": None, "per_joint": {}}

        def _chain_off(a_key: str, b_key: str, c_key: str) -> list[list[float]]:
            pa, pb, pc = joints.get(a_key, []), joints.get(b_key, []), joints.get(c_key, [])
            if min(len(pa), len(pb), len(pc)) <= wb:
                return []
            out: list[list[float]] = []
            for f in range(wa, wb + 1):
                ba = _sub3(pb[f], pa[f])
                ca = _sub3(pc[f], pa[f])
                ax = _norm3(ca)
                proj = _dot3(ba, ax)
                out.append([ba[i] - ax[i] * proj for i in range(3)])
            return out

        def _hand_rel(h_key: str, u_key: str) -> list[list[float]]:
            ph, pu = joints.get(h_key, []), joints.get(u_key, [])
            if min(len(ph), len(pu)) <= wb:
                return []
            return [_sub3(ph[f], pu[f]) for f in range(wa, wb + 1)]

        series_map: dict[str, list[list[float]]] = {
            "elbow_l": _chain_off("upperarm_MainPoint_l", "lowerarm_MainPoint_l", "hand_MainPoint_l"),
            "elbow_r": _chain_off("upperarm_MainPoint_r", "lowerarm_MainPoint_r", "hand_MainPoint_r"),
            "knee_l": _chain_off("thigh_MainPoint_l", "calf_MainPoint_l", "foot_MainPoint_l"),
            "knee_r": _chain_off("thigh_MainPoint_r", "calf_MainPoint_r", "foot_MainPoint_r"),
            "hand_l": _hand_rel("hand_MainPoint_l", "upperarm_MainPoint_l"),
            "hand_r": _hand_rel("hand_MainPoint_r", "upperarm_MainPoint_r"),
        }

        per_joint: dict[str, dict[str, Any]] = {}
        best_value = 0.0
        best_joint: str | None = None
        best_frame: int | None = None
        for label, seg in series_map.items():
            if len(seg) < 4:
                continue
            d2: list[list[float]] = [
                [seg[t + 2][i] - 2.0 * seg[t + 1][i] + seg[t][i] for i in range(3)]
                for t in range(len(seg) - 2)
            ]
            zigs: list[float] = []
            for t in range(len(d2) - 1):
                n0 = math.sqrt(sum(x * x for x in d2[t]))
                n1 = math.sqrt(sum(x * x for x in d2[t + 1]))
                if _dot3(d2[t + 1], d2[t]) < -0.5 * n1 * n0:
                    zigs.append(min(n1, n0))
                else:
                    zigs.append(0.0)
            if not zigs:
                continue
            m = max(zigs)
            m_frame = wa + zigs.index(m) + 1
            per_joint[label] = {"max": round(m, 2), "frame": m_frame}
            if m > best_value:
                best_value = m
                best_joint = label
                best_frame = m_frame
        return {"value": round(best_value, 2), "joint": best_joint, "frame": best_frame, "per_joint": per_joint}

    def _stats(vals: list[float]) -> dict[str, float] | None:
        if not vals:
            return None
        return {
            "min": round(min(vals), 2),
            "max": round(max(vals), 2),
            "mean": round(sum(vals) / len(vals), 2),
        }

    def _summarize_window(wa: int, wb: int) -> dict[str, Any]:
        if wa > wb:
            wa, wb = wb, wa
        lean: list[float] = []
        lean_local: list[float] = []
        bank: list[float] = []
        abd_l: list[float] = []
        abd_r: list[float] = []
        efl_l: list[float] = []
        efl_r: list[float] = []
        clear_l: list[float] = []
        clear_r: list[float] = []
        ankle_gaps: list[float] = []
        for f in range(wa, wb + 1):
            if all(k in joints and len(joints[k]) > f for k in (
                "thigh_MainPoint_l", "thigh_MainPoint_r",
                "upperarm_MainPoint_l", "upperarm_MainPoint_r",
            )):
                tl = joints["thigh_MainPoint_l"][f]
                tr = joints["thigh_MainPoint_r"][f]
                ul = joints["upperarm_MainPoint_l"][f]
                ur = joints["upperarm_MainPoint_r"][f]
                mh = [0.5 * (tl[i] + tr[i]) for i in range(3)]
                ms = [0.5 * (ul[i] + ur[i]) for i in range(3)]
                d = _sub3(ms, mh)
                lean.append(math.degrees(math.atan2(d[2], max(1e-4, d[1]))))
                lean_local.append(math.degrees(math.atan2(_dot3(d, fwd_vecs[f]), max(1e-4, d[1]))))
                horiz = math.hypot(ul[0] - ur[0], ul[2] - ur[2])
                bank.append(math.degrees(math.atan2(ul[1] - ur[1], max(1e-4, horiz))))

            for s, out_lst in (("l", clear_l), ("r", clear_r)):
                hk = f"hand_MainPoint_{s}"
                tk = f"thigh_MainPoint_{s}"
                ck = f"calf_MainPoint_{s}"
                if all(k in joints and len(joints[k]) > f for k in (hk, tk, ck)):
                    out_lst.append(_seg_dist(joints[hk][f], joints[tk][f], joints[ck][f]))

            if all(k in joints and len(joints[k]) > f for k in ("foot_MainPoint_l", "foot_MainPoint_r")):
                ankle_gaps.append(_dist(joints["foot_MainPoint_l"][f], joints["foot_MainPoint_r"][f]))

            for s, abd_lst, efl_lst in (("l", abd_l, efl_l), ("r", abd_r, efl_r)):
                uk = f"upperarm_MainPoint_{s}"
                lk = f"lowerarm_MainPoint_{s}"
                hk = f"hand_MainPoint_{s}"
                if all(k in joints and len(joints[k]) > f for k in (uk, lk, hk)):
                    up = joints[uk][f]
                    lo = joints[lk][f]
                    ha = joints[hk][f]
                    u = _norm3(_sub3(lo, up))
                    out_dir = lat_vecs[f] if s == "l" else [-x for x in lat_vecs[f]]
                    sagittal = math.hypot(_dot3(u, fwd_vecs[f]), u[1])
                    abd_lst.append(math.degrees(math.atan2(_dot3(u, out_dir), max(1e-6, sagittal))))
                    v1 = _norm3(_sub3(lo, up))
                    v2 = _norm3(_sub3(ha, lo))
                    efl_lst.append(math.degrees(math.acos(max(-1.0, min(1.0, _dot3(v1, v2))))))

        head_pitch: list[float] = []   # neck->head vs spine_05->neck, sagittal plane, + = chin up (head tilts back)
        head_fwd_cm: list[float] = []  # head forward of spine_05, character frame
        if all(k in joints and len(joints[k]) > wb for k in ("spine_05", "neck_01", "head")):
            for f in range(wa, wb + 1):
                d1 = _sub3(joints["neck_01"][f], joints["spine_05"][f])
                d2 = _sub3(joints["head"][f], joints["neck_01"][f])
                a1 = math.atan2(_dot3(d1, fwd_vecs[f]), d1[1])
                a2 = math.atan2(_dot3(d2, fwd_vecs[f]), d2[1])
                head_pitch.append(-math.degrees(a2 - a1))
                head_fwd_cm.append(_dot3(_sub3(joints["head"][f], joints["spine_05"][f]), fwd_vecs[f]))
        return {
            "frame_range": [wa, wb],
            "head_neck": {"head_pitch_rel_neck_deg": _stats(head_pitch), "head_fwd_of_spine5_cm": _stats(head_fwd_cm)},
            "torso_angles_deg": {
                "lean": _stats(lean),
                "lean_local_deg": _stats(lean_local),
                "bank": _stats(bank),
            },
            "arm_angles_deg": {
                "upperarm_abduction_deg": {"l": _stats(abd_l), "r": _stats(abd_r)},
                "elbow_flex_deg": {"l": _stats(efl_l), "r": _stats(efl_r)},
            },
            "clearance_cm": {
                "hand_l_thigh_l": _stats(clear_l),
                "hand_r_thigh_r": _stats(clear_r),
                "ankle_gap": _stats(ankle_gaps),
            },
            "jitter_max": _zig_max(wa, wb),
        }

    # joints_*.json is Y-up (x, y_up, z_fwd) from runstart_clip_joints_ue.py
    def _ys(key: str) -> list[float]:
        pts = joints.get(key, [])
        return [pts[f][1] for f in frames] if len(pts) > f1 else []

    hip_ys = [
        0.5 * (joints["thigh_MainPoint_l"][f][1] + joints["thigh_MainPoint_r"][f][1])
        for f in frames
        if (
            "thigh_MainPoint_l" in joints
            and "thigh_MainPoint_r" in joints
            and len(joints["thigh_MainPoint_l"]) > f
            and len(joints["thigh_MainPoint_r"]) > f
        )
    ]

    gates_py = repo / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_pivot_gates.py"
    gates_status = "UNKNOWN"
    gates_output = ""
    if gates_py.exists():
        proc = subprocess.run(
            [sys.executable, str(gates_py), "--json", str(jpath), str(gate_win)],
            capture_output=True,
            text=True,
        )
        gates_output = (proc.stdout or proc.stderr or "").strip()
        gates_status = "OK" if (proc.returncode == 0 and "GATES: FAIL" not in gates_output) else "FAIL"

    main_summary = _summarize_window(f0, f1)
    splits = {
        "f0_15": _summarize_window(0, min(15, total - 1)),
        "f16_35": _summarize_window(16, min(35, total - 1)) if total > 16 else None,
    }

    return {
        "ok": True,
        "asset": asset,
        "fps": data.get("fps", 30),
        "total_frames": total,
        "frame_range": [f0, f1],
        "heights_cm": {
            "hip": _stats(hip_ys),
            "foot_l": _stats(_ys("foot_MainPoint_l")),
            "foot_r": _stats(_ys("foot_MainPoint_r")),
            "elbow_l": _stats(_ys("lowerarm_MainPoint_l")),
            "elbow_r": _stats(_ys("lowerarm_MainPoint_r")),
            "shoulder_l": _stats(_ys("upperarm_MainPoint_l")),
            "shoulder_r": _stats(_ys("upperarm_MainPoint_r")),
        },
        "head_neck": main_summary["head_neck"],
        "torso_angles_deg": main_summary["torso_angles_deg"],
        "arm_angles_deg": main_summary["arm_angles_deg"],
        "jitter_max": main_summary["jitter_max"],
        "splits": splits,
        "clearance_cm": main_summary["clearance_cm"],
        "gates": {"status": gates_status, "detail": gates_output},
    }


def validate_intent(intent: dict[str, Any]) -> dict[str, Any]:
    """Validate and normalize semantic intent JSON for anim_apply_intent."""
    if not isinstance(intent, dict):
        raise ValueError("intent_json must be a JSON object")
    clean: dict[str, Any] = {}
    for k, v in intent.items():
        if k in INTENT_BOUNDS:
            lo, hi = INTENT_BOUNDS[k]
            if isinstance(v, bool):
                raise ValueError(f"{k} must be numeric, got bool")
            fv = float(v)
            if not math.isfinite(fv):
                raise ValueError(f"{k} must be finite, got {fv!r}")
            if not (lo <= fv <= hi):
                raise ValueError(f"{k}={fv} out of safe bounds [{lo}, {hi}]")
            clean[k] = fv
        elif k == "turn_side":
            norm = {"l": "L", "r": "R", "none": "none"}
            if not isinstance(v, str) or v.strip().lower() not in norm:
                raise ValueError(f"turn_side must be 'l', 'r' or 'none', got {v!r}")
            clean[k] = norm[v.strip().lower()]
        elif k == "start_from_frame":
            if isinstance(v, bool) or not isinstance(v, int):
                raise ValueError("start_from_frame must be an int")
            if not (0 <= v <= 300):
                raise ValueError(f"start_from_frame={v} out of range [0, 300]")
            clean[k] = v
        elif k == "smooth_pelvis_z":
            if not isinstance(v, bool):
                raise ValueError("smooth_pelvis_z must be a bool")
            clean[k] = v
        elif k in ("smooth_upper_passes", "smooth_leg_passes"):
            hi = 8 if k == "smooth_upper_passes" else 6
            if isinstance(v, bool) or not isinstance(v, int):
                raise ValueError(f"{k} must be an int")
            if not (0 <= v <= hi):
                raise ValueError(f"{k}={v} out of range [0, {hi}]")
            clean[k] = v
        elif k == "envelope":
            if v not in ("range", "turnaround"):
                raise ValueError("envelope must be 'range' or 'turnaround'")
            clean[k] = v
        elif k in ("handoff_smooth_range", "frame_range"):
            if not (isinstance(v, (list, tuple)) and len(v) == 2):
                raise ValueError(f"{k} must be [start_frame, end_frame]")
            a, b = int(v[0]), int(v[1])
            if a < 0 or b < a:
                raise ValueError(f"{k} invalid range: [{a}, {b}]")
            clean[k] = [a, b]
        else:
            raise ValueError(f"Unknown intent parameter: {k!r}")
    return clean


def _clip_frame_count(repo: Path, asset: str, ue59_dir: str = "") -> int | None:
    """Frame count of a clip from its joints dump (worktree first, then _ue59); None when no dump exists."""
    name = asset.rsplit("/", 1)[-1]
    cands = [_joints_path(repo, name)]
    if ue59_dir:
        cands.append(_joints_path(Path(ue59_dir), name))
    for jp in cands:
        if jp.exists():
            try:
                joints = json.loads(jp.read_text(encoding="utf-8")).get("joints", {})
                if joints:
                    return len(next(iter(joints.values())))
            except (OSError, ValueError):
                continue
    return None


def _snapshot_asset(repo: Path, asset: str) -> Path | None:
    name = asset.rsplit("/", 1)[-1]
    snap_dir = repo / "Saved" / "AnimPipeline" / "snapshots" / name
    snap_dir.mkdir(parents=True, exist_ok=True)
    copied = False
    jpath = _joints_path(repo, name)
    if jpath.exists():
        shutil.copy2(jpath, snap_dir / jpath.name)
        copied = True
    for ext in (".uasset", ".uexp"):
        for match in (repo / "Content").rglob(f"{name}{ext}"):
            shutil.copy2(match, snap_dir / match.name)
            copied = True
    meta = {"asset": asset, "name": name}
    (snap_dir / "snapshot_meta.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    return snap_dir if copied else snap_dir


def anim_apply_intent(
    src: str,
    dst: str,
    intent_json: dict[str, Any] | str,
    root: str | Path | None = None,
    dry_run: bool = False,
    worktree_dir: str | None = None,
    ue59_dir: str = r"C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59",
) -> dict[str, Any]:
    """Apply semantic intent parameters via the unified UE 5.9 geometric engine."""
    repo = _repo_root(root)
    raw = json.loads(intent_json) if isinstance(intent_json, str) else intent_json
    clean = validate_intent(raw)
    if clean.get("frame_range"):
        # frame_range is CLIP-LOCAL (0..n-1); a range outside the clip used to be a silent no-op
        n_frames = _clip_frame_count(repo, src, ue59_dir)
        if n_frames is not None:
            a, b = clean["frame_range"]
            if b > n_frames - 1:
                raise ValueError(
                    f"frame_range [{a}, {b}] is outside clip {src!r}: valid clip-local frames are 0..{n_frames - 1} "
                    f"(n={n_frames}); frame_range is clip-local, not the global v-frame of the capture"
                )

    snap_dir = _snapshot_asset(repo, dst)
    payload = {"src": src, "dst": dst, "intent": clean}
    intent_file = repo / "Saved" / "AnimPipeline" / "runstart" / f"intent_{dst.rsplit('/', 1)[-1]}.json"
    intent_file.parent.mkdir(parents=True, exist_ok=True)
    intent_file.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    if dry_run:
        return {
            "ok": True,
            "dry_run": True,
            "src": src,
            "dst": dst,
            "validated_intent": clean,
            "intent_file": str(intent_file),
            "snapshot_dir": str(snap_dir) if snap_dir else None,
        }

    wt = Path(worktree_dir).resolve() if worktree_dir else repo
    sparse_ps1 = wt / "Tools" / "CI" / "Run-SparseAnimStep.ps1"
    geom_rel = r"Tools\AnimPipeline\gstn_anim_mcp\geom_intent_ue.py"
    dst_name = dst.rsplit("/", 1)[-1]

    env = os.environ.copy()
    env["GSTN_INTENT_JSON"] = str(intent_file)
    env["GSTN_SAVE"] = "1"

    cmd = [
        "pwsh", "-NoProfile", "-File", str(sparse_ps1),
        "-WorktreeDir", str(wt),
        "-Script", geom_rel,
        "-Ue59Dir", ue59_dir,
        "-Assets", dst_name,
        "-Tag", f"intent_{dst_name[:24]}",
    ]
    proc = subprocess.run(cmd, env=env, capture_output=True, text=True)
    engine_error = [ln for ln in (proc.stdout or "").splitlines() if "GEOM_INTENT ERROR" in ln]
    if engine_error:
        return {"ok": False, "error": engine_error[0].strip(), "returncode": proc.returncode, "src": src, "dst": dst,
                "stdout": (proc.stdout or "")[-1500:]}
    return {
        "ok": proc.returncode == 0,
        "returncode": proc.returncode,
        "src": src,
        "dst": dst,
        "intent_file": str(intent_file),
        "snapshot_dir": str(snap_dir) if snap_dir else None,
        "stdout": (proc.stdout or "")[-1500:],
        "stderr": (proc.stderr or "")[-1000:],
    }


def _joint_conservation_check(ref_j: dict[str, Any], cur_j: dict[str, Any], max_loss: float = 0.15) -> dict[str, Any]:
    """Compare excursion of key chains in joints_*.json when FBX files are not present."""
    rj = ref_j.get("joints", {})
    cj = cur_j.get("joints", {})
    if not rj or not cj:
        return {"status": "SKIP", "reason": "missing joints"}

    def _span(pts: list[list[float]]) -> float:
        if len(pts) < 2:
            return 0.0
        lo = [min(p[i] for p in pts) for i in range(3)]
        hi = [max(p[i] for p in pts) for i in range(3)]
        return math.sqrt(sum((hi[i] - lo[i]) ** 2 for i in range(3)))

    failures: list[dict[str, Any]] = []
    checked = 0
    for k, rpts in rj.items():
        if k not in cj:
            continue
        rspan = _span(rpts)
        if rspan < 5.0:  # below floor (cm)
            continue
        checked += 1
        cspan = _span(cj[k])
        loss = (rspan - cspan) / rspan
        if loss > max_loss:
            failures.append({"joint": k, "ref_cm": round(rspan, 2), "cur_cm": round(cspan, 2), "loss_pct": round(loss * 100, 1)})

    return {
        "status": "FAIL" if failures else "OK",
        "max_loss_pct": round(max_loss * 100, 1),
        "checked_joints": checked,
        "failures": failures,
    }


def anim_gate_check(
    asset: str,
    ref_asset: str = "",
    root: str | Path | None = None,
    gate_win: int = 18,
    before_fbx: str = "",
    after_fbx: str = "",
) -> dict[str, Any]:
    """Run runstart_pivot_gates + conservation guard (<= 15%) + optional clipcheck."""
    repo = _repo_root(root)
    jpath = _joints_path(repo, asset)
    result: dict[str, Any] = {"ok": True, "asset": asset, "ref_asset": ref_asset}

    gates_py = repo / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_pivot_gates.py"
    if not jpath.exists():
        return {"ok": False, "error": f"joints file not found: {jpath}"}

    proc = subprocess.run(
        [sys.executable, str(gates_py), "--json", str(jpath), str(gate_win)],
        capture_output=True,
        text=True,
    )
    gates_out = (proc.stdout or proc.stderr or "").strip()
    gates_ok = proc.returncode == 0 and "GATES: FAIL" not in gates_out
    result["gates"] = {"status": "OK" if gates_ok else "FAIL", "output": gates_out}
    if not gates_ok:
        result["ok"] = False

    if before_fbx and after_fbx:
        cc_py = repo / "Tools" / "AnimPipeline" / "clipcheck" / "clip_check.py"
        rep_out = repo / "Saved" / "AnimPipeline" / "runstart" / f"conserve_{asset.rsplit('/', 1)[-1]}.json"
        cproc = subprocess.run(
            [sys.executable, str(cc_py), "--conserve", before_fbx, after_fbx, str(rep_out)],
            capture_output=True,
            text=True,
        )
        cons_ok = cproc.returncode == 0
        result["conservation"] = {"status": "OK" if cons_ok else "FAIL", "mode": "fbx", "output": (cproc.stdout or cproc.stderr or "").strip()}
        if not cons_ok:
            result["ok"] = False
    elif ref_asset:
        ref_jpath = _joints_path(repo, ref_asset)
        if ref_jpath.exists():
            cons = _joint_conservation_check(
                json.loads(ref_jpath.read_text(encoding="utf-8")),
                json.loads(jpath.read_text(encoding="utf-8")),
            )
            cons["mode"] = "joints_json"
            result["conservation"] = cons
            if cons["status"] == "FAIL":
                result["ok"] = False
        else:
            result["conservation"] = {"status": "SKIP", "reason": f"ref joints not found: {ref_jpath}"}
    else:
        result["conservation"] = {"status": "SKIP", "reason": "no ref_asset or FBX provided"}

    return result


def anim_capture_ab(
    variant_a: str | None = None,
    variant_b: str | None = None,
    seq: str = "mv_run_start_v5",
    window: str = "77-197",
    root: str | Path | None = None,
    views: str = DEFAULT_VIEWS,
    pivot_window: str = "84-116",
    pivot_step: int = 4,
    saved_dir: str = "CaptureSpeed",
    batch: str = "batch",
    run_gpu: bool = False,
    ue59_dir: str = r"C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59",
    pairs: list[dict[str, Any]] | None = None,
    variant_a_id: str = "",
    variant_b_id: str = "",
    clip_a: str = "",
    clip_b: str = "",
    assert_win: str = "85,120",
    cvars_a: str = "",
    cvars_b: str = "",
) -> dict[str, Any]:
    """Generate A/B videos (_real + _slow4x) and mandatory grids (az180, az90, game, grid_pivot)."""
    repo = _repo_root(root)

    # Enforce az180 first in the view list.
    views_list = [v.strip() for v in views.split(",") if v.strip()]
    if "az180" in views_list:
        views_list.remove("az180")
    views_list.insert(0, "az180")
    views = ",".join(views_list)

    ue59 = Path(ue59_dir).resolve()
    rs_dir = ue59 / "Tools" / "AnimPipeline" / "cards" / "runstart"
    batch_root = ue59 / "Saved" / saved_dir / batch

    # Normalize pairs (single-pair fallback uses the legacy positional args).
    norm_pairs: list[dict[str, Any]] = []
    if pairs is not None:
        if not isinstance(pairs, list) or not pairs:
            raise ValueError("pairs must be a non-empty list")
        for p in pairs:
            if not isinstance(p, dict):
                raise ValueError("each pair must be a dict")
            a = p.get("a")
            b = p.get("b")
            if not a or not b:
                raise ValueError("each pair requires 'a' and 'b'")
            p_seq = p.get("seq", seq)
            name = p.get("name") or p.get("label") or p_seq
            raw_assert = p.get("assert_win") or assert_win
            if raw_assert is None or raw_assert == "":
                raw_assert = "85,120"
            if isinstance(raw_assert, (list, tuple)):
                raw_assert = ",".join(str(x) for x in raw_assert)
            norm_pairs.append({
                "a": a,
                "b": b,
                "seq": p_seq,
                "name": name,
                "variant_a": p.get("variant_a") or p.get("variant_a_id") or a,
                "variant_b": p.get("variant_b") or p.get("variant_b_id") or b,
                "clip_a": p.get("clip_a", a),
                "clip_b": p.get("clip_b", b),
                "assert_win": str(raw_assert),
                "cvars_a": p.get("cvars_a", cvars_a),
                "cvars_b": p.get("cvars_b", cvars_b),
                "label_a": p.get("label_a") or a,
                "label_b": p.get("label_b") or b,
                "window": p.get("window", window),
                "pivot_window": p.get("pivot_window", pivot_window),
                "pivot_step": p.get("pivot_step", pivot_step),
                "out_subdir": p.get("out_subdir", f"ab_{name}"),
            })
    else:
        if not variant_a or not variant_b:
            raise ValueError("variant_a and variant_b are required when pairs is not provided")
        raw_single_assert = assert_win if (assert_win is not None and assert_win != "") else "85,120"
        if isinstance(raw_single_assert, (list, tuple)):
            raw_single_assert = ",".join(str(x) for x in raw_single_assert)
        norm_pairs.append({
            "a": variant_a,
            "b": variant_b,
            "seq": seq,
            "name": seq,
            "variant_a": variant_a_id or variant_a,
            "variant_b": variant_b_id or variant_b,
            "clip_a": clip_a or variant_a,
            "clip_b": clip_b or variant_b,
            "assert_win": str(raw_single_assert),
            "cvars_a": cvars_a,
            "cvars_b": cvars_b,
            "label_a": variant_a,
            "label_b": variant_b,
            "window": window,
            "pivot_window": pivot_window,
            "pivot_step": pivot_step,
            "out_subdir": "ab_out",
        })

    # Build deduplicated pipe-delimited runs.
    runs: list[str] = []
    seen_runs: dict[str, str] = {}
    for p in norm_pairs:
        win_csv = str(p["assert_win"]).replace("..", ",").replace("-", ",")
        for run_name, variant, clip, cvars in (
            (p["a"], p["variant_a"], p["clip_a"], p["cvars_a"]),
            (p["b"], p["variant_b"], p["clip_b"], p["cvars_b"]),
        ):
            entry = f"{run_name}|{variant}|{clip}|{p['seq']}|{win_csv}"
            if cvars:
                entry += f"|{cvars}"
            if run_name in seen_runs:
                if seen_runs[run_name] != entry:
                    raise ValueError(f"conflicting duplicate run_name: {run_name!r}")
                continue
            seen_runs[run_name] = entry
            runs.append(entry)

    if run_gpu:
        batch_ps1 = rs_dir / "runstart_batch_captures.ps1"
        runs_arg = ",".join("'" + r.replace("'", "''") + "'" for r in runs)
        ps_cmd = (
            f"& '{str(batch_ps1).replace(chr(39), chr(39) * 2)}' "
            f"-Runs @({runs_arg}) -SavedDir '{saved_dir}' -Batch '{batch}' -Views '{views}'"
        )
        gproc = subprocess.run(
            ["pwsh", "-NoProfile", "-Command", ps_cmd],
            capture_output=True,
            text=True,
        )
        if gproc.returncode != 0:
            return {"ok": False, "stage": "gpu_capture", "stdout": gproc.stdout, "stderr": gproc.stderr, "runs_sent": runs}

    ab_py = repo / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_ab_video.py"
    pair_results: list[dict[str, Any]] = []
    all_grids: list[str] = []
    all_videos: list[str] = []
    all_ok = True
    for p in norm_pairs:
        out_dir = batch_root / p["out_subdir"]
        cap_a = batch_root / p["a"]
        cap_b = batch_root / p["b"]
        if not (cap_a.exists() and cap_b.exists()):
            pair_results.append({
                "name": p["name"],
                "seq": p["seq"],
                "ok": False,
                "error": "Capture directories not found (set run_gpu=true to capture).",
                "expected": [str(cap_a), str(cap_b)],
            })
            all_ok = False
            continue

        af0, af1 = _parse_frame_range(p["window"], 10000)
        pf0, pf1 = _parse_frame_range(p["pivot_window"], 10000)
        assert_csv = str(p["assert_win"]).replace("..", ",").replace("-", ",")
        vproc = subprocess.run(
            [
                sys.executable, str(ab_py),
                str(cap_a), str(cap_b), str(out_dir),
                f"--a={p['label_a']}", f"--b={p['label_b']}",
                f"--seq={p['seq']}", f"--from={af0}", f"--to={af1}",
                f"--views={views}",
                f"--pivot-from={pf0}", f"--pivot-to={pf1}", f"--pivot-step={p['pivot_step']}",
            ],
            capture_output=True,
            text=True,
        )
        grids = sorted(str(x) for x in out_dir.glob("*.jpg"))
        videos = sorted(str(x) for x in out_dir.glob("*.mp4"))
        ok = vproc.returncode == 0
        all_ok = all_ok and ok
        all_grids.extend(grids)
        all_videos.extend(videos)
        assert_info: dict[str, Any] = {"window": assert_csv}
        for label_k, ap in (("a", cap_a / "assert.txt"), ("b", cap_b / "assert.txt"), ("batch", batch_root / "assert.txt")):
            if ap.exists():
                assert_info[label_k] = ap.read_text(encoding="utf-8", errors="replace").splitlines()[0]
        pair_results.append({
            "name": p["name"],
            "seq": p["seq"],
            "out_dir": str(out_dir),
            "ok": ok,
            "views": views_list,
            "grids": grids,
            "videos": videos,
            "assert": assert_info,
            "output": (vproc.stdout or vproc.stderr or "").strip(),
        })

    return {
        "ok": all_ok,
        "views": views_list,
        "pairs": pair_results,
        "grids": all_grids,
        "videos": all_videos,
        "runs_sent": runs,
    }


CHOOSER_REL = Path("Content") / "GSTN" / "Locomotion" / "StockBase" / "CHT_GSTN_StockMover_PoseMatch"
ROWS_SCRIPT_REL = "Tools\\AnimPipeline\\cards\\runstart\\runstart_generic_rows_ue.py"


def _sha(path: Path) -> str:
    import hashlib
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else ""


def _asset_files(root: Path, name: str) -> list[Path]:
    return [m for ext in (".uasset", ".uexp") for m in (root / "Content").rglob(f"{name}{ext}")]


def _variant_manifest(repo: Path, variant_id: int) -> Path:
    return repo / "Saved" / "AnimPipeline" / "variants" / f"variant_{variant_id}.json"


def anim_register_variant(
    asset: str,
    variant_id: int = 60,
    root: str | Path | None = None,
    worktree_dir: str | None = None,
    ue59_dir: str = r"C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59",
    allow_dirty_chooser: bool = False,
    dry_run: bool = False,
) -> dict[str, Any]:
    """Register a clip as run start variant `variant_id` (+ its Handoff sibling variant_id+10) in the CAPTURE project (_ue59).

    The tracked chooser CHT_GSTN_StockMover_PoseMatch is backed up byte-for-byte first; anim_unregister_variant puts it
    back (sha256 checked), so no tracked asset stays dirty. The clip is copied into _ue59 only for the capture.
    Refuses when the clip is missing, a variant manifest exists, or the chooser already has uncommitted (owner) changes
    (unless allow_dirty_chooser; the restore then returns the owner's exact bytes). Needs the _ue59 lock and no open editor.
    """
    repo = _repo_root(root)
    wt = Path(worktree_dir).resolve() if worktree_dir else repo
    ue59 = Path(ue59_dir).resolve()
    variant_id = int(variant_id)
    name = asset.rsplit("/", 1)[-1]
    if not name.replace("_", "").isalnum():
        raise ValueError(f"invalid asset name {name!r}")
    if not (30 <= variant_id <= 179):
        raise ValueError("variant_id must be 30..179 (0-29 are reviewed variants; N+10 is the Handoff sibling, so keep registered ids 20 apart)")
    files = _asset_files(wt, name)
    if not any(f.suffix == ".uasset" for f in files):
        return {"ok": False, "error": f"clip {name} not found under {wt / 'Content'} (run anim_apply_intent first)"}
    chooser = ue59 / (str(CHOOSER_REL) + ".uasset")
    chooser_uexp = ue59 / (str(CHOOSER_REL) + ".uexp")
    mf = _variant_manifest(wt, variant_id)
    if mf.exists():
        return {"ok": False, "error": f"variant {variant_id} already registered ({mf}); call anim_unregister_variant first"}
    dirty = subprocess.run(
        ["git", "-C", str(ue59), "status", "--porcelain", "--", str(CHOOSER_REL).replace("\\", "/") + ".uasset"],
        capture_output=True, text=True,
    ).stdout.strip()
    plan = {"asset": name, "variant_id": variant_id, "handoff_variant_id": variant_id + 10,
            "chooser_dirty_before": bool(dirty), "chooser_sha_before": _sha(chooser), "files": [f.name for f in files]}
    if dirty and not allow_dirty_chooser:
        return {"ok": False, "plan": plan,
                "error": "chooser CHT_GSTN_StockMover_PoseMatch has uncommitted (owner) changes in _ue59; pass "
                         "allow_dirty_chooser=true only with the owner's OK (the restore returns the same bytes)"}
    if dry_run:
        return {"ok": True, "dry_run": True, "plan": plan}

    bak = mf.parent / f"chooser_backup_{variant_id}"
    bak.mkdir(parents=True, exist_ok=True)
    for f in (chooser, chooser_uexp):
        if f.exists():
            shutil.copy2(f, bak / f.name)
    copied: list[str] = []
    for f in files:
        rel = f.relative_to(wt)
        dst = ue59 / rel
        if dst.exists():
            return {"ok": False, "plan": plan, "error": f"{dst} already exists in _ue59; not overwriting"}
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(f, dst)
        copied.append(str(rel))
    manifest = dict(plan, backup_dir=str(bak), copied=copied, ue59=str(ue59))
    mf.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    env = os.environ.copy()
    env.update({"GSTN_SAVE": "1", "RSG_ASSET": f"/Game/GSTN/Animation/RunStart/{name}", "RSG_K": str(variant_id),
                "RSG_REPORT": f"register_variant_{variant_id}"})
    cmd = ["pwsh", "-NoProfile", "-File", str(wt / "Tools" / "CI" / "Run-SparseAnimStep.ps1"), "-WorktreeDir", str(wt),
           "-Script", ROWS_SCRIPT_REL, "-Ue59Dir", str(ue59), "-Tag", f"register_{variant_id}", "-KeepUe59Content"]
    proc = subprocess.run(cmd, env=env, capture_output=True, text=True)
    manifest["chooser_sha_after"] = _sha(chooser)
    mf.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    ok = proc.returncode == 0 and manifest["chooser_sha_after"] != plan["chooser_sha_before"]
    return {"ok": ok, "returncode": proc.returncode, "plan": manifest,
            "next": f"capture with gstn.RunStart.Variant {variant_id} (variant_a_id/variant_b_id in anim_capture_ab), "
                    "then ALWAYS call anim_unregister_variant to restore the chooser",
            "stdout": (proc.stdout or "")[-800:], "stderr": (proc.stderr or "")[-500:]}


def anim_unregister_variant(
    variant_id: int = 60,
    root: str | Path | None = None,
    worktree_dir: str | None = None,
) -> dict[str, Any]:
    """Undo anim_register_variant: restore the chooser bytes (sha256 verified) and delete the clip copies from _ue59."""
    repo = _repo_root(root)
    wt = Path(worktree_dir).resolve() if worktree_dir else repo
    mf = _variant_manifest(wt, int(variant_id))
    if not mf.exists():
        return {"ok": False, "error": f"no registration manifest for variant {variant_id}"}
    m = json.loads(mf.read_text(encoding="utf-8"))
    ue59 = Path(m["ue59"])
    bak = Path(m["backup_dir"])
    for f in (ue59 / (str(CHOOSER_REL) + ".uasset"), ue59 / (str(CHOOSER_REL) + ".uexp")):
        b = bak / f.name
        if b.exists():
            shutil.copy2(b, f)
    for rel in m["copied"]:
        p = ue59 / rel
        if p.exists():
            p.unlink()
    ok = _sha(ue59 / (str(CHOOSER_REL) + ".uasset")) == m["chooser_sha_before"]
    if ok:
        mf.unlink()
    return {"ok": ok, "chooser_restored_sha_ok": ok, "removed": m["copied"]}


def anim_rollback(asset: str, root: str | Path | None = None) -> dict[str, Any]:
    """Restore the latest snapshot of an asset (joints + uasset/uexp) when gates or review fail."""
    repo = _repo_root(root)
    name = asset.rsplit("/", 1)[-1]
    snap_dir = repo / "Saved" / "AnimPipeline" / "snapshots" / name
    if not snap_dir.exists():
        return {"ok": False, "error": f"No snapshot found for {name} in {snap_dir}"}

    restored: list[str] = []
    j_snap = snap_dir / f"joints_{name}.json"
    if j_snap.exists():
        dst_j = _joints_path(repo, name)
        dst_j.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(j_snap, dst_j)
        restored.append(str(dst_j))

    for ext in (".uasset", ".uexp"):
        s = snap_dir / f"{name}{ext}"
        if s.exists():
            matches = list((repo / "Content").rglob(f"{name}{ext}"))
            for m in matches:
                shutil.copy2(s, m)
                restored.append(str(m))

    return {
        "ok": bool(restored),
        "asset": asset,
        "restored_from": str(snap_dir),
        "restored_files": restored,
    }
