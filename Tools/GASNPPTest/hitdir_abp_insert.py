# Inserts the directional hit additive node (FGSTN_AnimNode_DirectionalHitAdditive) in the main AnimGraph of the Mover ABP, right after the
# blend-out foot lock (montage slot upstream, so the additive rides on the reaction montages). Idempotent: InsertPoseNodeAfter does nothing
# when the node is already there. Backs the ABP up first and writes the graph before/after for the diff.
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GASNPPTest/hitdir_abp_insert.py" -unattended -nullrhi
# Prints at most 5 lines; full log Saved/HitDir/hitdir_abp_insert.log.
import os
import shutil

import unreal

ABP = '/Game/Blueprints/SandboxCharacter_Mover_ABP'
UPSTREAM = 'GSTN Blend-Out Foot Lock'

proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
out_dir = os.path.join(proj, 'Saved', 'HitDir')
os.makedirs(out_dir, exist_ok=True)
lines = []


def log(msg):
    lines.append(msg)
    unreal.log(f'[hitdir_abp_insert] {msg}')


def run():
    src = os.path.join(proj, 'Content', 'Blueprints', 'SandboxCharacter_Mover_ABP.uasset')
    bak = os.path.join(proj, 'Saved', 'Backups', 'GAS', 'hitdir')
    os.makedirs(bak, exist_ok=True)
    shutil.copy2(src, os.path.join(bak, 'SandboxCharacter_Mover_ABP.uasset'))
    abp = unreal.load_asset(ABP)
    lib = unreal.GSTN_AnimGraphAuthoringLibrary
    with open(os.path.join(out_dir, 'abp_before.txt'), 'w', encoding='utf-8') as f:
        f.write(lib.describe_graphs(abp))
    ok = lib.insert_pose_node_after(abp, UPSTREAM, unreal.GSTN_AnimGraphNode_DirectionalHitAdditive)
    if not ok:
        log('ESITO: FAIL insert (see editor log)')
        return
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(abp, False)
    with open(os.path.join(out_dir, 'abp_after.txt'), 'w', encoding='utf-8') as f:
        f.write(lib.describe_graphs(abp))
    log(f'ESITO: OK inserted after {UPSTREAM}, saved={saved}')


try:
    run()
except Exception as e:  # noqa: BLE001
    log(f'ESITO: FAIL {e}')
with open(os.path.join(out_dir, 'hitdir_abp_insert.log'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
for line in lines[-5:]:
    print(line)
