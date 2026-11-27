# AERIAL CHARGE (owner 2026-11-25, decisions 1C 2A 3A): gives every aerial of every test kit a Charge Hold track (FGSTN_TimelineTrack_ChargeHold,
# C++ on claude/project-thread-uyhp3p). Hold the attack button and the aerial stops in its anticipation pose a third into its startup, the
# character keeps falling and drifting with the stick, the release plays the rest of the startup (Smash Ultimate's smash charge, applied to aerials).
# Data only: no Blueprint graph, no input, no project setting.
#
# DRY RUN by default: it lists what it would write and writes nothing. run(apply=True) writes.
#
# What it finds: every binding (kit bindings and weapon bindings of every asset under /Game/GSTN) whose requirements need Input.Context.Airborne and
# whose command is under Command.Attack or Command.Strong; the binding's ability class; its timeline (the TimelineAsset when set, else the inline
# Timeline of the ability's class default object). A timeline shared by several bindings is written once.
#
# What it writes into each timeline, nothing else:
#   - Charge Hold on HoldFrame = round(first active frame x HOLD_FRACTION), at least 1 and at most first active - MIN_AFTER_RELEASE (Smash Ultimate
#     stops its smash attacks early in the startup, in the anticipation pose, so the release still shows a wind-up. The fraction is a starting
#     point to tune by eye on the capsule, UNVERIFIED against Ultimate's per-move charge frames). Button = the command's family: Combat.Attack /
#     Input.Button.Attack, or Combat.Strong / Input.Button.Strong. Max hold MAX_HOLD_FRAMES (60 = about 1 s, Ultimate's full charge).
#     Damage scale FULL_CHARGE_SCALE (1C: per move, 1 = off, the default; Ultimate is 1.4). Direction locked (2A). Pose paused (no loop section).
#   - The angle tolerance at the confirm (owner: "la tolleranza della leggera angolazione ... alla conferma"): a timeline with no steering cone
#     (MaxSteerDegrees 0) gets STEER_DEGREES on the move stick, steering from the frame after the hold to the first active frame. A timeline that
#     already steers is left as tuned (its yaw holds during the hold anyway).
# An asset saved by hand is still written (the change only ADDS a track, the hand edits stay), but it is NOT recorded in the setup ledger, so the kit
# scripts keep treating it as hand-edited; a script-owned one stays script-owned. A rerun replaces the Charge Hold track (idempotent).
#
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_aerial_charge_hold as s; s.run()            (dry run)
#            s.run(apply=True)
# Env:       CHARGE_HOLD_FRACTION (0.33), CHARGE_MAX_HOLD_FRAMES (60), CHARGE_FULL_SCALE (1.0), CHARGE_STEER_DEGREES (15), CHARGE_MIN_AFTER_RELEASE (3)
# Output:    Saved/AerialCharge/setup_aerial_charge_hold.txt, at most 5 printed lines.
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_guard  # noqa: E402

ROOT = '/Game/GSTN'
AIRBORNE = 'Input.Context.Airborne'
FAMILIES = {'Command.Attack': ('Combat.Attack', 'Input.Button.Attack'), 'Command.Strong': ('Combat.Strong', 'Input.Button.Strong')}
HOLD_FRACTION = float(os.environ.get('CHARGE_HOLD_FRACTION', '0.33'))
MAX_HOLD_FRAMES = int(os.environ.get('CHARGE_MAX_HOLD_FRAMES', '60'))
FULL_CHARGE_SCALE = float(os.environ.get('CHARGE_FULL_SCALE', '1.0'))
STEER_DEGREES = float(os.environ.get('CHARGE_STEER_DEGREES', '15'))
MIN_AFTER_RELEASE = int(os.environ.get('CHARGE_MIN_AFTER_RELEASE', '3'))
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'AerialCharge', 'setup_aerial_charge_hold.txt')
# Timelines never written: the approved, locked dair v9 (anim-approvate.md; coordinator 2026-11-27: no variant for now). CHARGE_SKIP replaces the list
SKIP = {n.strip() for n in os.environ.get('CHARGE_SKIP', 'TL_Overlord_DAirV9').split(',') if n.strip()}
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[aerial_charge] {msg}')


def _tag(name):
    # 5.9 Python: GameplayTag has neither request_gameplay_tag nor a tag_name keyword; import_text is what the other Tools scripts use
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _tag_names(container):
    try:
        return [str(t.get_editor_property('tag_name')) for t in container.get_editor_property('gameplay_tags')]
    except Exception:
        return [s.strip() for s in container.export_text().strip('()').replace('GameplayTags=', '').replace('(TagName="', '').replace('")', '').split(',') if s.strip()]


def _family(command):
    for prefix in FAMILIES:
        if command == prefix or command.startswith(prefix + '.'):
            return prefix
    return None


