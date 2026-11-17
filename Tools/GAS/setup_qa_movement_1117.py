# Movement QA 2026-11-17 (qa-movement-1117): the playtest kit still carried the PRE-preset jump / fall / air stats (JumpVelocity 700 measured
# in game: apex 1.1 character heights instead of 2.3), because the Mario 3D World preset scaled only the three archetype kits. Kit stats are
# absolute and win over the attribute defaults of Config/DefaultGSTNCombatGAS.ini. A stat cannot just be removed: the kit's init effect then
# writes 0 (measured: no jump at all). So the jump / fall / air stats the kit HAS are set to the preset's values (the ini defaults, below).
# Other stats (WalkSpeed, Weight...) are kept. Idempotent; QAM_DRY=1 only lists.
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_qa_movement_1117.py" -unattended -nullrhi
# Writes Saved/QAM/setup_qa_movement_1117.txt, prints one line.
import os

import unreal

FOLDER = '/Game/GASNPPTest/Combat'
KIT = FOLDER + '/KIT_Test_Playtest'
FAST_FALL = 'Command.Movement.FastFall'
# The Mario 3D World preset (Plugins/GSTNCombatGAS/Config/DefaultGSTNCombatGAS.ini)
PRESET_STATS = {'JumpVelocity': 1240.0, 'JumpRiseGravityScale': 0.0, 'JumpCutGravityScale': 4.1, 'FallGravityScale': 4.1, 'MaxFallSpeed': 1800.0,
                'FastFallSpeed': 2600.0, 'FastFallGravityScale': 6.0, 'MinJumpHoldFrames': 2.0, 'MaxJumpHoldFrames': 11.0, 'AirJumpVelocity': 900.0,
                'AirControl': 0.85}
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'QAM', 'setup_qa_movement_1117.txt')
_lines = []


def log(msg):
    _lines.append(msg)


def _name(v):
    return str(v.get_editor_property('stat').get_editor_property('tag_name')).rsplit('.', 1)[-1]


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _bindings(kit):
    out = []
    for b in kit.get_editor_property('kit_bindings'):
        try:
            out.append(str(b.get_editor_property('command').get_editor_property('tag_name')))
        except Exception:
            out.append('?')
    return out


def run():
    kit = unreal.load_asset(KIT)
    if not kit:
        log(f'{KIT} missing')
        return
    stats = kit.get_editor_property('stats')
    values = list(stats.get_editor_property('values'))
    log('stats before: ' + ', '.join(f'{_name(v)}={v.get_editor_property("value")}' for v in values))
    log('kit bindings: ' + ', '.join(_bindings(kit)))
    # Fast fall: the playtest kit has no Command.Movement.FastFall binding (measured: the crouch tap after the apex does nothing). Copied from
    # the first kit of the folder that has one
    cmds = _bindings(kit)
    if FAST_FALL not in cmds:
        src = None
        for path in unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=False):
            other = unreal.load_asset(path) if '/KIT_' in path else None
            if other and other != kit and FAST_FALL in _bindings(other):
                src = (path, [b for b in other.get_editor_property('kit_bindings') if FAST_FALL in str(b.get_editor_property('command').get_editor_property('tag_name'))])
                break
        if src and os.environ.get('QAM_DRY') != '1':
            kit.set_editor_property('kit_bindings', list(kit.get_editor_property('kit_bindings')) + src[1])
            log(f'fast fall binding copied from {src[0].rsplit("/", 1)[-1]} ({len(src[1])})')
        else:
            log('fast fall binding: ' + (f'would copy from {src[0]}' if src else 'NO kit has one'))
    changed = []
    for v in values:
        name = _name(v)
        if name in PRESET_STATS and abs(v.get_editor_property('value') - PRESET_STATS[name]) > 1e-4:
            changed.append(f'{name} {v.get_editor_property("value")} -> {PRESET_STATS[name]}')
            v.set_editor_property('value', PRESET_STATS[name])
    # JumpVelocity must be there (a first version of this script removed it, and a missing stat is written as 0)
    if 'JumpVelocity' not in {_name(v) for v in values}:
        v = unreal.GSTN_StatValue()
        v.set_editor_property('stat', _tag('Combat.SetByCaller.Stat.JumpVelocity'))
        v.set_editor_property('value', PRESET_STATS['JumpVelocity'])
        values.append(v)
        changed.append(f'JumpVelocity added = {PRESET_STATS["JumpVelocity"]}')
    if os.environ.get('QAM_DRY') != '1':
        stats.set_editor_property('values', values)
        kit.set_editor_property('stats', stats)
        unreal.EditorAssetLibrary.save_loaded_asset(kit, False)
        log('set to the preset: ' + (', '.join(changed) or 'nothing to change') + '; saved')
    else:
        log('dry run, would set: ' + (', '.join(changed) or 'nothing'))


try:
    run()
except Exception as e:  # noqa: BLE001
    log(f'ERROR {e!r}')
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w') as fh:
    fh.write('\n'.join(_lines) + '\n')
print('setup_qa_movement_1117: ' + (_lines[-1] if _lines else 'no output'))
