# Skid clip SS_180_Turn2 from the owner's 5.7 project (GaspMoverProject/Content/GiustinoMoves/Skid), handover hitstop-downed-jab-1 addendum 2.
# Staging: the .uasset files are copied by hand to Content/GiustinoMoves/Skid (same package path as in 5.7), then this script:
#  1. moves SS_180_Turn2 -> Ported57/SkidOwner/SS_180_Turn2_Src and the owner's RTG_RootCorrection -> Ported57/SkidOwner/RTG_RootCorrection
#  2. applies the owner's root fix = his IK Retargeter (UEFN -> UEFN, root motion op GenerateFromTargetPelvis, height from source)
#     with a batch retarget of the COPY only (SS_180_Turn2_Src -> SS_180_Turn2_Src_RF), logic untouched
#  3. renames the result to Ported57/SkidOwner/SS_180_Turn2 (the default SKID_CLIP of setup_skid_anim_layer.py)
# Writes Saved/SkidOwner/port_skid_owner_clip.log; prints ESITO.
import os
import unreal

LOG = os.path.join(unreal.Paths.project_saved_dir(), 'SkidOwner', 'port_skid_owner_clip.log')
os.makedirs(os.path.dirname(LOG), exist_ok=True)
_f = open(LOG, 'w', encoding='utf-8')


def log(s):
    _f.write(str(s) + '\n')
    _f.flush()


DST = '/Game/GSTN/Animation/Ported57/SkidOwner'
MESH = '/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin'
at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
errors = []


def move(src, dst):
    if eal.does_asset_exist(dst):
        log(f'exists {dst}')
        return True
    a = eal.load_asset(src)
    if not a:
        errors.append(f'missing {src}')
        return False
    ok = at.rename_assets([unreal.AssetRenameData(a, os.path.dirname(dst), os.path.basename(dst))])
    log(f'move {src} -> {dst} ok={ok}')
    return ok


def root_report(anim, tag):
    try:
        n = anim.get_editor_property('number_of_sampled_keys') if hasattr(anim, 'number_of_sampled_keys') else None
        length = unreal.AnimationLibrary.get_sequence_length(anim)
        for t in (0.0, length * 0.5, length):
            tr = unreal.AnimationLibrary.get_bone_pose_for_time(anim, 'root', t, False)
            pv = unreal.AnimationLibrary.get_bone_pose_for_time(anim, 'pelvis', t, False)
            log(f'{tag} t={t:.3f} root loc={tr.translation} yaw={tr.rotation.rotator().yaw:.1f} | pelvis loc={pv.translation}')
        log(f'{tag} length={length:.3f} rootmotion={anim.get_editor_property("enable_root_motion")} keys={n}')
    except Exception as e:
        log(f'{tag} root report error {e}')


def run():
    move('/Game/GiustinoMoves/Skid/SS_180_Turn2', f'{DST}/SS_180_Turn2_Src')
    move('/Game/GiustinoMoves/Skid/RootCorrection/RTG_RootCorrection', f'{DST}/RTG_RootCorrection')
    reds = [a.get_asset() for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/GiustinoMoves/Skid', recursive=True)
            if str(a.asset_class_path.asset_name) == 'ObjectRedirector']
    if reds:
        at.fix_up_redirectors(reds)
    src = eal.load_asset(f'{DST}/SS_180_Turn2_Src')
    rtg = eal.load_asset(f'{DST}/RTG_RootCorrection')
    mesh = eal.load_asset(MESH)
    if not (src and rtg and mesh):
        errors.append(f'load src={bool(src)} rtg={bool(rtg)} mesh={bool(mesh)}')
        return
    root_report(src, 'SRC')
    out_path = f'{DST}/SS_180_Turn2'
    if not eal.does_asset_exist(out_path):
        data = unreal.EditorAssetLibrary.find_asset_data(f'{DST}/SS_180_Turn2_Src')
        res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([data], mesh, mesh, rtg, '', '', '', '_RF', target_path=DST)
        log(f'retarget results {[str(r.package_name) for r in res]}')
        rf = f'{DST}/SS_180_Turn2_Src_RF'
        if not eal.does_asset_exist(rf):
            errors.append('retarget produced no asset')
            return
        move(rf, out_path)
    out = eal.load_asset(out_path)
    root_report(out, 'FIXED')
    eal.save_directory(DST, only_if_is_dirty=False, recursive=True)


try:
    run()
except Exception as e:
    errors.append(repr(e))
for e in errors:
    log('ERROR ' + e)
_f.close()
print(f'ESITO: {"OK" if not errors else "FAIL"} port skid owner clip ({len(errors)} errors) log {LOG}')
