import unreal, os
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "react_bones.txt")
L = []
def flush():
    open(OUT, "w").write("\n".join(L) + "\nDONE_MARK")
try:
    L.append("AnimPose names: " + ",".join(n for n in dir(unreal) if n.startswith("AnimPose")))
    L.append("AnimPoseExtensions: " + ",".join(n for n in dir(unreal.AnimPoseExtensions) if not n.startswith("_")))
    flush()
    sk = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin")
    refp = sk.get_reference_pose()
    bones = [str(b) for b in unreal.AnimPoseExtensions.get_bone_names(refp)]
    L.append("n_bones=%d" % len(bones))
    opts = unreal.AnimPoseEvaluationOptions()
    opts.should_retarget = False
    L.append("opts props: " + ",".join(n for n in dir(opts) if not n.startswith("_") and not callable(getattr(opts, n))))
    flush()
    for b in ("root","pelvis","head","thigh_l","foot_l","thigh_r","foot_r","upperarm_l","hand_l","calf_l","ik_foot_l"):
        r = unreal.AnimPoseExtensions.get_bone_pose(refp, b, unreal.AnimPoseSpaces.LOCAL)
        L.append("REF %s t=(%.2f,%.2f,%.2f) s=(%.3f,%.3f,%.3f)" % (b, r.translation.x, r.translation.y, r.translation.z, r.scale3d.x, r.scale3d.y, r.scale3d.z))
    flush()
    for n in ["AS_React_Tumble"]:
        a = unreal.load_asset("/Game/GASNPPTest/Combat/Reaction/" + n)
        for t in (0.0,):
            ap = unreal.AnimPoseExtensions.get_anim_pose_at_time(a, t, opts)
            out = []
            for b in bones:
                s = unreal.AnimPoseExtensions.get_bone_pose(ap, b, unreal.AnimPoseSpaces.LOCAL)
                r = unreal.AnimPoseExtensions.get_bone_pose(refp, b, unreal.AnimPoseSpaces.LOCAL)
                la = (s.translation.x ** 2 + s.translation.y ** 2 + s.translation.z ** 2) ** 0.5
                lr = (r.translation.x ** 2 + r.translation.y ** 2 + r.translation.z ** 2) ** 0.5
                sc = s.scale3d
                if abs(la - lr) > 0.3 or max(abs(sc.x - r.scale3d.x), abs(sc.y - r.scale3d.y), abs(sc.z - r.scale3d.z)) > 0.01:
                    out.append("%s anim=%.2f ref=%.2f scale=(%.2f,%.2f,%.2f)" % (b, la, lr, sc.x, sc.y, sc.z))
            L.append("== %s t=%.2f: %d of %d bones differ" % (n, t, len(out), len(bones)))
            L.extend("   " + o for o in out[:25])
            flush()
except Exception:
    import traceback
    L.append(traceback.format_exc())
flush()
