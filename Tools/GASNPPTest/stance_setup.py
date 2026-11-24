# STANCE-OFFSET (2026-11-16): puts the GSTN Stance Offset node in the main ABP right after the blend-out foot lock, and makes the TEST stance
# asset (GASP's Relaxed idle over the Neutral one; not assigned to any kit). Idempotent: no change when the node is already there or the asset
# exists. Backs the ABP up to Saved/Backups/GAS/stance first. Writes <out>/stance_setup.json (graphs before/after, checks) and done.txt, then quits.
# Boot (no space in the -ExecCmds path): exec(open(r"<project>/Tools/GASNPPTest/stance_setup.py").read())   Env: STANCE_OUT (output folder)
import json
import os
import runpy
import shutil
import traceback

import unreal

_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.environ.get('STANCE_OUT') or os.path.join(_PROJ, 'Saved', 'Stance', 'setup')
os.makedirs(OUT, exist_ok=True)
ABP = '/Game/Blueprints/SandboxCharacter_Mover_ABP'
UPSTREAM = 'GSTN Blend-Out Foot Lock'
STANCE_DIR, STANCE_NAME = '/Game/GSTN/_Test/Stances', 'DA_Stance_TestRelaxed'
STANCE_POSE = '/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Relaxed_Stand_Idle_Loop'
REFERENCE_POSE = '/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop'

result = {}
try:
    try:
        runpy.run_path(os.path.join(_PROJ, 'Tools', 'GASNPPTest', 'vd2_mover.py'), run_name='vd2_mover')
    except Exception as e:
        result['vd2'] = repr(e)[:120]

    lib = unreal.GSTN_AnimGraphAuthoringLibrary
    abp = unreal.load_asset(ABP)
    result['graph_before'] = lib.describe_graphs(abp)
    backup = os.path.join(_PROJ, 'Saved', 'Backups', 'GAS', 'stance')
    os.makedirs(backup, exist_ok=True)
    src = os.path.join(_PROJ, 'Content', 'Blueprints', 'SandboxCharacter_Mover_ABP.uasset')
    if not os.path.exists(os.path.join(backup, 'SandboxCharacter_Mover_ABP.uasset')):
        shutil.copy2(src, backup)
    changed = lib.insert_pose_node_after(abp, UPSTREAM, unreal.GSTN_AnimGraphNode_StanceOffset.static_class())
    result['inserted'] = bool(changed)
    report, ok = lib.compile_with_report(abp)
    result['compile_ok'], result['compile_report'] = bool(ok), str(report)[:2000]
    result['graph_after'] = lib.describe_graphs(abp)
    if changed and ok:
        result['abp_saved'] = bool(unreal.EditorAssetLibrary.save_loaded_asset(abp, False))

    # Test stance asset + additive compatibility checks
    stance_pose, reference_pose = unreal.load_asset(STANCE_POSE), unreal.load_asset(REFERENCE_POSE)
    result['skeletons'] = [stance_pose.get_editor_property('skeleton').get_path_name(), reference_pose.get_editor_property('skeleton').get_path_name(),
                           abp.get_editor_property('target_skeleton').get_path_name()]
    result['additive_types'] = [str(stance_pose.get_editor_property('additive_anim_type')), str(reference_pose.get_editor_property('additive_anim_type'))]
    result['lengths'] = [stance_pose.get_play_length(), reference_pose.get_play_length()]
    path = f'{STANCE_DIR}/{STANCE_NAME}'
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if asset is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.GSTN_StanceDefinition)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(STANCE_NAME, STANCE_DIR, unreal.GSTN_StanceDefinition, factory)
        asset.set_editor_property('stance_pose', stance_pose)
        asset.set_editor_property('reference_pose', reference_pose)
        result['stance_saved'] = bool(unreal.EditorAssetLibrary.save_loaded_asset(asset, False))
    tracks = [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(stance_pose)]
    result['listed_bones_missing'] = [str(w.bone) for w in asset.get_editor_property('bone_weights') if str(w.bone) not in tracks]
    result['stance_path'] = asset.get_path_name()
    status = 'ok'
except Exception:
    result['error'] = traceback.format_exc()[-1500:]
    status = 'error'

with open(os.path.join(OUT, 'stance_setup.json'), 'w') as f:
    json.dump(result, f, indent=1)
with open(os.path.join(OUT, 'done.txt'), 'w') as f:
    f.write(status)
unreal.SystemLibrary.quit_editor()
