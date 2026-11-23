# Owner decision 2026-11-17 (option A, data only; thread dair-dropkick-anim round 2): the playtest dropkick downs its user on landing and the 25 cm
# Self Landing Bounce plays, with the kit's GENERIC GroundBounce (the dropkick's own bounce ability goes on only after its clip passes review).
#   KIT_Test_Playtest     defence extension rules = DA_DefenceRules_Test (the kit had none and the standard combat rules carry none: no defence data
#                         at all for the player). The get-up set, tech set and the shield ability the kit already has are kept
#   TL_Overlord_Dropkick  Tags window Combat.Move.KnockdownOnLanding over the whole move (same as TL_Overlord_DropkickTB); no BounceAbility
# Owner standing rule (2026-11-17 21:41, scope: shared systems): shared systems go on ALL test kits; move-specific variants stay in their own kit
# until approved. DEFENCE_RULES_ALL_KITS=1 (run_all_kits): every kit under /Game with a defence extension and no rules gets RULES (its shield,
# get-up and tech sets kept). Kits that already name rules are left as they are.
# Rerun-safe, saves .uasset only. Editor: import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_playtest_dropkick_knockdown as s; s.run()
# Output: Saved/PlaytestFixes/setup_playtest_dropkick_knockdown.txt, at most 5 printed lines.
import os
import re

import unreal

KIT = '/Game/GASNPPTest/Combat/KIT_Test_Playtest'
TIMELINE = '/Game/GASNPPTest/Combat/Overlord/TL_Overlord_Dropkick'
RULES = '/GSTNDefence/Test/DA_DefenceRules_Test'
WINDOW_TAG = 'Combat.Move.KnockdownOnLanding'
# After the bounce clip's review PASS (Fable r3, 2026-11-17): PLAYTEST_DROPKICK_OWN_BOUNCE=R3 sets the dropkick's own bounce ability on
# TL_Overlord_Dropkick's Self Landing Bounce and registers it in KIT_Test_Playtest's TriggeredAbilities (the model starts only registered classes).
# Empty = the kit's generic GroundBounce (option A as first applied)
OWN_BOUNCE = os.environ.get('PLAYTEST_DROPKICK_OWN_BOUNCE', '')
OWN_BOUNCE_GA = '/Game/GASNPPTest/Combat/Overlord/GA_Overlord_DropkickTechBounce' + OWN_BOUNCE
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'PlaytestFixes', 'setup_playtest_dropkick_knockdown.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[playtest_dropkick_kd] {msg}')


def _shield(kit):
    """The shield ability class the kit's defence extension names now (kept), None when none"""
    text = kit.get_editor_property('extensions').export_text()
    m = re.search(r'ShieldAbility="?([^",)]+)', text)
    if not m or m.group(1) in ('None', ''):
        return None
    path = m.group(1).strip("'")
    cls = unreal.load_class(None, path) if '.' in path else None
    log(f'shield ability kept: {path} ({"loaded" if cls else "NOT loaded"})')
    return cls


def _window(asset):
    data = asset.get_editor_property('data')
    text = data.export_text()
    if WINDOW_TAG in text:
        log('TL_Overlord_Dropkick: window already there')
        return False
    total = data.get_editor_property('total_frames')
    window = f'/Script/GSTNCombatCore.GSTN_TimelineTrack_Tags(StartFrame=0,EndFrame={total},Tags=(GameplayTags=((TagName="{WINDOW_TAG}"))))'
    i = text.find('Tracks=(')
    if i < 0:
        raise RuntimeError('no Tracks in the dropkick timeline text')
    j = i + len('Tracks=(')
    data.import_text(text[:j] + window + ('' if text[j] == ')' else ',') + text[j:])
    if WINDOW_TAG not in data.export_text():
        raise RuntimeError('the KnockdownOnLanding window did not import')
    asset.set_editor_property('data', data)
    log(f'TL_Overlord_Dropkick: Tags window {WINDOW_TAG} [0, {total})')
    return True


def run():
    del _lines[:]
    ok = False
    try:
        kit, rules, tl = unreal.load_asset(KIT), unreal.load_asset(RULES), unreal.load_asset(TIMELINE)
        if not (kit and rules and tl):
            raise RuntimeError(f'missing asset: kit {bool(kit)} rules {bool(rules)} timeline {bool(tl)}')
        shield = _shield(kit)
        if not unreal.GSTN_DefenceContentLibrary.set_kit_defence_extension(kit, rules, shield):
            raise RuntimeError('set_kit_defence_extension failed')
        log(f'KIT_Test_Playtest: defence rules {RULES}')
        _window(tl)
        if OWN_BOUNCE:
            cls = unreal.EditorAssetLibrary.load_blueprint_class(OWN_BOUNCE_GA)
            if not cls:
                raise RuntimeError(f'{OWN_BOUNCE_GA} missing')
            tl.set_editor_property('data', unreal.GSTN_DefenceContentLibrary.set_self_landing_bounce_ability(tl.get_editor_property('data'), cls))
            if OWN_BOUNCE_GA.rsplit('/', 1)[1] not in tl.get_editor_property('data').export_text():
                raise RuntimeError('BounceAbility not set on TL_Overlord_Dropkick')
            trig = list(kit.get_editor_property('triggered_abilities'))
            if not any(OWN_BOUNCE_GA.rsplit('/', 1)[1] + '_C' in str(t) for t in trig):
                trig.append(cls)
                kit.set_editor_property('triggered_abilities', trig)
            log(f'TL_Overlord_Dropkick: BounceAbility = {OWN_BOUNCE_GA.rsplit("/", 1)[1]}; KIT_Test_Playtest triggered abilities {len(trig)}')
        for a in (kit, tl):
            unreal.EditorAssetLibrary.save_loaded_asset(a, False)
        log('saved: KIT_Test_Playtest, TL_Overlord_Dropkick')
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'ERROR' in l][:4] + [_lines[-1][:300] if _lines else '']:
        print(line)
    return ok


def _needs_rules(kit):
    """The kit's defence extension text, or None when the kit has none or it already names rules"""
    text = kit.get_editor_property('extensions').export_text()
    i = text.find('GSTN_DefenceKitExtension')
    if i < 0:
        return None
    m = re.search(r'Rules=("?)([^",)]*)', text[i:])
    value = m.group(2) if m else ''
    return None if value and value != 'None' else text


def run_all_kits():
    """DEFENCE_RULES_ALL_KITS: the owner's shared-systems rule for the defence rules; returns the kits it changed"""
    del _lines[:]
    changed = []
    try:
        rules = unreal.load_asset(RULES)
        for path in unreal.EditorAssetLibrary.list_assets('/Game', True, False):
            name = path.rsplit('/', 1)[-1].split('.')[0]
            if not name.startswith('KIT_'):
                continue
            kit = unreal.load_asset(path)
            if not isinstance(kit, unreal.GSTN_PrimaryKitDefinition) or _needs_rules(kit) is None:
                continue
            shield = _shield(kit)
            if unreal.GSTN_DefenceContentLibrary.set_kit_defence_extension(kit, rules, shield):
                unreal.EditorAssetLibrary.save_loaded_asset(kit, False)
                changed.append(name)
        log(f'defence rules {RULES} given to {len(changed)} kits: {", ".join(changed)}')
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT.replace('.txt', '_all_kits.txt'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    print(_lines[-1][:300] if _lines else '')
    return changed


if __name__ == '__main__':
    run()
