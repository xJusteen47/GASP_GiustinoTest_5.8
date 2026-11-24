# C5 cleanup (2026-11-24): the Content reorg moved SM_Implant_Placeholder from /Game/Blueprints/Equipment to
# /Game/GSTN/Equipment/Placeholders without a redirector, and KIT_Test_PlaytestTBK5/TBK6 were not open in that editor: their back
# implant part now loads with mesh = None (the hub shows no implant for them). This script re-points every part of a KIT_Test_* /
# WPN_Test_* visual whose mesh is missing AND whose socket is Implant_Back to the moved mesh (what setup_kit_visuals.py gives a kit).
# Parts with a mesh, other sockets and every other property are left as they are.
# Dry run by default (lists what it would change). Saves only with GSTN_SAVE=1, i.e. through Tools/CI/Save-Assets.ps1 (editor closed):
#   & .\Tools\CI\Save-Assets.ps1 -Script Tools\GAS\fix_kit_implant_refs.py -Tag kit_implant
# Report: Saved/C5/fix_kit_implant_refs.txt; last line "SUMMARY: ...".
import os

import unreal

IMPLANT_PATH = '/Game/GSTN/Equipment/Placeholders/SM_Implant_Placeholder'
SOCKET = 'Implant_Back'
WRITE = os.environ.get('GSTN_SAVE') == '1'
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'C5', 'fix_kit_implant_refs.txt')


def _assets(cls, prefix):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    reg.scan_paths_synchronous(['/Game'], True)
    path = unreal.TopLevelAssetPath(cls.static_class().get_outer().get_path_name(), cls.static_class().get_name())
    return [d for d in reg.get_assets_by_class(path, True)
            if str(d.asset_name).startswith(prefix) and not str(d.package_name).startswith('/Engine/')]


def run():
    lines = []
    mesh = unreal.EditorAssetLibrary.load_asset(IMPLANT_PATH)
    if not mesh:
        lines.append(f'ABORT: {IMPLANT_PATH} not found')
    else:
        fixed = 0
        for cls, prefix in ((unreal.GSTN_PrimaryKitDefinition, 'KIT_Test_'), (unreal.GSTN_WeaponDefinition, 'WPN_Test_')):
            for d in _assets(cls, prefix):
                a = unreal.EditorAssetLibrary.load_asset(str(d.package_name))
                if not a:
                    continue
                visual = a.get_editor_property('visual')
                parts = list(visual.get_editor_property('parts'))
                hit = [i for i, p in enumerate(parts) if p.get_editor_property('mesh') is None and str(p.get_editor_property('socket')) == SOCKET]
                if not hit:
                    continue
                for i in hit:
                    parts[i].set_editor_property('mesh', mesh)
                visual.set_editor_property('parts', parts)
                a.set_editor_property('visual', visual)
                saved = unreal.EditorAssetLibrary.save_loaded_asset(a, False) if WRITE else 'dry'
                fixed += 1
                lines.append(f'{d.package_name}: {len(hit)} part(s) -> {IMPLANT_PATH}, saved={saved}')
        lines.append(f'SUMMARY: {fixed} asset(s) {"fixed" if WRITE else "would be fixed (dry run)"}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    unreal.log(f'fix_kit_implant_refs: {lines[-1]} (report {OUT})')
    print('ESITO: OK' if not lines[-1].startswith('ABORT') else 'ESITO: FAIL')


run()
