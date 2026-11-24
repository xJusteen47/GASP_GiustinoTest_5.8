# Playtest launch tuning (owner 2026-11-17, scope: playtest kits only, NOT Overlord moves, NOT the global rules).
#   "knockback 30 deg from the ground, launch even at 0 % from a low value that grows with %" -> per move: the ground hits get AngleDeg 30 and
#   a base just above the launch threshold (standard rules: WeightDriven, LaunchThreshold 750, PercentGrowth 1.8, so K = base * (1 + 1.8 %)
#   on a weight-100 dummy: 780 launches at 0 % as a low hop, ~1400 at 40 %, tumble (1800) from ~73 %).
# Kits: KIT_Test_Playtest (its X on the ground and its special, through Blueprint children of the native test moves) and the three archetype
# kits KIT_Test_Light/Medium/Heavy (tilt, special, strong). Jabs of the archetypes stay as they are (Smash-like jab, owner question open).
# Specials keep their angle (light 80 pop-up, heavy 60: the move's identity); only their base is raised to launch at 0 %.
# Run in the editor (python Saved/AnimR2/run_in_editor.py Tools/GAS/setup_playtest_launch_1117.py) or headless. Rerun-safe.
# Out: Saved/PlaytestFixes/launch_1117.txt (before/after of every move)
import os
import re
import unreal
import sys  # noqa: E402
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gstn_content as gc  # noqa: E402  (where each asset lives under /Game/GSTN)

ABILITY_DIR = gc.move_folder('Playtest')
BASE = 780.0
ANGLE = 30.0
KEEP_ANGLE = -999.0
_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(_PROJ, 'Saved', 'PlaytestFixes', 'launch_1117.txt')
lines = []
L = unreal.GSTN_TestKitContentLibrary


def log(m):
    lines.append(m)


def _child(name, parent_cls):
    """A Blueprint child of a native or Blueprint ability class, created once. Returns (Blueprint, generated class)"""
    path = f'{ABILITY_DIR}/{name}'
    if not unreal.EditorAssetLibrary.does_directory_exist(ABILITY_DIR):
        unreal.EditorAssetLibrary.make_directory(ABILITY_DIR)
    bp = unreal.load_asset(path)
    if not bp:
        factory = unreal.GameplayAbilitiesBlueprintFactory() if hasattr(unreal, 'GameplayAbilitiesBlueprintFactory') else unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', parent_cls)
        asset_class = unreal.GameplayAbilityBlueprint if hasattr(unreal, 'GameplayAbilityBlueprint') else unreal.Blueprint
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ABILITY_DIR, asset_class, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f'created {path} (child of {parent_cls.get_name()})')
    return bp, unreal.EditorAssetLibrary.load_blueprint_class(path)


def _tune(cls, base, angle, label):
    before = L.describe_move_knockback(cls)
    changed = L.write_move_knockback(cls, base, 0.0, angle)
    log(f'{label} {cls.get_name()}: {before} -> {L.describe_move_knockback(cls)} changed={changed}')
    return changed


def _rebind(owner, prop, command, cls, require=None):
    out, n = [], 0
    for b in owner.get_editor_property(prop):
        tag = str(b.get_editor_property('command').get_editor_property('tag_name'))
        req = b.get_editor_property('requirements').export_text()
        if tag == command and (require is None or require in req):
            b.set_editor_property('ability', cls)
            n += 1
        out.append(b)
    owner.set_editor_property(prop, out)
    return n


def _bound(owner, prop, command, require=None):
    for b in owner.get_editor_property(prop):
        tag = str(b.get_editor_property('command').get_editor_property('tag_name'))
        if tag == command and (require is None or require in b.get_editor_property('requirements').export_text()):
            return b.get_editor_property('ability')
    return None


def run():
    del lines[:]
    saves = []
    # 1) KIT_Test_Playtest: X on the ground (native GSTN_GA_TestHitMove) and the special (native GSTN_GA_TestSpecial)
    kit = unreal.load_asset(gc.path('KIT_Test_Playtest'))
    weapon = kit.get_editor_property('primary_weapon')
    for prop_owner, prop, command, require, name, angle in (
            # (the special, GSTN_GA_TestSpecial, is not a timeline move: no hitbox data to tune, left as it is)
            (weapon, 'bindings', 'Command.Attack.Neutral', 'Input.Context.Grounded', 'GA_Playtest_GroundHit', ANGLE),):
        current = _bound(prop_owner, prop, command, require)
        if not current:
            log(f'WARNING: {prop_owner.get_name()} has no {command} binding')
            continue
        if current.get_name().startswith(name):
            cls = current
            bp = unreal.load_asset(f'{ABILITY_DIR}/{name}')
        else:
            bp, cls = _child(name, current)
            log(f'{prop_owner.get_name()} {command}: {current.get_name()} -> {cls.get_name()} ({_rebind(prop_owner, prop, command, cls, require)} bindings)')
            saves.append(prop_owner)
        _tune(cls, BASE, angle, 'playtest')
        saves.append(bp)
    # 2) Archetype kits: tilt (30 deg), special (base only), strong (a playtest copy: GA_Example_Strong is shared with the example kits)
    strong_cls = None
    for key in ('Light', 'Medium', 'Heavy'):
        for move, angle in (('Tilt', ANGLE), ('Special', KEEP_ANGLE)):
            path = gc.path(f'GA_{key}_{move}')
            cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
            if not cls:
                log(f'WARNING: {path} missing')
                continue
            cur = L.describe_move_knockback(cls)
            # Never lower a base that already launches higher (heavy special 1200): only raise to BASE
            import re
            bases = [float(x) for x in re.findall(r' b([\d.]+)', cur)]
            base = max([BASE] + bases) if move == 'Special' else max(BASE, min(bases) if bases else BASE)
            _tune(cls, base, angle, key.lower())
            saves.append(unreal.load_asset(path))
        akit = unreal.load_asset(gc.path(f'KIT_Test_{key}'))
        if not akit:
            continue
        current = _bound(akit, 'kit_bindings', 'Command.Strong.Neutral')
        if current:
            if strong_cls is None:
                if current.get_name().startswith('GA_Playtest_Strong'):
                    strong_cls = current
                else:
                    bp, strong_cls = _child('GA_Playtest_Strong', current)
                    saves.append(bp)
                # The example strong's own base stays when it is higher (1100): only its angle moves to 30
                bases = [float(x) for x in re.findall(r' b([\d.]+)', L.describe_move_knockback(strong_cls))]
                _tune(strong_cls, max([BASE] + bases), ANGLE, 'strong')
            n = _rebind(akit, 'kit_bindings', 'Command.Strong.Neutral', strong_cls) + _rebind(akit, 'kit_bindings', 'Command.Strong.Directional', strong_cls)
            log(f'KIT_Test_{key}: strong -> {strong_cls.get_name()} ({n} bindings)')
            saves.append(akit)
    for a in saves:
        if a:
            unreal.EditorAssetLibrary.save_loaded_asset(a, False)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w').write('\n'.join(lines) + '\n')
    print(f'{len(lines)} lines -> {OUT}')


if __name__ == '__main__':
    run()
