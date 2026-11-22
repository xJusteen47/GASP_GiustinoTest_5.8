# Placeholder look of the test backpacks (owner, 2026-11-13: a worn backpack must show on the back). Every backpack below with an empty
# Visual gets two to four engine basic shapes, each backpack its own shapes and colours so they read apart in play: JetPack = two orange
# tanks with dark nozzles, ShieldDome = blue box with a cyan dome on top, Example Dome = purple box with a yellow disc. Cheap, toy-like,
# no Nanite, no new asset: /Engine/BasicShapes meshes recoloured through BasicShapeMaterial's Color parameter.
#
# The parts name no socket: the backpack plugin puts them on the kit implant's Connector_Backpack, and without an implant the skin
# component maps that socket to bone spine_05. Offsets are in that bone's space on the UE5 mannequin (GASP): X up the spine, Y forward,
# Z to the right, so the back is -Y; they sit just behind the kit's placeholder box (25-35 cm behind the bone).
# Assets a designer already gave a look are left alone. Idempotent. Saves .uasset files only, never maps.
#
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_backpack_visuals.py [<report file>]"
# In the editor: py "<project>/Tools/GAS/setup_backpack_visuals.py"
import os
import sys

import unreal

CUBE = '/Engine/BasicShapes/Cube.Cube'
SPHERE = '/Engine/BasicShapes/Sphere.Sphere'

ORANGE = (1.0, 0.35, 0.02)
DARK = (0.08, 0.08, 0.08)
BLUE = (0.05, 0.15, 0.8)
CYAN = (0.2, 0.9, 1.0)
PURPLE = (0.5, 0.05, 0.7)
YELLOW = (1.0, 0.85, 0.05)

# (mesh, location in spine_05 space, scale, colour)
LOOKS = {
    '/Game/GAS/Backpacks/DA_Backpack_JetPack': [
        (SPHERE, (-4.0, -40.0, -11.0), (0.50, 0.20, 0.20), ORANGE),
        (SPHERE, (-4.0, -40.0, 11.0), (0.50, 0.20, 0.20), ORANGE),
        (CUBE, (-30.0, -40.0, -11.0), (0.08, 0.12, 0.12), DARK),
        (CUBE, (-30.0, -40.0, 11.0), (0.08, 0.12, 0.12), DARK),
    ],
    '/Game/GAS/Backpacks/DA_Backpack_ShieldDome': [
        (CUBE, (-8.0, -40.0, 0.0), (0.40, 0.16, 0.34), BLUE),
        (SPHERE, (14.0, -40.0, 0.0), (0.26, 0.26, 0.26), CYAN),
    ],
    '/Game/GASNPPTest/Examples/DA_Example_Backpack_Dome': [
        (CUBE, (-6.0, -40.0, 0.0), (0.30, 0.18, 0.40), PURPLE),
        (SPHERE, (-6.0, -50.0, 0.0), (0.24, 0.08, 0.24), YELLOW),
    ],
}


def _part(mesh, location, scale, colour):
    part = unreal.GSTN_AttachmentRequest()
    part.set_editor_property('mesh', unreal.load_asset(mesh))
    part.set_editor_property('visual_slot', 'Backpack')
    part.set_editor_property('offset', unreal.Transform(unreal.Vector(*location), unreal.Rotator(0.0, 0.0, 0.0), unreal.Vector(*scale)))
    part.set_editor_property('vector_parameters', {'Color': unreal.LinearColor(colour[0], colour[1], colour[2], 1.0)})
    return part


def run(report_path=None):
    report_path = report_path or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backpack', 'setup_backpack_visuals.txt')
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    lines = []
    changed = 0
    for path, parts in LOOKS.items():
        asset = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not asset:
            lines.append(f'{path}: missing')
            continue
        if len(asset.get_editor_property('visual').get_editor_property('parts')) > 0:
            lines.append(f'{path}: already has a look, left alone')
            continue
        visual = unreal.GSTN_EquipmentVisual()
        visual.set_editor_property('parts', [_part(*p) for p in parts])
        asset.set_editor_property('visual', visual)
        ok = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        changed += 1
        lines.append(f'{path}: {len(parts)} parts, saved={ok}')
    lines.append(f'SUMMARY: {changed} backpacks given a placeholder look')
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    unreal.log(f'setup_backpack_visuals: {lines[-1]} (report {report_path})')


run(sys.argv[1] if len(sys.argv) > 1 else None)
