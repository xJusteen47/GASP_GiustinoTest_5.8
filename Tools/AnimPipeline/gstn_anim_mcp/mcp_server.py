"""Stdio MCP JSON-RPC 2.0 server for gstn-anim-mcp (pure Python 3.14 stdlib).

Exposes 8 semantic animation tools so flash models never have to write 300+ lines of
quaternions/2-bone IK Python:
  - anim_inspect(asset, frame_range)
  - anim_apply_intent(src, dst, intent_json)
  - anim_gate_check(asset, ref_asset)
  - anim_capture_ab(variant_a, variant_b, seq, window)
  - anim_register_variant(asset, variant_id) / anim_unregister_variant(variant_id)
  - anim_run_intent(src, dst, intent_json): snapshot + intent + gates in one call
  - anim_rollback(asset)

Replies are the full JSON (no indentation) by default; compact mode (<= 5 lines, details in a file, see compact.py) is\nopt-in: env GSTN_MCP_COMPACT=1 or a `compact: true` argument on the call. The tool list is a FIXED-order, constant\ntext prefix so provider prompt caches (DeepSeek prefix cache) keep hitting; never put variable data in it.

Never writes non-JSON-RPC output to stdout; logs only to stderr.
"""
from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from typing import Any

_HERE = str(Path(__file__).resolve().parent)
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import anim_ops  # noqa: E402
import compact  # noqa: E402

SERVER_INFO = {"name": "gstn-anim-mcp", "version": "1.0.0"}
DEFAULT_PROTOCOL_VERSION = "2025-03-26"


