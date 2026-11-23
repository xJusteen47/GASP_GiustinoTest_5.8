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
    if isinstance(frame_range, str):
        parts = [p.strip() for p in frame_range.replace("..", "-").split("-") if p.strip()]
        if len(parts) == 2:
            f0, f1 = int(parts[0]), int(parts[1])
            return max(0, f0), min(max(0, total - 1), f1)
        if len(parts) == 1:
            f = max(0, min(max(0, total - 1), int(parts[0])))
            return f, f
    if isinstance(frame_range, (list, tuple)) and len(frame_range) == 2:
        return max(0, int(frame_range[0])), min(max(0, total - 1), int(frame_range[1]))
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
    gate_win: int = 40,
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
    f0, f1 = _parse_frame_range(frame_range, total)
    frames = range(f0, f1 + 1)

    # joints_*.json is Y-up (x, y_up, z_fwd) from runstart_clip_joints_ue.py
    def _ys(key: str) -> list[float]:
        pts = joints.get(key, [])
        return [pts[f][1] for f in frames] if len(pts) > f1 else []

    hip_ys = [
        0.5 * (joints["thigh_MainPoint_l"][f][1] + joints["thigh_MainPoint_r"][f][1])
        for f in frames
        if "thigh_MainPoint_l" in joints and "thigh_MainPoint_r" in joints
    ]

    lean_degs: list[float] = []
    bank_degs: list[float] = []
    clear_l: list[float] = []
    clear_r: list[float] = []
    ankle_gaps: list[float] = []

    for f in frames:
        if all(k in joints for k in ("thigh_MainPoint_l", "thigh_MainPoint_r", "upperarm_MainPoint_l", "upperarm_MainPoint_r")):
            mid_hip = [0.5 * (joints["thigh_MainPoint_l"][f][i] + joints["thigh_MainPoint_r"][f][i]) for i in range(3)]
            mid_sh = [0.5 * (joints["upperarm_MainPoint_l"][f][i] + joints["upperarm_MainPoint_r"][f][i]) for i in range(3)]
            dy = max(1e-4, mid_sh[1] - mid_hip[1])
            dz = mid_sh[2] - mid_hip[2]
            lean_degs.append(math.degrees(math.atan2(dz, dy)))

            sh_l = joints["upperarm_MainPoint_l"][f]
            sh_r = joints["upperarm_MainPoint_r"][f]
            horiz = math.hypot(sh_l[0] - sh_r[0], sh_l[2] - sh_r[2])
            bank_degs.append(math.degrees(math.atan2(sh_l[1] - sh_r[1], max(1e-4, horiz))))

        for s, out_lst in (("l", clear_l), ("r", clear_r)):
            hk = f"hand_MainPoint_{s}"
            tk = f"thigh_MainPoint_{s}"
            ck = f"calf_MainPoint_{s}"
            if hk in joints and tk in joints and ck in joints:
                out_lst.append(_seg_dist(joints[hk][f], joints[tk][f], joints[ck][f]))

        if all(k in joints for k in ("foot_MainPoint_l", "foot_MainPoint_r")):
            ankle_gaps.append(_dist(joints["foot_MainPoint_l"][f], joints["foot_MainPoint_r"][f]))

    def _stats(vals: list[float]) -> dict[str, float] | None:
        if not vals:
            return None
        return {
            "min": round(min(vals), 2),
            "max": round(max(vals), 2),
            "mean": round(sum(vals) / len(vals), 2),
        }

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
        gates_status = "OK" if proc.returncode == 0 else "FAIL"

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
        "torso_angles_deg": {
            "lean": _stats(lean_degs),
            "bank": _stats(bank_degs),
        },
        "clearance_cm": {
            "hand_l_thigh_l": _stats(clear_l),
            "hand_r_thigh_r": _stats(clear_r),
            "ankle_gap": _stats(ankle_gaps),
        },
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
            fv = float(v)
            if not (lo <= fv <= hi):
                raise ValueError(f"{k}={fv} out of safe bounds [{lo}, {hi}]")
            clean[k] = fv
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
    gate_win: int = 40,
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
    gates_ok = proc.returncode == 0
    result["gates"] = {"status": "OK" if gates_ok else "FAIL", "output": (proc.stdout or proc.stderr or "").strip()}
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
    variant_a: str,
    variant_b: str,
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
) -> dict[str, Any]:
    """Generate A/B videos (_real + _slow4x) and mandatory grids (az180, az90, game, grid_pivot)."""
    repo = _repo_root(root)
    f0, f1 = _parse_frame_range(window, 10000)
    pf0, pf1 = _parse_frame_range(pivot_window, 10000)

    if "az180" not in views.split(","):
        views = "az180," + views

    ue59 = Path(ue59_dir).resolve()
    rs_dir = ue59 / "Tools" / "AnimPipeline" / "cards" / "runstart"
    batch_root = ue59 / "Saved" / saved_dir / batch
    out_dir = batch_root / "ab_out"

    if run_gpu:
        batch_ps1 = rs_dir / "runstart_batch_captures.ps1"
        runs = [f"{variant_a}|1", f"{variant_b}|1"]
        gproc = subprocess.run(
            ["pwsh", "-NoProfile", "-File", str(batch_ps1), "-Runs", ",".join(runs), "-SavedDir", saved_dir, "-Batch", batch, "-Views", views],
            capture_output=True,
            text=True,
        )
        if gproc.returncode != 0:
            return {"ok": False, "stage": "gpu_capture", "stdout": gproc.stdout, "stderr": gproc.stderr}

    ab_py = repo / "Tools" / "AnimPipeline" / "cards" / "runstart" / "runstart_ab_video.py"
    cap_a = batch_root / variant_a
    cap_b = batch_root / variant_b
    if not (cap_a.exists() and cap_b.exists()):
        return {
            "ok": False,
            "error": "Capture directories not found (set run_gpu=true to capture).",
            "expected": [str(cap_a), str(cap_b)],
        }

    vproc = subprocess.run(
        [
            sys.executable, str(ab_py),
            str(cap_a), str(cap_b), str(out_dir),
            f"--a={variant_a}", f"--b={variant_b}",
            f"--seq={seq}", f"--from={f0}", f"--to={f1}",
            f"--views={views}",
            f"--pivot-from={pf0}", f"--pivot-to={pf1}", f"--pivot-step={pivot_step}",
        ],
        capture_output=True,
        text=True,
    )
    grids = sorted(str(p) for p in out_dir.glob("*.jpg"))
    videos = sorted(str(p) for p in out_dir.glob("*.mp4"))
    return {
        "ok": vproc.returncode == 0,
        "views": views.split(","),
        "grids": grids,
        "videos": videos,
        "output": (vproc.stdout or vproc.stderr or "").strip(),
    }


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
