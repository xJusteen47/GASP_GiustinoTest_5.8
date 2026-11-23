"""UE 5.9 headless engine for the follow_through intent (see follow_through.py for the math).

Run through Run-SparseAnimStep.ps1 like geom_intent_ue.py. Reads LOCAL bone rotations of the chain bones, solves the
follow-through in pure Python and writes only those tracks back. The sparse step copies just this one script into
_ue59, so the math module is imported from GSTN_MCP_DIR (set by anim_ops). Report goes to GSTN_FT_REPORT.
"""
from __future__ import annotations

import json
import os
import sys

try:
    import unreal
except ImportError:
    unreal = None

FOLDER = "/Game/GSTN/Animation/RunStart"


def _resolve_asset(name_or_path: str) -> str:
    return name_or_path if name_or_path.startswith("/Game/") else f"{FOLDER}/{name_or_path}"


def main() -> int:
    if unreal is None:
        print("FOLLOW_THROUGH ERROR: requires Unreal Engine environment")
        return 2
    mcp_dir = os.environ.get("GSTN_MCP_DIR", "")
    if mcp_dir and mcp_dir not in sys.path:
        sys.path.insert(0, mcp_dir)
    import follow_through as ftm

    intent_path = os.environ.get("GSTN_INTENT_JSON", "")
    if not intent_path or not os.path.exists(intent_path):
        print(f"FOLLOW_THROUGH ERROR: missing GSTN_INTENT_JSON={intent_path!r}")
        return 2
    payload = json.loads(open(intent_path, encoding="utf-8").read())
    report_path = os.environ.get("GSTN_FT_REPORT", "")
    save = os.environ.get("GSTN_SAVE", "0") == "1"
    src_path, dst_path = _resolve_asset(payload["src"]), _resolve_asset(payload["dst"])
    try:
        c = ftm.validate_follow_through(payload["intent"]["follow_through"])
    except ValueError as exc:
        print(f"FOLLOW_THROUGH ERROR: {exc}")
        return 2

    al, eal = unreal.AnimationLibrary, unreal.EditorAssetLibrary
    if not eal.does_asset_exist(src_path):
        print(f"FOLLOW_THROUGH ERROR: src not found {src_path}")
        return 2
    if save and src_path != dst_path:
        if eal.does_asset_exist(dst_path):
            eal.delete_asset(dst_path)
        if not eal.duplicate_asset(src_path, dst_path):
            print(f"FOLLOW_THROUGH ERROR: failed to duplicate {src_path} -> {dst_path}")
            return 2
    seq = unreal.load_asset(dst_path if save else src_path)
    n = int(seq.get_editor_property("number_of_sampled_frames")) if hasattr(seq, "number_of_sampled_frames") else al.get_num_frames(seq) + 1
    fps = 30.0
    if hasattr(seq, "get_sampling_frame_rate"):
        try:
            fr = seq.get_sampling_frame_rate()
            if fr and getattr(fr, "denominator", 1):
                fps = float(fr.numerator) / float(fr.denominator)
        except Exception:
            fps = 30.0
    opts = unreal.AnimPoseEvaluationOptions()
    if hasattr(opts, "should_retarget"):
        opts.should_retarget = False

    bones = sorted({b for ch in c["chains"] for links in ch["links"] for b in links})
    pos, rot_u, scl, rot = {b: [] for b in bones}, {b: [] for b in bones}, {b: [] for b in bones}, {b: [] for b in bones}
    for f in range(n):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / fps, opts)
        for b in bones:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL)
            q = t.rotation
            pos[b].append(t.translation)
            scl[b].append(t.scale3d)
            rot[b].append((q.x, q.y, q.z, q.w))
    try:
        new, rep = ftm.solve_chain(rot, c, n)
    except ValueError as exc:
        print(f"FOLLOW_THROUGH ERROR: {exc}")
        return 2
    cons = ftm.conservation_report(rot, new, bones, rep["window"][0], rep["window"][1])
    rep["conservation"] = cons
    rep["ok"] = bool(cons["ok"])
    rep["src"], rep["dst"] = payload["src"], payload["dst"]

    if save:
        ctrl = seq.controller if hasattr(seq, "controller") else seq.get_editor_property("controller")
        ctrl.open_bracket(unreal.Text("gstn_follow_through"), False)
        try:
            for b in bones:
                rots = [unreal.Quat(q[0], q[1], q[2], q[3]) for q in new[b]]
                if ctrl.set_bone_track_keys(b, pos[b], rots, scl[b], False) is False:
                    raise RuntimeError(f"set_bone_track_keys returned False for {b}")
        finally:
            ctrl.close_bracket(False)
        eal.save_asset(dst_path, only_if_is_dirty=False)
    if report_path:
        with open(report_path, "w", encoding="utf-8") as fh:
            json.dump(rep, fh, indent=2)
    peak = max((l["peak_deg"] for l in rep["links"]), default=0.0)
    print(f"FOLLOW_THROUGH: {'OK' if rep['ok'] else 'FAIL'} -> {dst_path if save else src_path} (save={save}, window={rep['window']}, peak_deg={peak})")
    return 0 if rep["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
