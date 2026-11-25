# grab-ganon (owner 2026-11-25, thread "Perfezionare grab e lancio dei nemici"): KIT_Test_Playtest grabs, throws and holds with GANONDORF's numbers
# from Super Smash Bros. Ultimate. Sources and conversion: docs/notes/grab-ganon-dati.md. Rerun-safe (a rerun with the same numbers saves nothing).
#
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<proj>/Tools/GAS/setup_grab_ganon_playtest.py"
# In the editor: import setup_grab_ganon_playtest as g; g.run()
#
# Writes (new Blueprint children under /Game/GSTN/Characters/Overlord/Moves/Playtest, like setup_knockback_smash.py's GA_Smash_*):
#   GA_Ganon_Playtest_Throw{Forward,Up,Down,Neutral,CrouchForward}   children of the common throws (UGSTN_GA_LinkThrow*), frames, knockback, turn to stick
#   GA_Ganon_Playtest_Grab   child of the kit's grab ability (when that is a native class): Ganondorf's standing grab frames
# and on KIT_Test_Playtest's link extension: the throw set (every entry above, no back throw) and the hold rule (Smash: 90 + 1.7 x %, min 19, no mash).
# The grab spin needs no asset: its rows come from DefaultGSTNLink.ini (FallbackThrows.Spin*), on every grabbing kit (shared system).
# setup_knockback_smash.py leaves GA_Ganon_* alone; setup_test_kit_archetypes.py does not touch this kit. No montage (capsule phase).
# Out: Saved/Logs/grab-ganon/setup.log
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gstn_content as gc  # noqa: E402

KIT = gc.path('KIT_Test_Playtest')
CHILD_DIR = gc.move_folder('Playtest')
LINK = unreal.GSTN_LinkContentLibrary
UNIT_SCALE = 22.5  # Smash knockback units -> cm/s (feel-f2, UGSTN_KnockbackModel_PercentScaling UnitScale)

# Ganondorf, Smash Ultimate. Frames from Ultimate Frame Data (ultimateframedata.com/ganondorf): (release frame, total frames, damage = hit + throw);
# BKB / KBG / angle of the throw hitbox from SmashWiki (ssbwiki.com/Ganondorf_(SSBU)/<Throw>). Frames 1:1 (Smash 60 Hz, our fixed tick 62.5 Hz).
# Angle: our elevation from the thrower's facing.
#                total release damage  BKB  KBG  angle   Smash source
GANON = {
    'F': (39, 13, 13.0, 68,  70,  43.0),  # F throw: 5 % punch + 8 % throw, release 13
    'U': (43, 13, 13.0, 70, 105,  90.0),  # U throw: 10 + 3 %
    'D': (39, 23,  7.0, 70,  50,  65.0),  # D throw: 7 %, release 23
    # B throw (5 + 5 %, release 14, total 49, BKB 30 KBG 130) not used: owner 2026-11-25 "il back non metterlo"
}

# Owner 2026-11-25, answer 1 (Notation mode): F. light = Ganon's forward throw, Cr. light = his UP throw, Cr.F. light = his DOWN throw, no back throw.
# N. was not named: it keeps the up throw of the first proposal (1A). The Cr. slot is Command.Link.Throw.Down, so the classic modes' down throw is the
# up throw too (the playtest kit runs Notation). Each entry: our throw slot -> (parent common throw, Ganon throw)
THROWS = {
    'Forward':       ('GSTN_GA_LinkThrowForward',       'F'),
    'Up':            ('GSTN_GA_LinkThrowUp',            'U'),
    'Down':          ('GSTN_GA_LinkThrowDown',          'U'),  # Cr.
    'Neutral':       ('GSTN_GA_LinkThrowNeutral',       'U'),  # N.
    'CrouchForward': ('GSTN_GA_LinkThrowCrouchForward', 'D'),  # Cr.F.
}
COMMANDS = {'Neutral': 'Command.Link.Throw.Neutral', 'CrouchForward': 'Command.Link.Throw.CrouchForward'}

# Owner 2026-11-25, answer 6: every throw turns toward the stick before the release, not with the instant snap: 180 degrees in TURN_FRAMES
# (Ganon's earliest release is 13, so the turn is over before it). Neutral stick: the facing stays, i.e. the held enemy's direction (the
# recommended option of the open point; the other was "the last stick movement")
TURN_FRAMES = 8
TURN_DEG_PER_S = 180.0 / (TURN_FRAMES / 62.5)

# Standing grab: startup 8, active 8-10, total 38 (UFD). Dash grab 11-13 / 46 and pivot grab 12-14 / 41 have no move of their own here yet
GRAB = {'total': 38, 'first_actionable': 38, 'active_start': 8, 'active_end': 11}

# Grab hold (SmashWiki "Grab", Grab time, Brawl onward): 90 + floor(1.7 x victim %) frames, never under 19. Owner 2026-11-25, answer 2: NO mash
# ("niente mash, lo scaling con percentuale ha senso"), so an input takes nothing off. MaxFrames 600 (Smash has no cap; 300 % = 600 frames)
MASH = {'base': 90, 'per_percent': 1.7, 'min': 19, 'max': 600, 'reduce_per_press': 0}

