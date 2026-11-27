# F2 (owner 2026-11-17, decision 2B; scope: the standard rules + the test and playtest kits): switches GSTN_CombatRules_Standard to the Smash
# knockback formula and writes every kit move's base (BKB, cm/s) and growth (KBG). Rerun-safe. The old model stays one flag away ("two plausible
# behaviours = a setting"): --model weight puts back the weight driven model, its curve and the old bases (growth 0).
# Formulas, targets and the move list: Tools/GAS/knockback_smash_data.py (its table: docs/notes/reviews/feel-f2/knockback_table.csv).
#
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<proj>/Tools/GAS/setup_knockback_smash.py [--model smash|weight] [--tilts hop0|smash] [--no-overlord]"
# In the editor: import setup_knockback_smash as s; s.run(model='smash', tilts='hop0', overlord=True)
#   --tilts hop0  (default) archetype tilts launch at 0 % as a low 30 deg hop, as in the 2026-11-17 playtest; smash = launch from ~19 / 11 / 4 %
#   --balloon on|off  switch the rules' balloon knockback (Ultimate constants, the struct defaults); absent = left as it is
#   --rules-only  write only the rules asset (model, curve, thresholds of knockback_smash_data): the moves are left as they are
#   --no-overlord leave the Overlord moves alone (under the Smash model they then keep their 0 % knockback at every percent: flat)
# Writes the rules asset, the ability Blueprints of the kits (inline timelines), the Overlord timelines when they are on an asset, and BP children
# of the common throws when a kit plays a native throw (GA_Smash_<Kit>_Throw<Dir> under /Game/GSTN/Characters/Overlord/Moves/Playtest).
# Moves that a kit binds and the table does not list are converted with the 'keep the 0 % knockback, KBG 70' rule (Strong commands: KO rule).
# Rerunning setup_test_kit_archetypes.py / setup_playtest_launch_1117.py / setup_launch_impacts.py resets bases with growth 0: rerun this after.
# Out: Saved/KnockbackSmash/setup.log (every move before -> after) and Saved/KnockbackSmash/before.json (the values --model weight restores).
import json
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gstn_content as gc  # noqa: E402  (where each asset lives under /Game/GSTN)
import knockback_smash_data as D  # noqa: E402

RULES = '/GSTNCombat/Rules/GSTN_CombatRules_Standard'
CHILD_DIR = gc.move_folder('Playtest')
KITS = {k: gc.path(f'KIT_Test_{k}') for k in ('Light', 'Medium', 'Heavy', 'Playtest')}
EXTRA_KITS = {'Examples': '/Game/GSTN/_Examples/KIT_Example_Impacts'}
THROW_DIRS = ('Forward', 'Back', 'Up', 'Down')
NATIVE_THROWS = {'Forward': 'GSTN_GA_LinkThrowForward', 'Back': 'GSTN_GA_LinkThrowBack', 'Up': 'GSTN_GA_LinkThrowUp', 'Down': 'GSTN_GA_LinkThrowDown'}
SAVED = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'KnockbackSmash')
OLD_PERCENT_GROWTH, OLD_CURVE = D.OLD_PERCENT_GROWTH, D.OLD_CURVE
HIT_RE = re.compile(r'\[([\d.]+)% b([\d.]+) g([\d.]+) f(-?[\d.]+) a(-?[\d.]+)\]')
L = unreal.GSTN_TestKitContentLibrary
LINK = getattr(unreal, 'GSTN_LinkContentLibrary', None)
_lines = []
BALLOON = None   # --balloon on|off: switch the balloon knockback of the rules; absent = leave it as it is


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[knockback_smash] {msg}')


def _hits(text):
    """[(damage, base, growth, fixed)] of a Describe* string"""
    return [(float(d), float(b), float(g), float(f)) for d, b, g, f, _a in HIT_RE.findall(text)]


def _known(name):
    """The table row of a class name (BP generated classes end in _C)"""
    base = name[:-2] if name.endswith('_C') else name
    for row in D.MOVES:
        if base == row[2] or (row[3] == 'overlord' and base.startswith(row[2].rstrip('V9'))):
            return row
    return None


