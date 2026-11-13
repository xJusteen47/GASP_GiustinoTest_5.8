# M11 L10: the first aimed move and the first kit mod as hub content (no Blueprint wiring by hand).
# Run headless (editor closed): UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file>"   or   import setup_m11_ranger_content as r; r.run()
# Writes, under /Game/GASNPPTest/Combat (new assets only; KIT_Test_Medium and its weapon are never edited):
#   KIT_Test_Ranger   a copy of KIT_Test_Medium (the K3 reference fighter) whose strong commands, with the reticle on, fire the starter aimed
#                     shot UGSTN_GA_AimedHitscanShot (hitscan along the camera aim): one binding per strong command that requires
#                     Input.Context.PrecisionAim on top of what the plain binding requires, so it wins only while the reticle shows and the
#                     plain strong move stays without it. One mod slot (Mod.Slot.Grip)
#   Mods/MOD_Test_Power   the first kit mod: every move of the kit 1.4x hitbox, 1.5x reach (where a hitbox has a ReachAxis), 1.25x damage,
#                     1.15x knockback, 1.2x slower
# The hub puts the kit on two pads, with and without the mod (test_hub.spawn_kits, build_test_hub_map.MOD_PADS). Every number is a
# placeholder for the playtest: change SHOT/MOD below or the assets, rerun (a rerun rewrites the mod and re-adds missing aimed bindings).
# Output: Saved/Logs/l10/setup_ranger.log, at most 5 printed lines.
import os

import unreal

DIR = '/Game/GASNPPTest/Combat'
MOD_DIR = DIR + '/Mods'
SOURCE_KIT = 'KIT_Test_Medium'
KIT = 'KIT_Test_Ranger'
MOD = 'MOD_Test_Power'
SLOT = 'Mod.Slot.Grip'
STRONG = ('Command.Strong.Neutral', 'Command.Strong.Directional')
# (time scale, hitbox scale, reach scale, damage scale, knockback scale)
MOD_SCALES = (1.2, 1.4, 1.5, 1.25, 1.15)

_lines = []


def log(msg):
    _lines.append(msg)


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    if not unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag):
        log(f'ERROR: tag {name} is not registered (Config/Tags/GSTNCombatTest.ini)')
    return tag


def _tags(container):
    return list(unreal.GameplayTagLibrary.break_gameplay_tag_container(container)) if container else []


def _container(tags):
    return unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array(tags)


def _copy(source, name):
    path = f'{DIR}/{name}'
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.duplicate_asset(f'{DIR}/{source}', path):
            log(f'ERROR: could not copy {source} to {name}')
            return None
        log(f'created {path} (copy of {source})')
    return unreal.load_asset(path)


def _mod_asset():
    path = f'{MOD_DIR}/{MOD}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    if not unreal.EditorAssetLibrary.does_directory_exist(MOD_DIR):
        unreal.EditorAssetLibrary.make_directory(MOD_DIR)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MOD, MOD_DIR, unreal.GSTN_KitModDefinition, unreal.DataAssetFactory())
    log(f'created {path}' if asset else f'ERROR: could not create {path}')
    return asset


def _command(binding):
    return str(binding.get_editor_property('command').get_editor_property('tag_name'))


def _is_shot(binding):
    ability = binding.get_editor_property('ability')
    return ability is not None and 'AimedHitscanShot' in str(ability)


def _aimed_bindings(kit, aim_tag):
    """The kit's bindings plus one aimed binding per strong command that lacks one. Its requirements are the most specific plain binding's of
    that command (kit or primary weapon) plus the reticle context, so it beats that binding only while the reticle shows"""
    bindings = list(kit.get_editor_property('kit_bindings'))
    weapon = kit.get_editor_property('primary_weapon')
    plain = bindings + (list(weapon.get_editor_property('bindings')) if weapon else [])
    added = 0
    for command in STRONG:
        if any(_command(b) == command and _is_shot(b) for b in bindings):
            continue
        base = [b for b in plain if _command(b) == command and not _is_shot(b)]
        required = []
        if base:
            best = max(base, key=lambda b: len(_tags(b.get_editor_property('requirements').get_editor_property('require_tags'))))
            required = _tags(best.get_editor_property('requirements').get_editor_property('require_tags'))
        else:
            log(f'note: {KIT} has no plain {command} binding; the aimed one only requires the reticle')
        req = unreal.GSTN_TagRequirements()
        req.set_editor_property('require_tags', _container(required + [aim_tag]))
        b = unreal.GSTN_AbilityBinding()
        b.set_editor_property('command', _tag(command))
        b.set_editor_property('requirements', req)
        b.set_editor_property('ability', unreal.GSTN_GA_AimedHitscanShot)
        bindings.append(b)
        added += 1
    return bindings, added


def run():
    del _lines[:]
    saves = []
    mod = _mod_asset()
    if mod:
        t, hb, reach, dmg, kb = MOD_SCALES
        changed = unreal.GSTN_KitModContentLibrary.write_all_moves_mod(mod, _tag('Mod.Test.Power'), _tag(SLOT), t, hb, reach, dmg, kb)
        log(f'{MOD}: {"written" if changed else "unchanged"}')
        saves.append(mod)

    kit = _copy(SOURCE_KIT, KIT)
    if kit:
        kit.set_editor_property('display_name', 'TIRATORE')
        kit.set_editor_property('kit_tags', _container([_tag('Kit.Test.Ranger')]))
        unreal.GSTN_KitModContentLibrary.set_kit_mod_slots(kit, [_tag(SLOT)], 2.0)
        bindings, added = _aimed_bindings(kit, _tag('Input.Context.PrecisionAim'))
        kit.set_editor_property('kit_bindings', bindings)
        log(f'{KIT}: {added} aimed binding(s) added, {len(bindings)} kit bindings')
        saves.append(kit)

    for asset in saves:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    log(f'saved {len(saves)} assets: ' + ', '.join(a.get_name() for a in saves))
    path = os.path.join(unreal.Paths.project_saved_dir(), 'Logs', 'l10', 'setup_ranger.log')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'WARNING' in l or 'ERROR' in l][:4] + [_lines[-1][:300]]:
        print(line)
    return [a.get_path_name() for a in saves]


if __name__ == '__main__':
    run()