def _tool_defs() -> list[dict[str, Any]]:
    return [
        {
            "name": "anim_inspect",
            "description": (
                "Inspect an animation asset's dumped joints and return readable metrics: "
                "hip/foot/elbow/shoulder heights (cm), torso lean/bank angles (deg), "
                "lean_local_deg, upperarm_abduction_deg, elbow_flex_deg, jitter_max, splits, "
                "hand-thigh/ankle clearance (cm), and runstart_pivot_gates status."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "asset": {"type": "string", "description": "AnimSequence name or /Game/... path."},
                    "frame_range": {"description": "Optional range like '84-116' or [84, 116]."},
                    "gate_win": {"type": "integer", "default": 18},
                },
                "required": ["asset"],
            },
        },
        {
            "name": "anim_apply_intent",
            "description": (
                "Apply semantic JSON intent parameters via the unified UE 5.9 geometric engine, "
                "automatically taking a rollback snapshot first. v2 character-anchored parameters: "
                "turn_side, turn_deg, start_from_frame, pelvis_fwd_cm, pelvis_drop_cm, "
                "spine_lean_pivot_deg, spine_lean_run_deg, torso_bank_deg, head_lift_deg, "
                "knee_drop, swing_lag_cm, catch_fwd_cm, step_narrow_cm, lat_widen_cm, "
                "pivot_heel_deg, clav_st_elev_deg, clav_pv_elev_deg, clav_open_deg, "
                "arm_st_tuck_in_deg, arm_pv_out_deg, arm_st_back_deg, arm_st_elbow_deg, "
                "arm_pv_fwd_deg, arm_pv_elbow_deg, arm_clr_deg, smooth_pelvis_z, "
                "smooth_upper_passes, smooth_leg_passes. v1 backward-compatible keys: "
                "com_shift_fwd_cm, torso_lean_pivot_deg, torso_bank_deg, shoulder_level_compensate, "
                "arm_back_arc, foot_pushoff_deg, w3_knee_drop, handoff_smooth_range, frame_range. "
                "ALL FRAMES ARE CLIP-LOCAL (0..n-1 of the SRC clip, e.g. w3 has 62 frames; NOT the capture v-frame). "
                "frame_range=[a,b] must lie inside the clip or the call is rejected. With frame_range the default "
                "envelope is 'range': every parameter ramps in over <=3 frames from a, holds its full value in the "
                "middle, ramps out <=3 frames before b, so the degrees you ask for are reached inside [a,b] "
                "(use a range of >= 6 frames). Without frame_range the envelope is 'turnaround': FIXED timings on the "
                "turnaround pivot, in clip frames (turn_side L/R; with turn_side none the rise starts 2 frames earlier; ×0.8 on pivot times when turn_deg is 45-135) (rise-fall, smoothstep): w3/pelvis_fwd/pelvis_drop 1-4 up, 22-40 down; "
                "spine_lean_pivot 1-3.8 up, 7-13 down; spine_lean_run 6-11 up, 22-42 down; torso_bank 1-4 up, 10-22 down; "
                "head_lift_deg 3.5-7.5 up, 16-25 down; clav_* (shoulders) 1-3.8 up, 20-31 down; arm_st_back 1-3.8 up, "
                "6-9.5 down; arm_pv_fwd 1-3.8 up, 7.5-13 down; arm_clr_deg 6.5-9.5 up, 12-16 down; swing_lag 1.5-4.5 up, "
                "8-12 down; catch_fwd 6-10.5 up, 18-25 down; pivot_heel 1-2.8 up, 5.5-9.5 down; lat_widen 1-4 up, 11-16 down. "
                "Pass envelope='range' or 'turnaround' to force one. Unit: degrees (cm for *_cm). Intent example: "
                "{\"arm_clr_deg\": 12, \"frame_range\": [8, 20]} = elbows opened by ~12 deg between clip frames 8 and 20. "
                "Always follow with anim_gate_check, then anim_inspect (upperarm_abduction_deg / head_neck)."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "src": {"type": "string", "description": "Source AnimSequence name or path."},
                    "dst": {"type": "string", "description": "Destination AnimSequence name or path."},
                    "intent_json": {"type": "object", "description": "Semantic intent parameters."},
                    "dry_run": {"type": "boolean", "default": False, "description": "No side effects: returns the plan (bones, active/peak frames, degrees actually reached) without snapshot or edit."},
                },
                "required": ["src", "dst", "intent_json"],
            },
        },
        {
            "name": "anim_run_intent",
            "description": (
                "One call = snapshot + anim_apply_intent + anim_gate_check (conservation vs src). Same intent_json rules "
                "as anim_apply_intent (clip-local frames, envelopes). Returns PASS/FAIL; on FAIL the next step is "
                "anim_rollback. dry_run=true returns only the plan, like anim_apply_intent."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "src": {"type": "string", "description": "Source AnimSequence name or path."},
                    "dst": {"type": "string", "description": "Destination AnimSequence name or path."},
                    "intent_json": {"type": "object", "description": "Semantic intent parameters."},
                    "gate_win": {"type": "integer", "default": 18},
                    "dry_run": {"type": "boolean", "default": False},
                },
                "required": ["src", "dst", "intent_json"],
            },
        },
        {
            "name": "anim_gate_check",
            "description": (
                "Run deterministic 0-token hard gates: runstart_pivot_gates + Conservation Guard "
                "(<= 15% motion loss vs ref_asset or FBX) + optional clipcheck."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "asset": {"type": "string", "description": "Candidate AnimSequence name."},
                    "ref_asset": {"type": "string", "default": "", "description": "Baseline AnimSequence name."},
                    "gate_win": {"type": "integer", "default": 18},
                    "before_fbx": {"type": "string", "default": ""},
                    "after_fbx": {"type": "string", "default": ""},
                },
                "required": ["asset"],
            },
        },
        {
            "name": "anim_capture_ab",
            "description": (
                "Generate A/B GPU Play captures, _real.mp4 + _slow4x.mp4 videos, and mandatory "
                ".jpg grids (az180, az90, game, grid_pivot_v84_116.jpg) at 0 LLM tokens."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "pairs": {"type": "array", "items": {"type": "object"}},
                    "variant_a": {"type": "string"},
                    "variant_b": {"type": "string"},
                    "variant_a_id": {"type": "string"},
                    "variant_b_id": {"type": "string"},
                    "clip_a": {"type": "string"},
                    "clip_b": {"type": "string"},
                    "seq": {"type": "string", "default": "mv_run_start_v5"},
                    "window": {"type": "string", "default": "77-197"},
                    "views": {"type": "string", "default": "az180,az90,game"},
                    "assert_win": {"type": "string"},
                    "cvars_a": {"type": "string"},
                    "cvars_b": {"type": "string"},
                    "pivot_window": {"type": "string"},
                    "pivot_step": {"type": "integer"},
                    "saved_dir": {"type": "string", "default": "CaptureSpeed"},
                    "batch": {"type": "string", "default": "batch"},
                    "run_gpu": {"type": "boolean", "default": False},
                },
            },
        },
        {
            "name": "anim_register_variant",
            "description": (
                "Register an edited run-start clip as chooser variant N (and N+10, the Handoff sibling) in the capture "
                "project _ue59 so anim_capture_ab can play it in game (variant_b_id=N). Needs the _ue59 lock and no open "
                "editor. The tracked chooser CHT_GSTN_StockMover_PoseMatch is backed up byte-for-byte and the clip copied "
                "into _ue59 only for the capture: ALWAYS call anim_unregister_variant afterwards (restores the chooser, "
                "sha256 verified, removes the copy). Refuses when the chooser already has owner changes unless "
                "allow_dirty_chooser=true (only with the owner's OK). variant_id: 30..179 (N+10 is the Handoff sibling: keep registered ids 20 apart)."
            ),
            "inputSchema": {
                "type": "object",
                "properties": {
                    "asset": {"type": "string", "description": "AnimSequence name already produced by anim_apply_intent."},
                    "variant_id": {"type": "integer", "default": 60},
                    "allow_dirty_chooser": {"type": "boolean", "default": False},
                    "dry_run": {"type": "boolean", "default": False},
                },
                "required": ["asset"],
            },
        },
        {
            "name": "anim_unregister_variant",
            "description": "Undo anim_register_variant: restore the chooser bytes (sha256 checked) and delete the clip copy from _ue59.",
            "inputSchema": {
                "type": "object",
                "properties": {"variant_id": {"type": "integer", "default": 60}},
            },
        },
        {
            "name": "anim_rollback",
            "description": "Restore the latest pre-intent snapshot of an asset when gates or review fail.",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "asset": {"type": "string", "description": "AnimSequence name to restore."},
                },
                "required": ["asset"],
            },
        },
    ]


