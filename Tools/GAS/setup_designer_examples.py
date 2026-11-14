# Designer examples (owner plan steps 3-4, 2026-11-13): a small example kit a designer can open, read, copy and retune, made only of data the way a
# designer would make it (duplicate an existing move, change numbers, bind it to a command). Nothing here is test-only logic.
# Run headless (editor closed): UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file>"   or   import setup_designer_examples as d; d.run()
# Writes under /Game/GASNPPTest/Examples (the kit and weapon scan of the Asset Manager only covers /Game/GASNPPTest, DefaultGame.ini):
#   GA_Example_Light     X (Command.Attack.*)    a duplicate of GA_Medium_Jab with its own frame data (fast, small, short push)
#   GA_Example_Strong    RT (Command.Strong.*)   a duplicate of GA_Heavy_Jab (slow startup, big hitbox, far knockback)
#   GA_Example_Special   Y (Command.Special.*)   a duplicate of GA_Medium_Special (a launcher: steep angle, sends up)
#   GA_Example_BackpackDome  a Blueprint child of GSTN_GA_BackpackDome (dodge button with the stick at rest; its timeline in Class Defaults)
#   WPN_Example          copy of WPN_Test_Medium: light and strong bound on both the neutral and the directional command
#   KIT_Example          copy of KIT_Test_Medium (dodges, grab, throws, jump kept) with WPN_Example and the special; label "ESEMPIO"
#   DA_Example_Backpack_Dome  copy of DA_Backpack_ShieldDome with its own identity tag and GA_Example_BackpackDome
# Created ONCE: a rerun leaves existing assets alone (the designer's edits stay). run(force=True) rewrites the frame data and bindings.
# Output: Saved/Examples/setup_examples.log (+ a dump of every example's data in Saved/Examples/examples_dump.txt), at most 5 printed lines.
import os

import unreal

SRC = '/Game/GASNPPTest/Combat'
DIR = '/Game/GASNPPTest/Examples'
SHOVE = '/Game/Characters/UEFN_Mannequin/Animations/Interactions/Shoves/AM_M_relaxed_ragdoll_shove_stand_{}_A'
TAKEDOWN = '/Game/Characters/UEFN_Mannequin/Animations/Interactions/Takedowns/AM_M_relaxed_takedown_stand_{}_A'

# (source Blueprint, commands it answers, strike spec). Frames at 60 Hz. Every number is a starting point for the designer, not a balance pass.
MOVES = {
    'GA_Example_Light': ('Archetypes/GA_Medium_Jab', ['Command.Attack.Neutral', 'Command.Attack.Directional'], dict(
        total_frames=18, first_actionable_frame=14, active_start=4, active_end=6, radius=45.0, reach_from=70.0, reach_to=150.0, height=0.0,
        damage=4.0, base_knockback=350.0, knockback_growth=0.0, angle_deg=35.0, hitstop_frames=5, attacker_hitstop_frames=3,
        montage=SHOVE.format('F'), montage_play_rate=1.6)),
    'GA_Example_Strong': ('Archetypes/GA_Heavy_Jab', ['Command.Strong.Neutral', 'Command.Strong.Directional'], dict(
        total_frames=42, first_actionable_frame=36, active_start=14, active_end=18, radius=70.0, reach_from=90.0, reach_to=210.0, height=10.0,
        damage=14.0, base_knockback=1100.0, knockback_growth=0.0, angle_deg=40.0, hitstop_frames=10, attacker_hitstop_frames=8,
        montage=TAKEDOWN.format('F'), montage_play_rate=0.9)),
    'GA_Example_Special': ('Archetypes/GA_Medium_Special', ['Command.Special.Neutral', 'Command.Special.Directional'], dict(
        total_frames=34, first_actionable_frame=30, active_start=8, active_end=12, radius=75.0, reach_from=40.0, reach_to=110.0, height=40.0,
        damage=8.0, base_knockback=900.0, knockback_growth=0.0, angle_deg=80.0, hitstop_frames=8, attacker_hitstop_frames=6,
        montage=TAKEDOWN.format('L'), montage_play_rate=1.2)),
}
# Startup steering (owner 2026-11-14, handoffs/attack-aiming-research.md): which stick turns the move, how far (cap, deg), how fast (deg/s at
# 62.5 Hz: 450 = 7.2 deg/frame) and until which frame (-1 = the first active frame). Written on every run: these are the test profiles.
#   Strong   left stick (Monster Hunter style), up to 90 deg, slow: the heavy charge that corrects its aim
#   Special  "signature" profile (Helm Breaker style): either stick, any direction (180) but only in its first 6 frames, fast
STEER = {
    'GA_Example_Strong': dict(cap=90.0, rate=450.0, source='MOVE_STICK', start=0, end=-1),
    'GA_Example_Special': dict(cap=180.0, rate=900.0, source='MOVE_OR_AIM_STICK', start=0, end=6),
}
WEAPON_MOVES = ('GA_Example_Light', 'GA_Example_Strong')   # on the weapon (they change with the weapon); the special is the kit's
KIT_LABEL = 'ESEMPIO'
KIT_TAG, WEAPON_TAG, BACKPACK_TAG = 'Kit.Example', 'Weapon.Example', 'Backpack.Example.Dome'