def _aerial_bindings():
    """(owner asset name, command, family, ability class) of every airborne attack/strong binding under ROOT"""
    found = []
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in reg.get_assets_by_path(ROOT, recursive=True):
        name = str(data.asset_name)
        if not (name.startswith('KIT_') or name.startswith('WPN_')):
            continue
        asset = data.get_asset()
        for prop in ('kit_bindings', 'bindings'):
            try:
                bindings = list(asset.get_editor_property(prop))
            except Exception:
                continue
            for b in bindings:
                command = str(b.get_editor_property('command').get_editor_property('tag_name'))
                family = _family(command)
                require = _tag_names(b.get_editor_property('requirements').get_editor_property('require_tags'))
                ability = b.get_editor_property('ability')
                if family and AIRBORNE in require and ability:
                    found.append((name, command, family, ability))
    return found


def _first_active(data):
    """The first active frame (C++ FGSTN_StartupSteer::GetSteerEndFrame), None for a move without an attack hitbox"""
    first = unreal.GSTN_TimelineUtils.get_first_active_frame(data)
    return first if first > 0 else None


def _hold_frame(first_active):
    frame = int(round(first_active * HOLD_FRACTION))
    return max(1, min(frame, first_active - MIN_AFTER_RELEASE))


def _apply(data, family):
    """The timeline data with the Charge Hold (and the steering cone when it had none); None when the move has no startup to hold in"""
    first = _first_active(data)
    if not first or first - MIN_AFTER_RELEASE < 1:
        return None, f'no startup to hold in (first active {first})'
    hold = _hold_frame(first)
    layout, button = FAMILIES[family]
    out = unreal.GSTN_TimelineUtils.set_charge_hold_track(data, hold, _tag(button), layout, MAX_HOLD_FRAMES, FULL_CHARGE_SCALE, True, 'None')
    note = f'hold frame {hold} of startup {first}, max {MAX_HOLD_FRAMES}, damage x{FULL_CHARGE_SCALE:g} at full'
    if out.get_editor_property('max_steer_degrees') <= 0.0:
        steer = out.get_editor_property('steer')
        steer.set_editor_property('source', unreal.GSTN_SteerSource.MOVE_STICK)
        steer.set_editor_property('start_frame', hold + 1)
        steer.set_editor_property('end_frame', -1)
        out.set_editor_property('steer', steer)
        out.set_editor_property('max_steer_degrees', STEER_DEGREES)
        note += f', steering cone {STEER_DEGREES:g} deg from frame {hold + 1}'
    return out, note


def run(apply=False, force=False):
    del _lines[:]
    ok = False
    try:
        guard = setup_guard.Guard.for_unreal('setup_aerial_charge_hold', force=force, log=log)
        # Headless (-run=pythonscript, Save-Assets.ps1) the registry is not scanned yet and finds no kit: wait for the scan (instant in an editor)
        unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
        bindings = _aerial_bindings()
        log(f'{len(bindings)} airborne attack/strong bindings: ' + '; '.join(f'{o} {c}' for o, c, _, _ in bindings))
        done = set()
        written = 0
        planned = 0
        for owner, command, family, cls in bindings:
            cdo = unreal.get_default_object(cls)
            # The move's TimelineAsset when it has one (the kit scripts put the frame data there; the inline timeline is then empty), as
            # setup_knockback_smash.py and setup_overlord_hop0.py resolve it
            asset = unreal.GSTN_TestKitContentLibrary.get_move_timeline_asset(cls)
            key = asset.get_path_name() if asset else cls.get_path_name()
            if key in done:
                continue
            done.add(key)
            if key.rsplit('/', 1)[-1].split('.')[0] in SKIP:
                log(f'SKIP {key.rsplit("/", 1)[-1]} ({owner} {command}): locked, in CHARGE_SKIP')
                continue
            data =asset.get_editor_property('data') if asset else cdo.get_editor_property('timeline')
            new, note = _apply(data, family)
            where = key.rsplit('/', 1)[-1]
            if new is None:
                log(f'SKIP {where} ({owner} {command}): {note}')
                continue
            log(f'{"WRITE" if apply else "would write"} {where} ({owner} {command}): {note}')
            planned += 1
            if not apply:
                continue
            if asset:
                path = asset.get_path_name().split('.')[0]
                guard.precheck([path])
                owned = guard.state(path) in setup_guard.WRITABLE
                asset.set_editor_property('data', new)
                unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
                if owned:
                    guard.record([path])
            else:
                # Inline timeline on the ability Blueprint: the class default object is saved with its Blueprint (a shared Blueprint: the owner
                # was told before this ran)
                cdo.set_editor_property('timeline', new)
                bp_path = cls.get_path_name().split('.')[0]
                unreal.EditorAssetLibrary.save_asset(bp_path, False)
            written += 1
        log(f'{"wrote" if apply else "dry run, would write"} {written if apply else planned} of {len(done)} timeline(s)')
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in _lines[-5:]:
        print(line[:300])
    return ok


if __name__ == '__main__':
    # GSTN_SAVE=1 is what Tools/CI/Save-Assets.ps1 sets (the agents' only save route)
    run(apply=os.environ.get('CHARGE_APPLY', '') == '1' or os.environ.get('GSTN_SAVE', '') == '1')
