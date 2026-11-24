"""UE 5.9 headless side of `anim_layer` (math in anim_layer.py). Run through Run-SparseAnimStep.ps1 like follow_through_ue.py.

Two jobs, chosen by environment (set by anim_layer_ops):
- DUMP  (GSTN_LAYER_DUMP=<json path>): write the LOCAL transforms of the FK bones of the src clip, one row per frame.
  Done once per clip; afterwards every layer is planned and measured offline at 0 editor minutes.
- BUILD (GSTN_INTENT_JSON=<payload>, GSTN_SAVE=1): dst = src + the whole layer stack. Reads local transforms, checks that FK
  over anim_layer.PARENTS gives the engine's own component-space positions (a wrong hierarchy row is an error, not a bad
  edit), applies the stack, verifies conservation on the data and writes back ONLY the rotation tracks of the edited bones
  (translations and scales are written back unchanged; one value per frame, no keys added). Report: GSTN_LAYER_REPORT.
The sparse step copies just this script into _ue59, so the math module is imported from GSTN_MCP_DIR.
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
FK_TOL_CM = 0.5


def _resolve_asset(name_or_path: str) -> str:
    return name_or_path if name_or_path.startswith("/Game/") else f"{FOLDER}/{name_or_path}"


def _fail(msg: str) -> int:
    print(f"ANIM_LAYER ERROR: {msg}")
    return 2


def _read(seq, bones):
    al = unreal.AnimationLibrary
    n = int(seq.get_editor_property("number_of_sampled_frames")) if hasattr(seq, "number_of_sampled_frames") else al.get_num_frames(seq) + 1
    fps = 30.0
    try:
        fr = seq.get_sampling_frame_rate()
        if fr and getattr(fr, "denominator", 1):
            fps = float(fr.numerator) / float(fr.denominator)
    except Exception:
        fps = 30.0
    opts = unreal.AnimPoseEvaluationOptions()
    if hasattr(opts, "should_retarget"):
        opts.should_retarget = False
    present = None
    local, scale, comp = {}, {}, {}
    for f in range(n):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / fps, opts)
        if present is None:
            names = {str(b) for b in unreal.AnimPoseExtensions.get_bone_names(pose)}
            present = [b for b in bones if b in names]
            for b in present:
                local[b], scale[b], comp[b] = [], [], []
        for b in present:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL)
            q, p, s = t.rotation, t.translation, t.scale3d
            local[b].append([p.x, p.y, p.z, q.x, q.y, q.z, q.w])
            scale[b].append((s.x, s.y, s.z))
            w = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
            comp[b].append((w.x, w.y, w.z))
    return n, local, scale, comp


def main() -> int:
    if unreal is None:
        return _fail("requires Unreal Engine environment")
    mcp_dir = os.environ.get("GSTN_MCP_DIR", "")
    if mcp_dir and mcp_dir not in sys.path:
        sys.path.insert(0, mcp_dir)
    import anim_layer as L

    eal = unreal.EditorAssetLibrary
    dump_path = os.environ.get("GSTN_LAYER_DUMP", "")
    intent_path = os.environ.get("GSTN_INTENT_JSON", "")
    if dump_path:
        src_path = _resolve_asset(os.environ.get("GSTN_LAYER_SRC", ""))
        if not eal.does_asset_exist(src_path):
            return _fail(f"src not found {src_path}")
        n, local, _, comp = _read(unreal.load_asset(src_path), L.FK_BONES)
        with open(dump_path, "w", encoding="utf-8") as fh:
            json.dump({"src": src_path, "n": n, "local": local, "component_pos": comp}, fh)
        print(f"ANIM_LAYER: DUMP OK -> {dump_path} (frames={n}, bones={len(local)})")
        return 0

    if not intent_path or not os.path.exists(intent_path):
        return _fail(f"missing GSTN_INTENT_JSON={intent_path!r}")
    payload = json.loads(open(intent_path, encoding="utf-8").read())
    report_path = os.environ.get("GSTN_LAYER_REPORT", "")
    save = os.environ.get("GSTN_SAVE", "0") == "1"
    src_path, dst_path = _resolve_asset(payload["src"]), _resolve_asset(payload["dst"])
    if src_path == dst_path:
        return _fail("dst must be a new variant, never the src clip")
    if not eal.does_asset_exist(src_path):
        return _fail(f"src not found {src_path}")
    if save:
        if eal.does_asset_exist(dst_path):
            eal.delete_asset(dst_path)
        if not eal.duplicate_asset(src_path, dst_path):
            return _fail(f"failed to duplicate {src_path} -> {dst_path}")
    seq = unreal.load_asset(dst_path if save else src_path)
    n, rows, scale, comp = _read(seq, L.FK_BONES)
    try:
        layers = [L.validate_layer(raw, n) for raw in payload["layers"]]
        _, local = L.clip_from_dump({"n": n, "local": rows})
        parents = L.resolve_parents(list(local))
        worst, worst_b = 0.0, ""
        for f in range(0, n, max(1, n // 8)):
            w = L.fk({b: local[b][f] for b in local}, parents)
            for b in parents:
                d = L.vlen(L.vsub(w[b][0], comp[b][f]))
                if d > worst:
                    worst, worst_b = d, b
        if worst > FK_TOL_CM:
            return _fail(f"FK over anim_layer.PARENTS differs from the engine pose by {worst:.2f} cm at {worst_b}: fix PARENTS")
        new, rep = L.apply_stack(n, local, layers)
        cons = L.conservation(n, local, new, layers)
    except ValueError as exc:
        return _fail(str(exc))
    rep.update({"conservation": cons, "ok": bool(cons["ok"]), "src": payload["src"], "dst": payload["dst"], "n_frames": n,
                "fk_check_max_cm": round(worst, 4)})

    if save and rep["ok"]:
        ctrl = seq.controller if hasattr(seq, "controller") else seq.get_editor_property("controller")
        tracks = {str(t) for t in unreal.AnimationLibrary.get_animation_track_names(seq)}
        ctrl.open_bracket(unreal.Text("gstn_anim_layer"), False)
        try:
            for b, qs in new.items():
                if b not in tracks:
                    ctrl.add_bone_curve(b, False)
                pos = [unreal.Vector(r[0], r[1], r[2]) for r in rows[b]]
                rot = [unreal.Quat(q[0], q[1], q[2], q[3]) for q in qs]
                scl = [unreal.Vector(s[0], s[1], s[2]) for s in scale[b]]
                if ctrl.set_bone_track_keys(b, pos, rot, scl, False) is False:
                    raise RuntimeError(f"set_bone_track_keys returned False for {b}")
        finally:
            ctrl.close_bracket(False)
        eal.save_asset(dst_path, only_if_is_dirty=False)
    if report_path:
        with open(report_path, "w", encoding="utf-8") as fh:
            json.dump(rep, fh, indent=2)
    print(f"ANIM_LAYER: {'OK' if rep['ok'] else 'FAIL'} -> {dst_path if save else src_path} (save={save}, layers={len(layers)}, "
          f"bones={','.join(rep['edited_bones'])})")
    return 0 if rep["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
