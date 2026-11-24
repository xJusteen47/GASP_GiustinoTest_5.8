"""Owner 2026-11-18: movement/VFX refinements on every kit, kits differ only by moves. Idempotent, no editor (UnrealEditor-Cmd -run=pythonscript).
- DA_Locomotion_Default: Jump Designer on (350 cm full jump, apex 0.5 s: the struct defaults);
- KIT_Test_Playtest back on DA_Locomotion_Default; DA_Locomotion_Playtest deleted;
- the air crouch extension on every KIT_Test_* that lacks it (UGSTN_AirCrouchContentLibrary.ensure_kit_air_crouch).
Report -> Saved/Diag1118/align_kits.txt"""
import os
import re
import unreal

L = unreal.GSTN_PropertyTextLibrary
out = []
restore = []
COMMON = ['(Stat=(TagName="Combat.SetByCaller.Stat.WalkSpeed"),Value=600.000000)', '(Stat=(TagName="Combat.SetByCaller.Stat.JumpVelocity"),Value=1350.000000)']
DEFAULT = '/Game/GSTN/Data/Locomotion/DA_Locomotion_Default'
PLAYTEST = '/Game/GSTN/Data/Locomotion/DA_Locomotion_Playtest'

da = unreal.load_asset(DEFAULT)
# The pink kit's designer jump is the one the owner played and tuned (2026-11-18 12:00): it becomes the common one; the Default's own values go to the report
if unreal.EditorAssetLibrary.does_asset_exist(PLAYTEST):
    out.append('default designer before ' + L.get_property_text(da, 'JumpDesigner'))
    L.set_property_text(da, 'JumpDesigner', L.get_property_text(unreal.load_asset(PLAYTEST), 'JumpDesigner'))
else:
    jd = da.get_editor_property('jump_designer')
    jd.set_editor_property('enabled', True)
    da.set_editor_property('jump_designer', jd)
out.append('default designer ' + L.get_property_text(da, 'JumpDesigner')[:120])
out.append(f'save default {unreal.EditorAssetLibrary.save_loaded_asset(da, False)}')

for path in unreal.EditorAssetLibrary.list_assets('/Game/GSTN', recursive=True, include_folder=False):
    name = path.split('/')[-1].split('.')[0]
    if not name.startswith('KIT_Test_'):
        continue
    kit = unreal.load_asset(path)
    changed = []
    t = L.get_property_text(kit, 'Extensions')
    if 'DA_Locomotion_Playtest' in t:
        L.set_property_text(kit, 'Extensions', t.replace('DA_Locomotion_Playtest', 'DA_Locomotion_Default'))
        changed.append('locomotion -> Default')
    # Movement stats as KIT_Test_Playtest's (WalkSpeed 600, JumpVelocity 1350, the rest at the stat defaults); Weight and MaxCarryWeight are
    # not movement feel and stay. The old text goes to the report so it can be put back
    stats = L.get_property_text(kit, 'Stats')
    keep = re.findall(r'\(Stat=\(TagName="Combat\.SetByCaller\.Stat\.(?:Weight|MaxCarryWeight)"\),Value=[-0-9.]+\)', stats)
    wanted = '(Values=(' + ','.join(COMMON + keep) + '))'
    if stats != wanted:
        restore.append(f'{name}\t{stats}')
        L.set_property_text(kit, 'Stats', wanted)
        changed.append('movement stats -> common')
    if not unreal.GSTN_AirCrouchContentLibrary.has_kit_air_crouch(kit):
        if unreal.GSTN_AirCrouchContentLibrary.ensure_kit_air_crouch(kit):
            changed.append('air crouch added')
    if changed:
        out.append(f'{name}: {", ".join(changed)} save {unreal.EditorAssetLibrary.save_loaded_asset(kit, False)}')

if unreal.EditorAssetLibrary.does_asset_exist(PLAYTEST):
    out.append(f'delete playtest DA {unreal.EditorAssetLibrary.delete_asset(PLAYTEST)}')
d = os.path.join(unreal.Paths.project_saved_dir(), 'Diag1118')
os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'align_kits.txt'), 'w').write('\n'.join(out) + '\n')
if restore:
    open(os.path.join(d, 'kit_stats_before_align.tsv'), 'w').write('\n'.join(restore) + '\n')
print('ALIGN_KITS', len(out))