_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[designer_examples] {msg}')


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _tags(*names):
    return unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([_tag(n) for n in names])


def _exists(name):
    return unreal.EditorAssetLibrary.does_asset_exist(f'{DIR}/{name}')


def _copy(source_path, name):
    """(asset, created now). Duplicates source_path to DIR/name when missing"""
    path = f'{DIR}/{name}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path), False
    if not unreal.EditorAssetLibrary.duplicate_asset(source_path, path):
        log(f'ERROR: could not copy {source_path} to {name}')
        return None, False
    log(f'created {name} (copy of {source_path.rsplit("/", 1)[-1]})')
    return unreal.load_asset(path), True


def _child_blueprint(name, parent_name):
    """(Blueprint, created now): a Blueprint child of a native ability class"""
    path = f'{DIR}/{name}'
    bp = unreal.load_asset(path)
    if bp:
        return bp, False
    factory = unreal.GameplayAbilitiesBlueprintFactory() if hasattr(unreal, 'GameplayAbilitiesBlueprintFactory') else unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', getattr(unreal, parent_name).static_class())
    asset_class = unreal.GameplayAbilityBlueprint if hasattr(unreal, 'GameplayAbilityBlueprint') else unreal.Blueprint
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DIR, asset_class, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log(f'created {name} (child of {parent_name})')
    return bp, True


def _strike_spec(values):
    spec = unreal.GSTN_TestStrikeSpec()
    for prop, value in values.items():
        if prop == 'montage':
            value = unreal.load_asset(value)
            if not value:
                log(f'WARNING: no montage {values["montage"]}')
        spec.set_editor_property(prop, value)
    return spec


def _write_steer(cls, values):
    steer = unreal.GSTN_SteerWindow()
    steer.set_editor_property('source', getattr(unreal.GSTN_SteerSource, values['source']))
    steer.set_editor_property('start_frame', values['start'])
    steer.set_editor_property('end_frame', values['end'])
    changed = unreal.GSTN_TestKitContentLibrary.write_startup_steer(cls, values['cap'], values['rate'], steer)
    log(f'steer {cls.get_name()}: {values} changed={changed}')
    return changed


def _command(binding):
    return str(binding.get_editor_property('command').get_editor_property('tag_name'))


def _bind(bindings, commands, cls, template_command):
    """bindings with every command in `commands` bound to cls: an existing binding of that command is retargeted, a missing one is added as a
    copy of the binding of template_command (same requirements). Returns the new list"""
    out = list(bindings)
    template = next((b for b in out if _command(b) == template_command), None)
    for command in commands:
        found = [b for b in out if _command(b) == command]
        for b in found:
            b.set_editor_property('ability', cls)
        if not found:
            b = unreal.GSTN_AbilityBinding()
            b.set_editor_property('command', _tag(command))
            b.set_editor_property('ability', cls)
            if template:
                b.set_editor_property('requirements', template.get_editor_property('requirements'))
            out.append(b)
    return out


