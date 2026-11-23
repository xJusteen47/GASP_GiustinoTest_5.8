# Mixamo clip -> UEFN mannequin (skid-walkturn-1, 2026-11-20). Same chain the owner used for the skid in 5.7:
#  Mixamo FBX on its own X Bot skeleton -> auto-generated IK retarget to SKM_UEFN_Mannequin -> owner's RTG_RootCorrection
#  (UEFN -> UEFN, root motion generated from the target pelvis) -> enable root motion.
# Env: MIXAMO_FBX (clip fbx), MIXAMO_SKIN (X Bot.fbx), MIXAMO_NAME (final asset name, default AS_GSTN_Mixamo_Clip).
# Dry run unless GSTN_SAVE=1 (Tools/CI/Save-Assets.ps1). Writes Saved/Mixamo/convert_<name>.log + a root/feet report; prints ESITO.
import math
import os
import unreal

NAME = os.environ.get('MIXAMO_NAME', 'AS_GSTN_Mixamo_Clip')
FBX = os.environ.get('MIXAMO_FBX', '')
SKIN = os.environ.get('MIXAMO_SKIN', '')
SAVE = os.environ.get('GSTN_SAVE') == '1'
DST = '/Game/GSTN/Animation/Mixamo'
SRC_DIR = f'{DST}/Source'
UEFN_MESH = '/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin'
UEFN_RIG = '/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin'
ROOTFIX = '/Game/Blueprints/Animation/Ported57/SkidOwner/RTG_RootCorrection'
LOG = os.path.join(unreal.Paths.project_saved_dir(), 'Mixamo', f'convert_{NAME}.log')
os.makedirs(os.path.dirname(LOG), exist_ok=True)
_f = open(LOG, 'w', encoding='utf-8')
errors = []


def log(s):
    _f.write(str(s) + '\n')
    _f.flush()


eal = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()


def import_fbx(path, dest, name, skeleton=None):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = dest
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = False
    o = unreal.FbxImportUI()
    o.automated_import_should_detect_type = False
    if skeleton is None:
        o.import_mesh = True
        o.import_as_skeletal = True
        o.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
        o.import_animations = False
        o.import_materials = False
        o.import_textures = False
        o.create_physics_asset = False
    else:
        o.import_mesh = False
        o.import_animations = True
        o.skeleton = skeleton
        o.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
        o.anim_sequence_import_data.set_editor_property('import_bone_tracks', True)
        o.anim_sequence_import_data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    t.options = o
    at.import_asset_tasks([t])
    log(f'import {os.path.basename(path)} -> {list(t.imported_object_paths)}')
    return [eal.load_asset(p) for p in t.imported_object_paths]


def report(anim, tag):
    L = unreal.AnimationLibrary.get_sequence_length(anim)
    n = 8
    for i in range(n + 1):
        t = L * i / n
        r = unreal.AnimationLibrary.get_bone_pose_for_time(anim, 'root', t, False)
        pv = unreal.AnimationLibrary.get_bone_pose_for_time(anim, 'pelvis', t, False)
        log(f'{tag} t={t:.3f} root=({r.translation.x:.1f},{r.translation.y:.1f},{r.translation.z:.1f}) yaw={r.rotation.rotator().yaw:.1f}'
            f' pelvis=({pv.translation.x:.1f},{pv.translation.y:.1f},{pv.translation.z:.1f})')
    log(f'{tag} length={L:.3f} root_motion={anim.get_editor_property("enable_root_motion")}')


