# run-start-stop-skid-1 (owner 2026-11-18): our own copies of GASP's main Mover ABP and its raw chooser, so the skid row and state can be
# added without touching GASP's originals (going back = point the locomotion profile at the original class). Idempotent: skips existing copies.
import unreal

COPIES = [
    ('/Game/Blueprints/SandboxCharacter_Mover_ABP', '/Game/Blueprints/Animation/GSTNLocomotion/ABP_GSTN_Locomotion'),
    ('/Game/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_MoverCharacterAnimations_PoseMatch',
     '/Game/Blueprints/Animation/GSTNLocomotion/CHT_GSTN_LocomotionAnimations'),
]
OUT = unreal.Paths.project_saved_dir() + 'Logs/ci/copy_locomotion_abp.txt'
lines = []
for src, dst in COPIES:
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        lines.append('EXISTS ' + dst)
        continue
    ok = unreal.EditorAssetLibrary.duplicate_asset(src, dst)
    if ok:
        unreal.EditorAssetLibrary.save_asset(dst, only_if_is_dirty=False)
    lines.append(('OK ' if ok else 'FAIL ') + dst)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
print(' | '.join(lines))
