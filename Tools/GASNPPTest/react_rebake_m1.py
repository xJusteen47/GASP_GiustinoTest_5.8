"""M1 reaction re-bake: force scale3d = 1 on every bone track of the 8 base AS_React_* clips (animpipe/ue_rig.py::_unit_scale, the same
function the fixed _lock_lengths now calls), save them, then verify every scale key of every track and frame. Run inside the editor.
Writes Saved/Logs/ci/react_rebake.txt (ends with DONE_MARK). Translation and rotation keys are rewritten with their own values."""
import os
import sys
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "react_rebake.txt")
L = []


def flush():
    open(OUT, "w").write("\n".join(L) + "\nDONE_MARK")


try:
    tools = os.path.join(unreal.Paths.project_dir(), "Tools", "AnimPipeline")
    sys.path.insert(0, tools)
    from animpipe import ue_rig
    names = ["AS_React_HitStand", "AS_React_Tumble", "AS_React_Splat", "AS_React_GroundHit", "AS_React_LieUp",
             "AS_React_GetUp", "AS_React_Roll", "AS_React_RollBack"]
    for n in names:
        path = "/Game/GASNPPTest/Combat/Reaction/" + n
        a = unreal.load_asset(path)
        before_len = a.get_editor_property("sequence_length")
        p0 = unreal.AnimationLibrary.get_bone_pose_for_frame(a, "pelvis", 0, False).translation
        fixed = ue_rig._unit_scale(a)
        unreal.EditorAssetLibrary.save_asset(path)
        a = unreal.load_asset(path)
        nfr = a.get_editor_property("number_of_sampled_keys")
        worst, wb = 0.0, ""
        tracks = [str(x) for x in unreal.AnimationLibrary.get_animation_track_names(a)]
        for b in tracks:
            for f in range(nfr):
                sc = unreal.AnimationLibrary.get_bone_pose_for_frame(a, b, f, False).scale3d
                d = max(abs(sc.x - 1), abs(sc.y - 1), abs(sc.z - 1))
                if d > worst:
                    worst, wb = d, "%s@%d" % (b, f)
        p1 = unreal.AnimationLibrary.get_bone_pose_for_frame(a, "pelvis", 0, False).translation
        dp = ((p1.x - p0.x) ** 2 + (p1.y - p0.y) ** 2 + (p1.z - p0.z) ** 2) ** 0.5
        L.append("%s tracks=%d frames=%d fixed=%s max|scale-1|=%.5f at %s len %.4f->%.4f pelvis_f0_shift=%.4f cm" % (
            n, len(tracks), nfr, ",".join(fixed), worst, wb or "-", before_len, a.get_editor_property("sequence_length"), dp))
        flush()
except Exception:
    import traceback
    L.append(traceback.format_exc())
flush()
