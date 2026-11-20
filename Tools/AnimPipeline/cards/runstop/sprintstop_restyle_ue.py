"""Restyled GASP sprint stops -> game clips (headless UE 5.9, editor closed on this checkout).

Source of the restyle: Cascadeur scenes Saved/AnimPipeline/runstop/SprintStop_{L,R}_v2.casc (restyle_v2.py: body only, feet pinned).
Both the untouched round trip (v0.fbx) and the restyle (v2.fbx) are exported from Cascadeur; this script transfers only the
DIFFERENCE v2 - v0 onto a duplicate of the GASP clip, so the Cascadeur round-trip error cancels and the duplicate keeps the
GASP root track, curves (contact_l/r, movedata_speed), notifies (Foley, PoseSearch*) and root-motion settings unchanged.
  rotation:     src * inv(v0) * v2 (local, per bone, every frame)
  translation:  pelvis only, src + (v2 - v0); every other bone keeps the GASP translation (bone lengths exact)
  untouched:    root, ik_* (feet are pinned in Cascadeur, so ik_foot still matches the feet)
Run: UnrealEditor-Cmd <uproject> -run=pythonscript -script="<copy of this file>" -unattended -nullrhi
Writes Saved/AnimPipeline/runstop/ue_build.json; prints max 5 lines.
"""
import json, math, os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
WORK = os.path.join(PROJ, "Saved/AnimPipeline/runstop")
DST_DIR = "/Game/GSTN/Locomotion/StockBase/RunStop"
SIDES = {
    "L": "/Game/Characters/UEFN_Mannequin/Animations/Sprint/M_Relaxed_Sprint_Stop_F_Lfoot",
    "R": "/Game/Characters/UEFN_Mannequin/Animations/Sprint/M_Relaxed_Sprint_Stop_F_Rfoot",
}
VER = os.environ.get("RS_VER", "v2")
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary


def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(a):
    return (-a[0], -a[1], -a[2], a[3])


def qang(a, b):
    d = abs(sum(x * y for x, y in zip(a, b)))
    return math.degrees(2 * math.acos(min(1.0, d)))


def tup(q):
    return (q.x, q.y, q.z, q.w)


def import_fbx(path, name, skel):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = DST_DIR + "/_Src"
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = False
    ui = unreal.FbxImportUI()
    ui.import_mesh = False
    ui.import_animations = True
    ui.import_materials = False
    ui.import_textures = False
    ui.skeleton = skel
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    ad = ui.anim_sequence_import_data
    for k, v in (("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME),
                 ("remove_redundant_keys", False), ("use_default_sample_rate", False), ("custom_sample_rate", 30),
                 ("import_custom_attribute", False)):
        try:
            ad.set_editor_property(k, v)
        except Exception as e:
            unreal.log_warning(f"SPRINTSTOP import option {k}: {e}")
    t.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return unreal.load_asset(DST_DIR + "/_Src/" + name)


def tracks(anim, n):
    out = {}
    for b in [str(x) for x in AL.get_animation_track_names(anim)]:
        out[b] = [AL.get_bone_pose_for_frame(anim, b, f, False) for f in range(n)]
    return out


def build(side, src_path, rep):
    src = unreal.load_asset(src_path)
    skel = src.get_editor_property("skeleton")
    n = src.get_editor_property("number_of_sampled_keys")
    a = import_fbx(os.path.join(WORK, f"SprintStop_{side}_v0.fbx"), f"Casc_SprintStop_{side}_v0", skel)
    b = import_fbx(os.path.join(WORK, f"SprintStop_{side}_{VER}.fbx"), f"Casc_SprintStop_{side}_{VER}", skel)
    na, nb = a.get_editor_property("number_of_sampled_keys"), b.get_editor_property("number_of_sampled_keys")
    r = rep.setdefault(side, {"frames": [n, na, nb]})
    if not (n == na == nb):
        r["error"] = "frame count mismatch"
        return
    dst_path = f"{DST_DIR}/AS_GSTN_SprintStop_Ganon_{side}"
    if not EAL.does_asset_exist(dst_path):
        EAL.duplicate_asset(src_path, dst_path)
    dst = unreal.load_asset(dst_path)
    S, A, B = tracks(src, n), tracks(a, n), tracks(b, n)
    rt_rot, rt_pel, delta_max, wrote = 0.0, 0.0, {}, 0
    ctrl = dst.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("sprint stop restyle"), False)
    for bone, st in S.items():
        if bone == "root" or bone.startswith("ik_") or bone not in A or bone not in B:
            continue
        pos, rot, scl = [], [], []
        for f in range(n):
            qs, qa, qb = tup(st[f].rotation), tup(A[bone][f].rotation), tup(B[bone][f].rotation)
            rt_rot = max(rt_rot, qang(qs, qa))
            qd = qmul(qinv(qa), qb)
            qn = qmul(qs, qd)
            delta_max[bone] = max(delta_max.get(bone, 0.0), qang(qs, qn))
            t = st[f].translation
            if bone == "pelvis":
                rt_pel = max(rt_pel, (A[bone][f].translation - t).length())
                t = t + (B[bone][f].translation - A[bone][f].translation)
            pos.append(t); rot.append(unreal.Quat(*qn)); scl.append(unreal.Vector(1.0, 1.0, 1.0))
        ctrl.set_bone_track_keys(bone, pos, rot, scl, False)
        wrote += 1
    ctrl.close_bracket(False)
    D = tracks(dst, n)
    root_diff = max((D["root"][f].translation - S["root"][f].translation).length() for f in range(n))
    pel_dz = [round((D["pelvis"][f].translation - S["pelvis"][f].translation).length(), 2) for f in range(0, n, 3)]
    r.update({"asset": dst_path, "bones_written": wrote, "roundtrip_rot_max_deg": round(rt_rot, 2),
              "roundtrip_pelvis_cm": round(rt_pel, 2), "root_diff_cm": round(root_diff, 4),
              "pelvis_delta_every3": pel_dz,
              "top_delta_deg": sorted(((round(v, 1), k) for k, v in delta_max.items()), reverse=True)[:8],
              "curves": [str(c) for c in AL.get_animation_curve_names(dst, unreal.RawCurveTrackTypes.RCT_FLOAT)],
              "notifies": len(AL.get_animation_notify_events(dst))})
    EAL.save_asset(dst_path)


def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game"], True)
    rep = {}
    for side, p in SIDES.items():
        try:
            build(side, p, rep)
        except Exception as e:
            rep.setdefault(side, {})["error"] = repr(e)
    json.dump(rep, open(os.path.join(WORK, "ue_build.json"), "w"), indent=1)
    for side, r in rep.items():
        print("SPRINTSTOP", side, r.get("error", "ok"), r.get("roundtrip_rot_max_deg"), r.get("root_diff_cm"))


main()