def run():
    if not (os.path.isfile(FBX) and os.path.isfile(SKIN)):
        errors.append(f'fbx missing clip={FBX} skin={SKIN}')
        return
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.scan_paths_synchronous(['/Game'], True)
    # 1. X Bot skin (source skeleton), reused if already there
    skin_path = f'{SRC_DIR}/SKM_Mixamo_XBot'
    skin = eal.load_asset(skin_path) if eal.does_asset_exist(skin_path) else None
    if not skin:
        objs = import_fbx(SKIN, SRC_DIR, 'SKM_Mixamo_XBot')
        skin = next((o for o in objs if isinstance(o, unreal.SkeletalMesh)), None)
    if not skin:
        errors.append('X Bot import failed')
        return
    skel = skin.get_editor_property('skeleton')
    # 2. raw clip on the X Bot skeleton
    raw = next((o for o in import_fbx(FBX, SRC_DIR, f'{NAME}_Mixamo', skel) if isinstance(o, unreal.AnimSequence)), None)
    if not raw:
        errors.append('clip import failed')
        return
    log(f'raw length={unreal.AnimationLibrary.get_sequence_length(raw):.3f} frames={unreal.AnimationLibrary.get_num_frames(raw)}')
    # 3. source IK rig (auto) + retargeter Mixamo -> UEFN (auto map + auto align)
    rig_path = f'{SRC_DIR}/IK_Mixamo_XBot'
    rig = eal.load_asset(rig_path) if eal.does_asset_exist(rig_path) else None
    if not rig:
        rig = at.create_asset('IK_Mixamo_XBot', SRC_DIR, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        rc = unreal.IKRigController.get_controller(rig)
        rc.set_skeletal_mesh(skin)
        rc.apply_auto_generated_retarget_definition()
        rc.apply_auto_fbik()
    rc = unreal.IKRigController.get_controller(rig)
    log(f'src rig chains={[str(c.chain_name) for c in rc.get_retarget_chains()]} root={rc.get_retarget_root()}')
    rtg_path = f'{SRC_DIR}/RTG_Mixamo_to_UEFN'
    rtg = eal.load_asset(rtg_path) if eal.does_asset_exist(rtg_path) else None
    if not rtg:
        rtg = at.create_asset('RTG_Mixamo_to_UEFN', SRC_DIR, unreal.IKRetargeter, unreal.IKRetargetFactory())
        c = unreal.IKRetargeterController.get_controller(rtg)
        c.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rig)
        c.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, eal.load_asset(UEFN_RIG))
        if c.get_num_retarget_ops() == 0:
            c.add_default_ops()
        c.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, rig)
        c.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, eal.load_asset(UEFN_RIG))
        c.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        c.auto_align_all_bones(unreal.RetargetSourceOrTarget.SOURCE)
    c = unreal.IKRetargeterController.get_controller(rtg)
    log(f'rtg ops={[str(c.get_op_name(i)) for i in range(c.get_num_retarget_ops())]}')
    uefn = eal.load_asset(UEFN_MESH)
    raw_data = eal.find_asset_data(raw.get_path_name())
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([raw_data], skin, uefn, rtg, '', '', '', '_UEFN', target_path=SRC_DIR)
    log(f'retarget1 -> {[str(r.package_name) for r in res]}')
    mid_path = f'{SRC_DIR}/{NAME}_Mixamo_UEFN'
    mid = eal.load_asset(mid_path)
    if not mid:
        errors.append('retarget Mixamo->UEFN produced nothing')
        return
    report(mid, 'MID')
    # 4. owner's root fix: root motion from pelvis (UEFN -> UEFN)
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([eal.find_asset_data(mid_path)], uefn, uefn, eal.load_asset(ROOTFIX),
                                                                '', '', '', '_RF', target_path=SRC_DIR)
    log(f'retarget2 -> {[str(r.package_name) for r in res]}')
    rf = eal.load_asset(f'{mid_path}_RF')
    if not rf:
        errors.append('root fix produced nothing')
        return
    final_path = f'{DST}/{NAME}'
    if eal.does_asset_exist(final_path):
        eal.delete_asset(final_path)
    at.rename_assets([unreal.AssetRenameData(rf, DST, NAME)])
    out = eal.load_asset(final_path)
    out.set_editor_property('enable_root_motion', True)
    out.set_editor_property('force_root_lock', False)
    report(out, 'FINAL')
    if SAVE:
        eal.save_directory(DST, only_if_is_dirty=False, recursive=True)
        log('saved')


try:
    run()
except Exception as e:
    import traceback
    errors.append(f'exception {e}')
    log(traceback.format_exc())
log(f'errors={errors}')
_f.close()
print(f'ESITO: {"OK" if not errors else "ERRORE " + "; ".join(errors)} {"(salvato)" if SAVE else "(dry run)"}')
