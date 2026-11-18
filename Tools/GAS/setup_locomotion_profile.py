# Creates the locomotion animation profile (additive lean, docs/notes/additive-lean-1.md) and plugs it into the character's anim set collection,
# so every kit gets it (the collection belongs to the character, not to a kit). Idempotent; changes only data assets, no Blueprint.
#
# Run headless (editor closed; needs GSTNCombatNP built with UGSTN_LocomotionAnimSet):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_locomotion_profile.py" -unattended -nullrhi
# Prints at most 5 lines; full log in the editor log ([setup_locomotion_profile]).
#
# Steps (assets in /Game/Blueprints/Animation):
#  1. DA_AnimSet_Locomotion_Default (GSTN_LocomotionAnimSet, SetTag Anim.Set.Locomotion, no layer class): the lean at its C++ defaults.
#  2. One rule (always, priority 0) in DA_AnimSetCollection_SandboxCharacter.
# Without this script the lean still runs, at the same built-in defaults; the asset is where the values get tuned.
import os, sys
import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_wall_anim_layer as wall  # noqa: E402

ANIM_DIR = wall.ANIM_DIR
SET_NAME = 'DA_AnimSet_Locomotion_Default'
SET_TAG = 'Anim.Set.Locomotion'

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
lines = []
errors = []


def log(msg):
    lines.append(msg)
    unreal.log(f'[setup_locomotion_profile] {msg}')


def fail(msg):
    errors.append(msg)
    log(f'ERROR {msg}')


def ensure_profile():
    path = f'{ANIM_DIR}/{SET_NAME}'
    profile = wall.load(path)
    if not profile:
        profile = asset_tools.create_asset(SET_NAME, ANIM_DIR, unreal.GSTN_LocomotionAnimSet, unreal.DataAssetFactory())
        if not profile:
            fail(f'cannot create {path}')
            return None
        log(f'created {path}')
    if str(profile.get_editor_property('set_tag').export_text()).find(SET_TAG) < 0:
        profile.set_editor_property('set_tag', wall.tag(SET_TAG))
        wall.save(profile)
        log(f'{SET_NAME}: set tag {SET_TAG}')
    return profile


def run():
    if not hasattr(unreal, 'GSTN_LocomotionAnimSet'):
        fail('GSTN_LocomotionAnimSet missing: build GSTNCombatNP first')
        return False
    profile = ensure_profile()
    if profile:
        wall.ensure_collection([(profile, None, 0)])
        log(f'{SET_NAME} in {wall.COLLECTION_NAME}')
    return not errors


if __name__ == '__main__':
    try:
        ok = run()
    except Exception as e:  # noqa: BLE001
        fail(str(e))
        ok = False
    tail = [l for l in lines if l.startswith('ERROR')][-4:] or lines[-4:]
    for line in tail:
        print(line)
    print(f'ESITO: {"OK" if ok else "FAIL"} locomotion profile ({len(errors)} errors)')