def run(force=False):
    del _lines[:]
    if not unreal.EditorAssetLibrary.does_directory_exist(DIR):
        unreal.EditorAssetLibrary.make_directory(DIR)
    saves = []

    classes = {}
    for name, (source, _commands, spec) in MOVES.items():
        bp, created = _copy(f'{SRC}/{source}', name)
        if not bp:
            return _finish(saves)
        cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DIR}/{name}')
        classes[name] = cls
        if created or force:
            unreal.GSTN_TestKitContentLibrary.write_strike_move(cls, _strike_spec(spec))
            saves.append(bp)
        if name in STEER and _write_steer(cls, STEER[name]) and bp not in saves:
            saves.append(bp)

    dome_bp, dome_new = _child_blueprint('GA_Example_BackpackDome', 'GSTN_GA_BackpackDome')
    dome_cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DIR}/GA_Example_BackpackDome')
    if dome_new:
        saves.append(dome_bp)

    weapon, w_new = _copy(f'{SRC}/WPN_Test_Medium', 'WPN_Example')
    if weapon and (w_new or force):
        weapon.set_editor_property('display_name', 'WPN_Example')
        weapon.set_editor_property('weapon_tags', _tags(WEAPON_TAG, 'Test.Role.Primary'))
        bindings = list(weapon.get_editor_property('bindings'))
        for name in WEAPON_MOVES:
            bindings = _bind(bindings, MOVES[name][1], classes[name], 'Command.Attack.Neutral')
        weapon.set_editor_property('bindings', bindings)
        saves.append(weapon)

    kit, k_new = _copy(f'{SRC}/KIT_Test_Medium', 'KIT_Example')
    if kit and weapon and (k_new or force):
        kit.set_editor_property('display_name', KIT_LABEL)
        kit.set_editor_property('kit_tags', _tags(KIT_TAG))
        kit.set_editor_property('primary_weapon', weapon)
        kit.set_editor_property('kit_bindings', _bind(list(kit.get_editor_property('kit_bindings')), MOVES['GA_Example_Special'][1],
                                                      classes['GA_Example_Special'], 'Command.Special.Neutral'))
        saves.append(kit)

    backpack, b_new = _copy('/Game/GAS/Backpacks/DA_Backpack_ShieldDome', 'DA_Example_Backpack_Dome')
    if backpack and (b_new or force):
        backpack.set_editor_property('backpack_tags', _tags(BACKPACK_TAG))
        abilities = list(backpack.get_editor_property('abilities'))
        for b in abilities:
            if _command(b) == 'Command.Backpack.Dome':
                b.set_editor_property('ability', dome_cls)
        backpack.set_editor_property('abilities', abilities)
        saves.append(backpack)
    return _finish(saves)


def _finish(saves):
    for asset in saves:
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    log(f'saved {len(saves)}: ' + ', '.join(a.get_name() for a in saves))
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'Examples')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_examples.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    _dump(os.path.join(out_dir, 'examples_dump.txt'))
    return _lines[-5:]


def _dump(path):
    """What a designer sees in each asset (for the guide): the timeline of every move, the bindings of the weapon, kit and backpack"""
    rows = []
    for name in list(MOVES) + ['GA_Example_BackpackDome']:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DIR}/{name}')
        if cls:
            rows.append(f'== {name}')
            rows.append(unreal.get_default_object(cls).get_editor_property('timeline').export_text())
    for name, prop in (('WPN_Example', 'bindings'), ('KIT_Example', 'kit_bindings'), ('DA_Example_Backpack_Dome', 'abilities')):
        asset = unreal.load_asset(f'{DIR}/{name}')
        if asset:
            rows.append(f'== {name}.{prop}')
            rows += [f'  {_command(b)} -> {b.get_editor_property("ability")}' for b in asset.get_editor_property(prop)]
    with open(path, 'w') as f:
        f.write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    for line in run():
        print(line)