_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[grab_ganon] {msg}')


def _child(name, parent):
    path = f'{CHILD_DIR}/{name}'
    bp = unreal.load_asset(path)
    if not bp:
        if not unreal.EditorAssetLibrary.does_directory_exist(CHILD_DIR):
            unreal.EditorAssetLibrary.make_directory(CHILD_DIR)
        factory = unreal.GameplayAbilitiesBlueprintFactory() if hasattr(unreal, 'GameplayAbilitiesBlueprintFactory') else unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        asset_class = unreal.GameplayAbilityBlueprint if hasattr(unreal, 'GameplayAbilityBlueprint') else unreal.Blueprint
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, CHILD_DIR, asset_class, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f'created {path} (child of {parent.get_name()})')
    return bp, unreal.EditorAssetLibrary.load_blueprint_class(path)


def _tag(name):
    # As setup_test_kit_archetypes.py: the tag's text form (GameplayTag has no constructor by name in Python)
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def write_throws(kit, saves):
    classes = {}
    for key, (parent, ganon) in THROWS.items():
        total, release, damage, bkb, kbg, angle = GANON[ganon]
        bp, cls = _child(f'GA_Ganon_Playtest_Throw{key}', getattr(unreal, parent).static_class())
        changed = LINK.write_throw_move(cls, total, total, release, damage, bkb * UNIT_SCALE, angle, None, 1.0)
        changed |= LINK.write_throw_knockback(cls, bkb * UNIT_SCALE, kbg)
        changed |= LINK.write_throw_stick_turn(cls, TURN_DEG_PER_S, release)
        log(f'{key} = Ganon {ganon}: total {total}, release {release}, {damage} %, BKB {bkb} ({bkb * UNIT_SCALE:.0f} cm/s), KBG {kbg}, {angle} deg, '
            f'turn to stick {TURN_DEG_PER_S:.0f} deg/s -> {LINK.describe_throw_knockback(cls)} changed={changed}')
        if changed:
            saves.append(bp)
        classes[key] = cls
    # No back throw (owner 2026-11-25): the slot stays empty, so only the BackThrow ergonomics would bind the common one
    changed = LINK.set_kit_throws(kit, classes['Forward'], None, classes['Up'], classes['Down'])
    for key, command in COMMANDS.items():
        changed |= LINK.set_kit_throw(kit, _tag(command), classes[key])
    log(f'kit throw set changed={changed}')
    return changed


def write_grab(kit, saves):
    """The kit's grab rows play GA_Ganon_Playtest_Grab with Ganondorf's standing grab frames"""
    changed = False
    bindings = list(kit.get_editor_property('kit_bindings'))
    grab_rows = [b for b in bindings if str(b.get_editor_property('command').get_editor_property('tag_name')) == 'Command.Link.Grab']
    if not grab_rows or not grab_rows[0].get_editor_property('ability'):
        log('WARNING: KIT_Test_Playtest binds no Command.Link.Grab ability: grab frames not written')
        return False
    current = grab_rows[0].get_editor_property('ability')
    if current.get_name().startswith('GA_Ganon_'):
        cls = current
        bp = unreal.load_asset(cls.get_path_name().rsplit('.', 1)[0])
    else:
        # A native grab (the harness UGSTN_GA_LinkTestGrab) or another Blueprint: a child keeps the original untouched
        bp, cls = _child('GA_Ganon_Playtest_Grab', current)
        for b in grab_rows:
            b.set_editor_property('ability', cls)
        kit.set_editor_property('kit_bindings', bindings)
        changed = True
    moved = LINK.write_grab_move(cls, GRAB['total'], GRAB['first_actionable'], GRAB['active_start'], GRAB['active_end'])
    log(f'grab: {cls.get_name()} frames {GRAB} changed={moved} (False with no change, or when the move has no grab hitbox: check)')
    if moved:
        saves.append(bp)
    return changed


def run():
    del _lines[:]
    kit = unreal.load_asset(KIT)
    if not kit:
        log(f'ERROR: no {KIT}')
        return []
    saves = []
    kit_changed = write_throws(kit, saves)
    kit_changed |= write_grab(kit, saves)
    mash = LINK.set_kit_grab_mash_rule(kit, MASH['base'], MASH['per_percent'], MASH['min'], MASH['max'], MASH['reduce_per_press'])
    log(f'mash rule {MASH} changed={mash}')
    if kit_changed or mash:
        saves.append(kit)
    saved = []
    for asset in {a.get_path_name(): a for a in saves if a}.values():
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        saved.append(asset.get_path_name())
    log(f'saved {len(saved)}: ' + ', '.join(p.rsplit('/', 1)[-1].split('.')[0] for p in saved))
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Logs', 'grab-ganon', 'setup.log')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'WARNING' in l or 'ERROR' in l][:4] + [_lines[-1][:300]]:
        print(line)
    return saved


if __name__ == '__main__':
    run()