# ---- Rules
def set_rules(model):
    rules = unreal.load_asset(RULES)
    if not rules:
        log(f'ERROR: no {RULES}')
        return None
    current = rules.get_editor_property('knockback_model')
    before = current.get_class().get_name() if current else 'None'
    if model == 'smash':
        if not isinstance(current, unreal.GSTN_KnockbackModel_PercentScaling) or isinstance(current, unreal.GSTN_KnockbackModel_CappedPercent):
            current = unreal.GSTN_CombatRules.switch_knockback_model(rules, unreal.GSTN_KnockbackModel_PercentScaling)
        current.set_editor_property('unit_scale', D.UNIT_SCALE)
        current.set_editor_property('base_knockback_in_cm_per_second', True)
        unreal.GSTN_CombatRules.set_default_weight_curve_linear(rules, 1.0, 1.0)   # the percent is a term of the formula: a falling curve counts it twice
        # Every target of the table assumes the thresholds of knockback_smash_data (launch 750, owner playtest 2026-11-16). Found at 1080 on
        # 2026-11-24: every hit between 750 and 1080 was a horizontal push back instead of a launch (lanci-diagnosi-2026-11-24.md)
        tuning = current.get_editor_property('tuning')
        for prop, value in (('push_back_threshold', D.PUSH_BACK), ('launch_threshold', D.LAUNCH), ('tumble_threshold', D.TUMBLE)):
            if abs(tuning.get_editor_property(prop) - value) > 0.5:
                log(f'rules: {prop} {tuning.get_editor_property(prop):.0f} -> {value:.0f}')
                tuning.set_editor_property(prop, value)
        if BALLOON is not None:
            # Balloon knockback (owner 2026-11-24, decision A: "Ultimate + balloon"): Ultimate's constants are the struct defaults
            balloon = tuning.get_editor_property('balloon')
            if balloon.get_editor_property('enabled') != BALLOON:
                log(f'rules: balloon {balloon.get_editor_property("enabled")} -> {BALLOON}')
                balloon.set_editor_property('enabled', BALLOON)
                tuning.set_editor_property('balloon', balloon)
        current.set_editor_property('tuning', tuning)
    else:
        if not isinstance(current, unreal.GSTN_KnockbackModel_WeightDriven):
            current = unreal.GSTN_CombatRules.switch_knockback_model(rules, unreal.GSTN_KnockbackModel_WeightDriven)
        current.set_editor_property('percent_growth', OLD_PERCENT_GROWTH)
        unreal.GSTN_CombatRules.set_default_weight_curve_linear(rules, OLD_CURVE[0], OLD_CURVE[1])
    t = current.get_editor_property('tuning')
    log(f'rules: {before} -> {current.get_class().get_name()} (push {t.get_editor_property("push_back_threshold"):.0f}, launch '
        f'{t.get_editor_property("launch_threshold"):.0f}, tumble {t.get_editor_property("tumble_threshold"):.0f}, curve at 1 = '
        f'{unreal.GSTN_CombatRules.evaluate_default_weight_curve(rules, 1.0):.2f})')
    return rules


# ---- Moves
def _role(kit, name, command):
    row = _known(name)
    if row:
        return row[3], (row[1] if row[1] in ('Light', 'Medium', 'Heavy') else kit)
    if 'Overlord' in name:
        return 'overlord', kit
    if command.startswith('Command.Strong'):
        return 'strong', kit
    return 'other', kit


def convert_move(kit, cls, command, model, tilts, overlord, before, after, done):
    name = cls.get_name()
    if name in done:
        return []
    done.add(name)
    role, row_kit = _role(kit, name, command)
    if role.startswith('overlord') and not overlord:
        log(f'{kit} {name}: Overlord move skipped (--no-overlord)')
        return []
    desc = L.describe_move_knockback(cls)
    hits = _hits(desc)
    if not hits:
        return []
    asset = L.get_move_timeline_asset(cls)
    changed = False
    for i, hit in enumerate(hits):
        key = f'{name}#{i}'
        if hit[3] >= 0:
            continue           # set knockback: no percent, no weight, the same in both models
        if model == 'smash':
            # The old base: before.json when this script already ran here; else the hitbox's own base while its growth is 0 (never converted)
            if key not in before and hit[2] == 0:
                before[key] = {'base': hit[1], 'growth': 0.0}
            if key not in before:
                log(f'{kit} {name} #{i}: growth {hit[2]:.0f} already set and no before.json record: kept')
                continue
            bkb, kbg = D.convert(role, row_kit if row_kit in ('Light', 'Medium', 'Heavy') else 'Medium', hit[0], before[key]['base'], tilts, name)
        else:
            old = before.get(key)
            if old is None:
                # no record: derive the 0 % knockback the conversion kept (keep0 rule), else leave it
                bkb, kbg = (hit[1], 0.0) if hit[2] == 0 else (round(D.k_new(hit[1], hit[2], hit[0], 0)), 0.0)
            else:
                bkb, kbg = old['base'], old['growth']
        changed |= L.write_move_hit_knockback(cls, i, bkb, kbg, True)
        after[key] = {'base': bkb, 'growth': kbg}
    log(f'{kit} {name} ({role}{", timeline asset" if asset else ""}): {desc} -> {L.describe_move_knockback(cls)} changed={changed}')
    saves = []
    if changed:
        saves.append(asset if asset else unreal.load_asset(cls.get_path_name().rsplit('.', 1)[0]))
    return saves


def _child_throw(kit, direction, parent):
    name = f'GA_Smash_{kit}_Throw{direction}'
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


