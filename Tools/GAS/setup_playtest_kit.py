# The single playtest kit and the launch tuning (owner playtest 2026-11-16). Data only, idempotent, saves .uasset only (never a map).
#   KIT_Test_Playtest  copy of KIT_Test_OverlordDropkick (= KIT_Test_Hits + the dropkick with its air timeout), plus every kit extension
#                      KIT_Test_Hits has and the copy lacks (locomotion profile, air crouch, defence get-ups, tumble recovery...).
#                      The hub gives it at Play through gstn.Debug.KitAtStart (Config/DefaultEngine.ini); the GSTN Debug Options window
#                      lists every kit. A rerun keeps an existing KIT_Test_Playtest (owner tuning) and only adds missing extensions.
#   GSTN_CombatRules_Standard > Knockback Model: launches start at low % like Smash. Before: LaunchThreshold 1080, PercentGrowth 1 =>
#                      a 550 strike launched from ~60 %. After: LaunchThreshold 750, PercentGrowth 1.8 => 550 launches from ~20 %,
#                      900 at 0 %, 400 from ~50 %; tumble (1800) for 550 at ~125 %, for 900 at ~55 %.
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_playtest_kit.py" -unattended -nullrhi
# Writes Saved/PlaytestKit/setup_playtest_kit.txt, prints one line.
import os

import unreal

FOLDER = '/Game/GASNPPTest/Combat'
KIT = FOLDER + '/KIT_Test_Playtest'
SOURCES = [FOLDER + '/KIT_Test_OverlordDropkick', FOLDER + '/KIT_Test_Hits']
EXTRA_FROM = FOLDER + '/KIT_Test_Hits'
RULES = '/GSTNCombat/Rules/GSTN_CombatRules_Standard'
LAUNCH_THRESHOLD = 750.0
PERCENT_GROWTH = 1.8


def _type_of(entry):
    text = entry.export_text()
    return text.split('(', 1)[0].strip() if '(' in text else text


def _kit(lines):
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(KIT):
        src = next((s for s in SOURCES if eal.does_asset_exist(s)), None)
        if not src or not eal.duplicate_asset(src, KIT):
            raise RuntimeError('cannot make %s' % KIT)
        lines.append('%s: copied from %s' % (KIT, src.rsplit('/', 1)[-1]))
    else:
        lines.append('%s: exists, kept' % KIT)
    kit = unreal.load_asset(KIT)
    extra = unreal.load_asset(EXTRA_FROM)
    ext = kit.get_editor_property('extensions')
    entries = list(ext.get_editor_property('extension_data'))
    have = {_type_of(e) for e in entries}
    added = []
    for e in extra.get_editor_property('extensions').get_editor_property('extension_data'):
        t = _type_of(e)
        if t not in have:
            entries.append(e)
            have.add(t)
            added.append(t.rsplit('.', 1)[-1])
    if added:
        ext.set_editor_property('extension_data', entries)
        kit.set_editor_property('extensions', ext)
    eal.save_loaded_asset(kit, False)
    lines.append('extensions now %d, added: %s' % (len(entries), ', '.join(added) or 'none'))
    lines.extend('  ' + _type_of(e).rsplit('.', 1)[-1] for e in entries)


def _rules(lines):
    rules = unreal.load_asset(RULES)
    model = rules.get_editor_property('knockback_model')
    tuning = model.get_editor_property('tuning')
    before = (tuning.get_editor_property('launch_threshold'), model.get_editor_property('percent_growth'))
    tuning.set_editor_property('launch_threshold', max(LAUNCH_THRESHOLD, tuning.get_editor_property('push_back_threshold')))
    model.set_editor_property('tuning', tuning)
    model.set_editor_property('percent_growth', PERCENT_GROWTH)
    unreal.EditorAssetLibrary.save_loaded_asset(rules, False)
    lines.append('rules: launch threshold %.0f -> %.0f, percent growth %.2f -> %.2f (push back %.0f, tumble %.0f)' % (
        before[0], tuning.get_editor_property('launch_threshold'), before[1], PERCENT_GROWTH,
        tuning.get_editor_property('push_back_threshold'), tuning.get_editor_property('tumble_threshold')))


def run():
    lines = []
    for step in (_kit, _rules):
        try:
            step(lines)
        except Exception as exc:  # one failing step must not hide the other
            lines.append('%s FAILED: %r' % (step.__name__, exc))
    out = os.path.join(unreal.Paths.project_saved_dir(), 'PlaytestKit')
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'setup_playtest_kit.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print('setup_playtest_kit: ' + ('FAILED' if any('FAILED' in l for l in lines) else 'OK') + ' (Saved/PlaytestKit/setup_playtest_kit.txt)')
    return lines


run()
