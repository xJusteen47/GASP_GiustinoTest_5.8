# Playtest fixes 2026-11-17 (branch claude/playtest-fixes-1117). Data only, idempotent, saves .uasset only (never a map).
#   DA_Locomotion_Default  Mario 3D World preset (handoffs/archivio/2026-11/preset-mario3dw.md): stage speeds / accelerations / turning / brakes / frames /
#                          air control scale, Air MinAirSpeed 470; Analog Walk: no second dead zone, no minimum speed, no start boost
#                          (InnerDeadzone 0, MinSpeedFraction 0, StartAccelerationMultiplier 1), air turn 3600 deg/s, facing 0.02/0.05 s;
#                          run (stage 1) acceleration ramp 6 frames from 30 % (run-start-skid-import-piano R1b; sprints: no ramp).
#   TL_Overlord_Dropkick   Self Landing Bounce track (ground-bounce-1: the attacker hops on landing, like Big Band), mode Height 25 cm.
# The ini part of the preset (jump / gravity / air control defaults) is in Plugins/GSTNCombatGAS/Config/DefaultGSTNCombatGAS.ini, the kits'
# part in Tools/GAS/setup_test_kit_archetypes.py. Jump squat is NOT changed (owner picks 0 or 3).
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_playtest_fixes_1117.py" -unattended -nullrhi
# Writes Saved/PlaytestFixes/setup_playtest_fixes_1117.txt, prints at most 5 lines.
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_guard  # noqa: E402

PROFILE = '/Game/GASNPPTest/Combat/DA_Locomotion_Default'
DROPKICK = '/Game/GASNPPTest/Combat/Overlord/TL_Overlord_Dropkick'
PLAYTEST_KIT = '/Game/GASNPPTest/Combat/KIT_Test_Playtest'
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'PlaytestFixes', 'setup_playtest_fixes_1117.txt')

# (max_speed, acceleration, turning_strength, release_brake, frames_to_next_stage, air_control_scale, accel_ramp_frames, ramp_start_fraction)
STAGES = [
    (470.0, 2000.0, 16.0, 3200.0, 40, 1.0, 6, 0.3),
    (610.0, 1400.0, 10.0, 1900.0, 94, 0.9, 0, 1.0),
    (850.0, 1200.0, 6.0, 1700.0, 1, 0.75, 0, 1.0),
]
ANALOG = {'inner_deadzone': 0.0, 'min_speed_fraction': 0.0, 'start_acceleration_multiplier': 1.0,
          'air_turn_rate_deg_per_sec': 3600.0, 'start_facing_time': 0.02, 'run_facing_time': 0.05}
MIN_AIR_SPEED = 470.0
SELF_BOUNCE_HEIGHT = 25.0
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[playtest_fixes_1117] {msg}')


def _profile():
    asset = unreal.load_asset(PROFILE)
    if not asset:
        raise RuntimeError(f'{PROFILE} missing')
    run = asset.get_editor_property('run_stages')
    stages = list(run.get_editor_property('stages'))
    if len(stages) != len(STAGES):
        raise RuntimeError(f'{PROFILE} has {len(stages)} stages, expected {len(STAGES)}: not touched')
    for stage, (speed, accel, turn, brake, frames, air, ramp, ramp_from) in zip(stages, STAGES):
        stage.set_editor_property('max_speed', speed)
        stage.set_editor_property('acceleration', accel)
        stage.set_editor_property('turning_strength', turn)
        stage.set_editor_property('release_brake', brake)
        stage.set_editor_property('frames_to_next_stage', frames)
        stage.set_editor_property('air_control_scale', air)
        stage.set_editor_property('accel_ramp_frames', ramp)
        stage.set_editor_property('ramp_start_fraction', ramp_from)
    run.set_editor_property('stages', stages)
    air = run.get_editor_property('air')
    air.set_editor_property('min_air_speed', MIN_AIR_SPEED)
    run.set_editor_property('air', air)
    asset.set_editor_property('run_stages', run)
    analog = asset.get_editor_property('analog_walk')
    for key, value in ANALOG.items():
        analog.set_editor_property(key, value)
    asset.set_editor_property('analog_walk', analog)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    log('DA_Locomotion_Default: Mario 3D World stages (470/610/850), run ramp 6 f from 30 %, analog walk without dead zone/step; saved')


