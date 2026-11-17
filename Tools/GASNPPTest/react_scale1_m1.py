"""M1 step 3 (diagnostic): copy AS_React_GroundHit to /Game/AnimPipeline/Diag/AS_React_GroundHit_scale1 with every bone scale
set to 1 (the base clip bakes non-unit scale on head, thigh_l/r, foot_l/r). The original is untouched. Run inside the editor."""
import unreal, os, json
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "react_scale1.txt")
L = []
def flush():
    open(OUT, "w").write("\n".join(L) + "\nDONE_MARK")
try:
    SRC = "/Game/GASNPPTest/Combat/Reaction/AS_React_GroundHit"
    DST = "/Game/AnimPipeline/Diag/AS_React_GroundHit_scale1"
    if unreal.EditorAssetLibrary.does_asset_exist(DST):
        unreal.EditorAssetLibrary.delete_asset(DST)
    dst = unreal.EditorAssetLibrary.duplicate_asset(SRC, DST)
    ctrl = dst.get_editor_property("controller")
    n = dst.get_editor_property("number_of_sampled_keys")
    names = [str(b) for b in unreal.AnimationLibrary.get_animation_track_names(dst)]
    fixed = []
    ctrl.open_bracket(unreal.Text("scale1"), False)
    for b in names:
        tfs = [unreal.AnimationLibrary.get_bone_pose_for_frame(dst, b, f, False) for f in range(n)]
        if any(abs(t.scale3d.x - 1) > 1e-3 or abs(t.scale3d.y - 1) > 1e-3 or abs(t.scale3d.z - 1) > 1e-3 for t in tfs):
            one = unreal.Vector(1, 1, 1)
            ctrl.set_bone_track_keys(b, [t.translation for t in tfs], [t.rotation for t in tfs], [one] * n, False)
            fixed.append(b)
    ctrl.close_bracket(False)
    unreal.EditorAssetLibrary.save_asset(DST)
    L.append("tracks=%d frames=%d fixed=%s" % (len(names), n, fixed))
except Exception:
    import traceback
    L.append(traceback.format_exc())
flush()
