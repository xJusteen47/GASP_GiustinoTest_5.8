# Air crouch content (owner 2026-11-16: "air crouched state ... un set di animazione apposita"), by data only. Idempotent.
# Needs a build with FGSTN_AirCrouchKitExtension and UGSTN_AirCrouchContentLibrary (handover docs/notes/handovers/air-crouch-1.md).
#
# Run with the editor CLOSED (headless), AFTER setup_reaction_anims.py has made DA_ReactionAnims:
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_air_crouch.py" -unattended -nullrhi
# It prints at most 5 lines; the full log is Saved/AirCrouch/setup_air_crouch.log.
#
# What it does:
#  1. PLACEHOLDER clip /Game/GSTN/Animation/AirCrouch/AS_AirCrouch_Loop_PLACEHOLDER: a copy of the sample's crouch idle (found by name in
#     the asset registry, SOURCE_HINTS). Only made when missing: replace it by a real pose from the animation pipeline (rules 10 and 13),
#     never by editing this script. An enter clip (AS_AirCrouch_Enter) is optional: when it exists it plays first, then the loop.
#  2. The AirCrouch rule in DA_ReactionAnims (UGSTN_ReactionAnimSet): while Combat.State.AirCrouch, the enter clip (if any) then the loop,
#     below every hit reaction. setup_reaction_anims.py keeps it when it rewrites its own rules.
#  3. Every KIT_* under /Game/GSTN gets an Air Crouch kit extension with its defaults (one already there is left as tuned).
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_defence_link_content as dl  # noqa: E402  (the shared asset helpers: _data_asset, _tag)

FOLDER = '/Game/GSTN/Animation/AirCrouch'
LOOP = 'AS_AirCrouch_Loop_PLACEHOLDER'
ENTER = 'AS_AirCrouch_Enter'
REACTION_SET = '/Game/GSTN/Combat/Reactions/DA_ReactionAnims'
KIT_ROOT = '/Game/GSTN'
# The placeholder's source: the first sequence whose name has all the words of a hint, in hint order (GASP sample names)
SOURCE_HINTS = [('crouch', 'idle', 'loop'), ('crouch', 'idle'), ('crouch',)]

# Rule (60 fps frames). Owner rule 2026-11-14: the first frame of a new state reads as a clear change, so blend in 1.
# Priority 5: under every hit reaction (HitStand is 10), so a hit in the air always shows the hit.
RULE_NAME = 'AirCrouch'
RULE_PRIORITY = 5
BLEND_IN, CHAIN_BLEND, BLEND_OUT = 1, 2, 3
BLOCKED = ['Combat.State.Hitstun', 'Combat.State.Tumble', 'Combat.State.Splat']

_lines = []


def log(msg):
    _lines.append(msg)
    unreal.log(f'[setup_air_crouch] {msg}')


def _find_source():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence')], package_paths=['/Game'], recursive_paths=True)
    names = sorted((str(d.package_name) for d in reg.get_assets(flt)), key=len)
    names = [n for n in names if not n.startswith(FOLDER)]
    for hint in SOURCE_HINTS:
        for n in names:
            low = n.rsplit('/', 1)[-1].lower()
            if all(w in low for w in hint):
                return n
    return None


def placeholder():
    path = f'{FOLDER}/{LOOP}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log(f'{LOOP}: kept')
        return unreal.load_asset(path)
    src = _find_source()
    if not src:
        log('ERROR: no crouch sequence found for the placeholder')
        return None
    asset = unreal.EditorAssetLibrary.duplicate_asset(src, path)
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        log(f'{LOOP}: copied from {src}')
    return asset


def rule(loop):
    rset = unreal.load_asset(REACTION_SET)
    if not rset:
        log('ERROR: DA_ReactionAnims missing, run setup_reaction_anims.py first')
        return
    enter = unreal.load_asset(f'{FOLDER}/{ENTER}') if unreal.EditorAssetLibrary.does_asset_exist(f'{FOLDER}/{ENTER}') else None
    r = unreal.GSTN_ReactionAnimRule()
    r.set_editor_property('name', RULE_NAME)
    r.set_editor_property('required_tags', unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([dl._tag('Combat.State.AirCrouch')]))
    r.set_editor_property('blocked_tags', unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([dl._tag(t) for t in BLOCKED]))
    r.set_editor_property('priority', RULE_PRIORITY)
    r.set_editor_property('animation', enter or loop)
    if enter:
        r.set_editor_property('then_animation', loop)
    r.set_editor_property('end_behaviour', unreal.GSTN_ReactionAnimEnd.LOOP)
    r.set_editor_property('blend_in_frames', BLEND_IN)
    r.set_editor_property('chain_blend_frames', CHAIN_BLEND)
    r.set_editor_property('blend_out_frames', BLEND_OUT)
    rules = [x for x in rset.get_editor_property('rules') if str(x.get_editor_property('name')) != RULE_NAME]
    old = [x.export_text() for x in rset.get_editor_property('rules')]
    rules.append(r)
    if old != [x.export_text() for x in rules]:
        rset.set_editor_property('rules', rules)
        unreal.EditorAssetLibrary.save_loaded_asset(rset, False)
        log(f'DA_ReactionAnims: AirCrouch rule written ({"enter + loop" if enter else "loop only"})')
    else:
        log('DA_ReactionAnims: AirCrouch rule unchanged')


def kits():
    changed = []
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in reg.get_assets_by_path(KIT_ROOT, recursive=True):
        if not str(data.asset_name).startswith('KIT_'):
            continue
        kit = data.get_asset()
        if isinstance(kit, unreal.GSTN_PrimaryKitDefinition) and unreal.GSTN_AirCrouchContentLibrary.ensure_kit_air_crouch(kit):
            unreal.EditorAssetLibrary.save_loaded_asset(kit, False)
            changed.append(str(data.asset_name))
    log(f'kits given an air crouch: {", ".join(changed) if changed else "none changed"}')


def run():
    del _lines[:]
    loop = placeholder()
    if loop:
        rule(loop)
    kits()
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'AirCrouch')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_air_crouch.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    return _lines[-5:]


if __name__ == '__main__':
    for line in run():
        print(line)