def handle_tool_call(name: str, args: dict[str, Any]) -> dict[str, Any]:
    if name == "anim_inspect":
        return anim_ops.anim_inspect(
            asset=args["asset"],
            frame_range=args.get("frame_range"),
            gate_win=int(args.get("gate_win", 18)),
        )
    if name == "anim_apply_intent":
        return anim_ops.anim_apply_intent(
            src=args["src"],
            dst=args["dst"],
            intent_json=args["intent_json"],
            dry_run=bool(args.get("dry_run", False)),
        )
    if name == "anim_run_intent":
        return anim_ops.anim_run_intent(
            src=args["src"],
            dst=args["dst"],
            intent_json=args["intent_json"],
            gate_win=int(args.get("gate_win", 18)),
            dry_run=bool(args.get("dry_run", False)),
        )
    if name == "anim_gate_check":
        return anim_ops.anim_gate_check(
            asset=args["asset"],
            ref_asset=args.get("ref_asset", ""),
            gate_win=int(args.get("gate_win", 18)),
            before_fbx=args.get("before_fbx", ""),
            after_fbx=args.get("after_fbx", ""),
        )
    if name == "anim_capture_ab":
        return anim_ops.anim_capture_ab(
            pairs=args.get("pairs"),
            variant_a=args.get("variant_a"),
            variant_b=args.get("variant_b"),
            variant_a_id=args.get("variant_a_id") or "",
            variant_b_id=args.get("variant_b_id") or "",
            clip_a=args.get("clip_a") or "",
            clip_b=args.get("clip_b") or "",
            seq=args.get("seq") or "mv_run_start_v5",
            window=args.get("window") or "77-197",
            views=args.get("views") or anim_ops.DEFAULT_VIEWS,
            assert_win=args.get("assert_win") or "85,120",
            cvars_a=args.get("cvars_a") or "",
            cvars_b=args.get("cvars_b") or "",
            pivot_window=args.get("pivot_window") or "84-116",
            pivot_step=int(args.get("pivot_step") or 4),
            saved_dir=args.get("saved_dir") or "CaptureSpeed",
            batch=args.get("batch") or "batch",
            run_gpu=bool(args.get("run_gpu", False)),
        )
    if name == "anim_register_variant":
        return anim_ops.anim_register_variant(
            asset=args["asset"],
            variant_id=int(args.get("variant_id", 60)),
            allow_dirty_chooser=bool(args.get("allow_dirty_chooser", False)),
            dry_run=bool(args.get("dry_run", False)),
        )
    if name == "anim_unregister_variant":
        return anim_ops.anim_unregister_variant(variant_id=int(args.get("variant_id", 60)))
    if name == "anim_rollback":
        return anim_ops.anim_rollback(asset=args["asset"])
    raise KeyError(f"Unknown tool: {name}")


def _out_dir() -> Path | None:
    """Where full tool results go (details path is returned in the reply)."""
    try:
        return anim_ops._repo_root(None) / "Saved" / "AnimPipeline" / "mcp_out"
    except Exception:  # noqa: BLE001
        return None


def _render(tool: str, res: dict[str, Any], use_compact: bool) -> str:
    if use_compact:
        return compact.compact_reply(tool, res, _out_dir())
    return json.dumps(res, ensure_ascii=False)


def _write_msg(msg: dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(msg, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def process_request(req: dict[str, Any]) -> dict[str, Any] | None:
    req_id = req.get("id")
    method = req.get("method", "")
    params = req.get("params") or {}

    if method == "initialize":
        proto = params.get("protocolVersion") or DEFAULT_PROTOCOL_VERSION
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": proto,
                "capabilities": {"tools": {}},
                "serverInfo": SERVER_INFO,
            },
        }
    if method.startswith("notifications/"):
        return None
    if method == "ping":
        return {"jsonrpc": "2.0", "id": req_id, "result": {}}
    if method == "tools/list":
        return {"jsonrpc": "2.0", "id": req_id, "result": {"tools": _tool_defs()}}
    if method == "tools/call":
        tname = params.get("name", "")
        targs = dict(params.get("arguments") or {})
        use_compact = bool(targs.pop("compact", False)) or os.environ.get("GSTN_MCP_COMPACT", "") == "1"
        try:
            res = handle_tool_call(tname, targs)
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [{"type": "text", "text": _render(tname, res, use_compact)}],
                    "isError": not bool(res.get("ok", True)),
                },
            }
        except Exception as exc:  # noqa: BLE001
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [{"type": "text", "text": json.dumps({"ok": False, "error": str(exc)})}],
                    "isError": True,
                },
            }
    if req_id is None:
        return None
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {"code": -32601, "message": f"Method not found: {method}"},
    }


def main() -> int:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError as exc:
            _write_msg({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": str(exc)}})
            continue
        resp = process_request(req)
        if resp is not None:
            _write_msg(resp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
