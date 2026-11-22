# Owner 2026-11-18: our assets live under /Game/GSTN, apart from GASP's. Moves the locomotion ABP and chooser copies (nothing references
# them yet) and fixes the redirectors. Idempotent.
import unreal

MOVES = [
    ('/Game/Blueprints/Animation/GSTNLocomotion/ABP_GSTN_Locomotion', '/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion'),
    ('/Game/Blueprints/Animation/GSTNLocomotion/CHT_GSTN_LocomotionAnimations', '/Game/GSTN/Animation/Locomotion/CHT_GSTN_LocomotionAnimations'),
]
OUT = unreal.Paths.project_saved_dir() + 'Logs/ci/move_locomotion_copies.txt'
lines = []
for src, dst in MOVES:
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        lines.append('EXISTS ' + dst)
        continue
    ok = unreal.EditorAssetLibrary.rename_asset(src, dst)
    lines.append(('OK ' if ok else 'FAIL ') + dst)
reds = unreal.EditorAssetLibrary.list_assets('/Game/Blueprints/Animation/GSTNLocomotion', recursive=True)
if reds:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    objs = [unreal.load_asset(p) for p in reds]
    tools.fixup_referencers([o for o in objs if isinstance(o, unreal.ObjectRedirector)])
    for p in reds:
        unreal.EditorAssetLibrary.delete_asset(p)
    lines.append('redirectors fixed %d' % len(reds))
unreal.EditorAssetLibrary.delete_directory('/Game/Blueprints/Animation/GSTNLocomotion')
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