def _playtest_kit_profile():
    kit = unreal.load_asset(PLAYTEST_KIT)
    if not kit:
        return log('KIT_Test_Playtest missing')
    text = kit.get_editor_property('extensions').export_text()
    log('KIT_Test_Playtest locomotion profile: ' + ('DA_Locomotion_Default' if 'DA_Locomotion_Default' in text else 'OTHER (check by hand)'))


def _dropkick():
    asset = unreal.load_asset(DROPKICK)
    if not asset:
        return log(f'{DROPKICK} missing: dropkick track not added')
    # The track is ADDED to the asset as it is (hand edits kept), so a hand-edited timeline is written too, after the guard's backup
    guard = setup_guard.Guard.for_unreal('setup_playtest_fixes_1117', force=['TL_Overlord_Dropkick'], log=log)
    guard.precheck([DROPKICK])
    if not guard.may_write(DROPKICK):
        return log('TL_Overlord_Dropkick was edited by hand: Self Landing Bounce NOT added (add it in the timeline editor)')
    data = asset.get_editor_property('data')
    data = unreal.GSTN_DefenceContentLibrary.set_self_landing_bounce_track(data, unreal.GSTN_SelfLandingBounce.HEIGHT, SELF_BOUNCE_HEIGHT)
    asset.set_editor_property('data', data)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    guard.record([DROPKICK])
    log(f'TL_Overlord_Dropkick: Self Landing Bounce {SELF_BOUNCE_HEIGHT:.0f} cm, {len(data.get_editor_property("tracks"))} tracks; saved')


OVERLORD_WEAPON = '/Game/GASNPPTest/Combat/WPN_Test_Overlord'
# Weapons whose Overlord move binding (ability GA_Overlord_*) the main Overlord kit lacks (owner 2026-11-17: "no down air, no dropkick")
OVERLORD_MOVE_SOURCES = ['/Game/GASNPPTest/Combat/WPN_Test_OverlordDAirV9', '/Game/GASNPPTest/Combat/WPN_Test_OverlordDropkick']


def _describe(b):
    return f"{b.get_editor_property('command').get_editor_property('tag_name')}->{b.get_editor_property('ability').get_name() if b.get_editor_property('ability') else 'None'}"


def _overlord_kit():
    weapon = unreal.load_asset(OVERLORD_WEAPON)
    if not weapon:
        return log(f'{OVERLORD_WEAPON} missing')
    bindings = list(weapon.get_editor_property('bindings'))
    added = []
    for src_path in OVERLORD_MOVE_SOURCES:
        src = unreal.load_asset(src_path)
        if not src:
            log(f'{src_path} missing')
            continue
        for b in src.get_editor_property('bindings'):
            ability = b.get_editor_property('ability')
            if not ability or 'GA_Overlord_' not in ability.get_name():
                continue
            # A rerun finds the same ability already bound and replaces it with the source's binding
            # and a generic move on the same command and context (copied from WPN_Test_Hits) would win, being declared first: it goes
            same = lambda x: (x.get_editor_property('ability') == ability or (
                str(x.get_editor_property('command').get_editor_property('tag_name')) == str(b.get_editor_property('command').get_editor_property('tag_name'))
                and x.get_editor_property('requirements').export_text() == b.get_editor_property('requirements').export_text()))
            for x in bindings:
                if same(x) and x.get_editor_property('ability') != ability:
                    log(f'WPN_Test_Overlord: replaced {_describe(x)}')
            bindings = [x for x in bindings if not same(x)] + [b]
            added.append(_describe(b))
    weapon.set_editor_property('bindings', bindings)
    unreal.EditorAssetLibrary.save_loaded_asset(weapon, only_if_is_dirty=False)
    log('WPN_Test_Overlord: + ' + ', '.join(added) + f'; {len(bindings)} bindings: ' + '; '.join(_describe(b) for b in bindings))


def run():
    del _lines[:]
    for step in (_profile, _playtest_kit_profile, _dropkick, _overlord_kit):
        try:
            step()
        except Exception as e:
            log(f'ERROR in {step.__name__}: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in _lines[-5:]:
        print(line[:300])


run()
