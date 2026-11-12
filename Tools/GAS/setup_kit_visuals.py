# M11 L6: placeholder equipment look for the test kits and weapons (combat-loadout-mods 9), so a kit swap in the hub shows on the
# pawn. Every KIT_Test_* kit with an empty Visual gets a back implant (SM_Implant_Placeholder, a flat box carrying the
# Connector_Backpack socket) tinted with the kit's own colour; every WPN_Test_* weapon with an empty Visual gets two small spheres
# on Implant_Hand_R and Implant_Foot_R (the weapon implants). The skin component maps those sockets to mannequin bones until the
# skeleton has them. Assets a designer already gave a look are left alone. Idempotent: a second run changes and saves nothing.
# Saves .uasset files only, never maps. Editor tooling: run it on the machine that owns the content (desktop), not on two at once.
#
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_kit_visuals.py [<report file>]"
# In the editor: py "<project>/Tools/GAS/setup_kit_visuals.py"
# Writes its report to the file (default Saved/M11L6/setup_kit_visuals.txt) and logs one summary line.
import colorsys
import os
import sys
import zlib

import unreal

IMPLANT_PATH = '/Game/Blueprints/Equipment/SM_Implant_Placeholder'
CUBE = '/Engine/BasicShapes/Cube.Cube'
SPHERE = '/Engine/BasicShapes/Sphere.Sphere'
CONNECTOR = 'Connector_Backpack'


def _assets_of(cls, prefix):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    path = unreal.TopLevelAssetPath(cls.static_class().get_outer().get_path_name(), cls.static_class().get_name())
    return [d for d in registry.get_assets_by_class(path, True)
            if str(d.asset_name).startswith(prefix) and not str(d.package_name).startswith('/Engine/')]


def _implant_mesh(lines):
    """The placeholder back implant: the engine cube duplicated once, with the connector socket on its back face"""
    mesh = unreal.EditorAssetLibrary.load_asset(IMPLANT_PATH) if unreal.EditorAssetLibrary.does_asset_exist(IMPLANT_PATH) else None
    if not mesh:
        mesh = unreal.EditorAssetLibrary.duplicate_asset(CUBE.split('.')[0], IMPLANT_PATH)
        lines.append(f'created {IMPLANT_PATH}')
    if mesh and not mesh.find_socket(CONNECTOR):
        socket = unreal.StaticMeshSocket(mesh)
        socket.set_editor_property('socket_name', CONNECTOR)
        # The cube is 100 cm centred on its origin: the connector sits on its +X face (away from the spine with the offset below)
        socket.set_editor_property('relative_location', unreal.Vector(50.0, 0.0, 0.0))
        mesh.add_socket(socket)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        lines.append(f'{IMPLANT_PATH}: added socket {CONNECTOR}')
    return mesh


def _colour(name):
    """A stable, distinct colour per kit name"""
    hue = (zlib.crc32(name.encode()) % 360) / 360.0
    r, g, b = colorsys.hsv_to_rgb(hue, 0.75, 0.9)
    return unreal.LinearColor(r, g, b, 1.0)


def _part(mesh, socket, slot, location, scale, colour=None):
    part = unreal.GSTN_AttachmentRequest()
    part.set_editor_property('socket', socket)
    part.set_editor_property('mesh', mesh)
    part.set_editor_property('visual_slot', slot)
    part.set_editor_property('offset', unreal.Transform(location, unreal.Rotator(0.0, 0.0, 0.0), scale))
    if colour is not None:
        part.set_editor_property('vector_parameters', {'Color': colour})
    return part


def _set_visual(asset, parts):
    visual = unreal.GSTN_EquipmentVisual()
    visual.set_editor_property('parts', parts)
    asset.set_editor_property('visual', visual)
    return unreal.EditorAssetLibrary.save_loaded_asset(asset, False)


def run(report_path=None):
    report_path = report_path or os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'M11L6', 'setup_kit_visuals.txt')
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    lines = []
    if not hasattr(unreal, 'GSTN_EquipmentVisual'):
        lines.append('ABORT: unreal.GSTN_EquipmentVisual not found (editor built before M11 L6?)')
    else:
        implant = _implant_mesh(lines)
        sphere = unreal.load_asset(SPHERE)
        changed = 0
        for data in _assets_of(unreal.GSTN_PrimaryKitDefinition, 'KIT_Test_'):
            kit = unreal.EditorAssetLibrary.load_asset(str(data.package_name))
            if not kit or len(kit.get_editor_property('visual').get_editor_property('parts')) > 0:
                continue
            back = _part(implant, 'Implant_Back', 'Implant.Back', unreal.Vector(0.0, -18.0, 0.0), unreal.Vector(0.08, 0.30, 0.40),
                         _colour(str(data.asset_name)))
            ok = _set_visual(kit, [back])
            changed += 1
            lines.append(f'{data.package_name}: back implant, saved={ok}')
        for data in _assets_of(unreal.GSTN_WeaponDefinition, 'WPN_Test_'):
            weapon = unreal.EditorAssetLibrary.load_asset(str(data.package_name))
            if not weapon or len(weapon.get_editor_property('visual').get_editor_property('parts')) > 0:
                continue
            small = unreal.Vector(0.08, 0.08, 0.08)
            parts = [_part(sphere, 'Implant_Hand_R', 'Implant.Hand', unreal.Vector(0.0, 0.0, 0.0), small),
                     _part(sphere, 'Implant_Foot_R', 'Implant.Foot', unreal.Vector(0.0, 0.0, 0.0), small)]
            ok = _set_visual(weapon, parts)
            changed += 1
            lines.append(f'{data.package_name}: hand and foot implants, saved={ok}')
        lines.append(f'SUMMARY: {changed} assets given a placeholder look')
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    unreal.log(f'setup_kit_visuals: {lines[-1]} (report {report_path})')


run(sys.argv[1] if len(sys.argv) > 1 else None)