def convert_throws(kit, kit_asset, model, before, after, done):
    if not LINK:
        return []
    ok, *throws = LINK.get_kit_throws(kit_asset)
    if not ok:
        return []
    saves, classes, rebind = [], [], False
    row_kit = kit if kit in ('Light', 'Medium', 'Heavy') else 'Medium'
    for direction, cls in zip(THROW_DIRS, throws):
        if cls is None or cls.get_name().startswith('GSTN_GA_'):
            if model == 'weight':
                classes.append(cls)
                continue
            parent = cls or getattr(unreal, NATIVE_THROWS[direction]).static_class()
            bp, cls = _child_throw(kit, direction, parent)
            saves.append(bp)
            rebind = True
        classes.append(cls)
        name = cls.get_name()
        if name in done:
            continue
        if name.startswith('GA_Ganon_'):
            # setup_grab_ganon_playtest.py writes these from Smash Ultimate itself (BKB x 22.5, KBG as is): never reconverted
            log(f'{kit} {name} (throw {direction}): Ganondorf numbers, left alone')
            continue
        done.add(name)
        desc = LINK.describe_throw_knockback(cls)
        hits = _hits(desc)
        if not hits or hits[0][3] >= 0:
            continue
        damage, base, growth, _f = hits[0]
        key = f'{name}#0'
        if model == 'smash':
            if key not in before and growth == 0:
                before[key] = {'base': base, 'growth': 0.0}
            old = before.get(key, {}).get('base', base)
            bkb, kbg = D.convert(f'throw_{direction}', row_kit, damage, old)
        else:
            rec = before.get(key, {'base': base if growth == 0 else round(D.k_new(base, growth, damage, 0)), 'growth': 0.0})
            bkb, kbg = rec['base'], rec['growth']
        changed = LINK.write_throw_knockback(cls, bkb, kbg)
        after[key] = {'base': bkb, 'growth': kbg}
        log(f'{kit} {name} (throw {direction}): {desc} -> {LINK.describe_throw_knockback(cls)} changed={changed}')
        if changed:
            saves.append(unreal.load_asset(cls.get_path_name().rsplit('.', 1)[0]))
    if rebind and LINK.set_kit_throws(kit_asset, *classes):
        log(f'{kit}: throws -> {", ".join(c.get_name() for c in classes if c)}')
        saves.append(kit_asset)
    return saves


def _bindings(kit_asset):
    """[(command, ability class)] of the kit and its weapons"""
    out = []
    owners = [(kit_asset, 'kit_bindings')]
    for prop in ('primary_weapon', 'secondary_weapon'):
        try:
            weapon = kit_asset.get_editor_property(prop)
        except Exception:
            weapon = None
        if weapon:
            owners.append((weapon, 'bindings'))
    for owner, prop in owners:
        for b in owner.get_editor_property(prop):
            cls = b.get_editor_property('ability')
            if cls:
                out.append((str(b.get_editor_property('command').get_editor_property('tag_name')), cls))
    return out


def run(model='smash', tilts='hop0', overlord=True, rules_only=False):
    del _lines[:]
    assert model in ('smash', 'weight') and tilts in D.TILT_PRESETS
    os.makedirs(SAVED, exist_ok=True)
    before_path = os.path.join(SAVED, 'before.json')
    before = json.load(open(before_path)) if os.path.exists(before_path) else {}
    after = {}
    saves = []
    rules = set_rules(model)
    if rules:
        saves.append(rules)
    done = set()
    for kit, path in ([] if rules_only else list(KITS.items()) + list(EXTRA_KITS.items())):
        kit_asset = unreal.load_asset(path)
        if not kit_asset:
            log(f'WARNING: no {path}')
            continue
        for command, cls in _bindings(kit_asset):
            saves += convert_move(kit, cls, command, model, tilts, overlord, before, after, done)
        saves += convert_throws(kit, kit_asset, model, before, after, done)
    for asset in {a.get_path_name(): a for a in saves if a}.values():
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    if model == 'smash' and not rules_only:
        json.dump(before, open(before_path, 'w'), indent=1, sort_keys=True)
    with open(os.path.join(SAVED, 'setup.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    problems = [l for l in _lines if 'WARNING' in l or 'ERROR' in l]
    for line in (problems[:3] + [_lines[0][:200] if _lines else 'nothing done',
                                 f'{len(after)} hitboxes written, {len(saves)} assets saved -> Saved/KnockbackSmash/setup.log'])[:5]:
        print(line)
    return after


def _arg(flag, default):
    return sys.argv[sys.argv.index(flag) + 1] if flag in sys.argv[:-1] else default


if __name__ == '__main__':
    if '--balloon' in sys.argv[:-1]:
        BALLOON = _arg('--balloon', 'on') == 'on'
    run(model=_arg('--model', 'smash'), tilts=_arg('--tilts', 'hop0'), overlord='--no-overlord' not in sys.argv,
        rules_only='--rules-only' in sys.argv)
