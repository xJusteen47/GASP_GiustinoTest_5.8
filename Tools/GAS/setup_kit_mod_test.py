# M11 desktop verify: the first mod and backpack content, data only (combat-loadout-mods.md §8, §10).
#   KIT_Test_Modded   KIT_Test_Hits duplicated, identity Kit.Test.Modded, one Kit mod slots extension (Mod.Slot.Blade)
#   MOD_Test_LongBlade identity Mod.Test.LongBlade on Mod.Slot.Blade: every move x1.5 time, x1.3 reach and hitbox (empty filter = all)
#   BPK_Test_JetPack  identity Backpack.Test.JetPack: a flat box on the implant connector, no ability yet (needs a Blueprint ability)
# The tags are ini leaves in Config/Tags/GSTNCombatTest.ini. Idempotent: an asset that exists is left alone. Saves .uasset files only.
# Headless (editor closed; the script path must have no spaces in its arguments):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_kit_mod_test.py"
# Writes Saved/M11Verify/setup_kit_mod_test.txt and logs one summary line.
import os

import unreal

FOLDER = '/Game/GASNPPTest/Combat'
BASE_KIT = FOLDER + '/KIT_Test_Hits'
KIT = FOLDER + '/KIT_Test_Modded'
MOD = FOLDER + '/MOD_Test_LongBlade'
BACKPACK = FOLDER + '/BPK_Test_JetPack'
CUBE = '/Engine/BasicShapes/Cube.Cube'


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="%s")' % name)
    return tag


def _tags(*names):
    container = unreal.GameplayTagContainer()
    container.import_text('(GameplayTags=(%s))' % ','.join('(TagName="%s")' % n for n in names))
    return container


def _instanced(struct_value):
    """An FInstancedStruct holding struct_value, through its text form (/Script/Module.Struct + the struct's own text)"""
    inst = unreal.InstancedStruct()
    path = struct_value.static_struct().get_path_name()
    inst.import_text('%s%s' % (path, struct_value.export_text()))
    return inst


def _new(path, cls, lines):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        lines.append('%s: exists, left alone' % path)
        return None
    folder, name = path.rsplit('/', 1)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, None)
    lines.append('%s: created' % path)
    return asset


def _kit(lines):
    if unreal.EditorAssetLibrary.does_asset_exist(KIT):
        lines.append('%s: exists, left alone' % KIT)
        return
    kit = unreal.EditorAssetLibrary.duplicate_asset(BASE_KIT, KIT)
    kit.set_editor_property('kit_tags', _tags('Kit.Test.Modded'))
    slots = unreal.GSTN_KitModSlots()
    slots.set_editor_property('slots', [_tag('Mod.Slot.Blade')])
    ext = kit.get_editor_property('extensions')
    data = list(ext.get_editor_property('extension_data'))
    data.append(_instanced(slots))
    ext.set_editor_property('extension_data', data)
    kit.set_editor_property('extensions', ext)
    unreal.EditorAssetLibrary.save_loaded_asset(kit, False)
    lines.append('%s: duplicated from KIT_Test_Hits, extensions %s' % (KIT, ext.export_text()))


def _mod(lines):
    mod = _new(MOD, unreal.GSTN_KitModDefinition, lines)
    if not mod:
        return
    mod.set_editor_property('mod_tags', _tags('Mod.Test.LongBlade'))
    mod.set_editor_property('slot', _tag('Mod.Slot.Blade'))
    modifier = unreal.GSTN_MoveModifier()
    modifier.set_editor_property('time_scale', 1.5)
    modifier.set_editor_property('reach_scale', 1.3)
    modifier.set_editor_property('hitbox_scale', unreal.Vector(1.3, 1.3, 1.3))
    mod.set_editor_property('move_modifiers', [modifier])
    unreal.EditorAssetLibrary.save_loaded_asset(mod, False)


def _backpack(lines):
    pack = _new(BACKPACK, unreal.GSTN_BackpackDefinition, lines)
    if not pack:
        return
    pack.set_editor_property('backpack_tags', _tags('Backpack.Test.JetPack'))
    part = unreal.GSTN_AttachmentRequest()
    part.set_editor_property('mesh', unreal.load_asset(CUBE))
    # A 30 x 40 x 50 cm box standing behind the connector (no Socket, no ParentSlot = the implant's Connector_Backpack)
    part.set_editor_property('offset', unreal.Transform(unreal.Vector(15.0, 0.0, 0.0), unreal.Rotator(0, 0, 0), unreal.Vector(0.3, 0.4, 0.5)))
    part.set_editor_property('visual_slot', 'Backpack')
    visual = pack.get_editor_property('visual')
    visual.set_editor_property('parts', [part])
    pack.set_editor_property('visual', visual)
    unreal.EditorAssetLibrary.save_loaded_asset(pack, False)


def main():
    lines = []
    for step in (_kit, _mod, _backpack):
        try:
            step(lines)
        except Exception as e:  # report and go on: one broken step must not hide the others
            lines.append('%s failed: %s' % (step.__name__, e))
    report = os.path.join(unreal.Paths.project_saved_dir(), 'M11Verify', 'setup_kit_mod_test.txt')
    os.makedirs(os.path.dirname(report), exist_ok=True)
    with open(report, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    unreal.log('setup_kit_mod_test: %d lines, %d failed -> %s' % (len(lines), sum('failed' in l for l in lines), report))


main()
