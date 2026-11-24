# F4 (owner 2026-11-17, scope: KIT_Test_Overlord, the orange Overlord kit): shared systems go on ALL test kits (owner standing rule 21:41).
# The Overlord kit missed what KIT_Test_Playtest got from the shared-systems scripts, and its dropkick (the forward air, TL_Overlord_Dropkick,
# shared with the playtest kit) slid: the timeline asks for a knockdown on landing (Combat.Move.KnockdownOnLanding) and names its own bounce
# ability, but a kit with no defence extension cannot knock its user down and the bounce ability was not registered, so the land part played
# with locomotion running under it (measured: 0 -> 451 cm/s while the feet stay planted, Saved/F4/loco_ov_walk70/dropkick_land_walk).
# What it does to KIT_Test_Overlord, copied from KIT_Test_Playtest (rerun-safe):
#   1. the defence extension (rules DA_DefenceRules_Test, get-up and tech sets), next to its locomotion and air crouch extensions
#   2. TriggeredAbilities: the dropkick's bounce ability (GA_Overlord_DropkickTechBounceR3), so the same move behaves the same in both kits
#   3. the kit bindings the playtest kit has and this one lacks (spin, air spin, spin cancel, drill: shared movement)
#   4. the jump / fall / air stats of the movement preset (setup_qa_movement_1117.PRESET_STATS: JumpVelocity 700 -> 1240 and the rest)
# Headless: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<proj>/Tools/GAS/setup_overlord_kit_shared_f4.py" -unattended -nullrhi
# Output: Saved/PlaytestFixes/setup_overlord_kit_shared_f4.txt, at most 5 printed lines.
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_qa_movement_1117 as qam  # noqa: E402

SRC = '/Game/GSTN/Characters/Overlord/Kits/KIT_Test_Playtest'
DST = '/Game/GSTN/Characters/Overlord/Kits/KIT_Test_Overlord'
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'PlaytestFixes', 'setup_overlord_kit_shared_f4.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[overlord_kit_f4] {msg}')


def _entries(text):
    """Top-level entries of ExtensionData=(...) in an exported FGSTN_KitExtensions text"""
    start = text.index('ExtensionData=(') + len('ExtensionData=(')
    depth, cur, out = 0, '', []
    for ch in text[start:]:
        if ch == '(':
            depth += 1
        elif ch == ')':
            if depth == 0:
                break
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
            continue
        cur += ch
    if cur:
        out.append(cur)
    return out


def _binding_key(b):
    return b.export_text()


def run():
    src, dst = unreal.load_asset(SRC), unreal.load_asset(DST)
    # 1. defence extension
    ext = dst.get_editor_property('extensions')
    have = _entries(ext.export_text())
    if any('GSTN_DefenceKitExtension' in e for e in have):
        log('defence extension: already there')
    else:
        defence = [e for e in _entries(src.get_editor_property('extensions').export_text()) if 'GSTN_DefenceKitExtension' in e]
        new_text = '(ExtensionData=(' + ','.join(have + defence) + '))'
        err = ext.import_text(new_text)
        dst.set_editor_property('extensions', ext)
        back = dst.get_editor_property('extensions').export_text()
        log(f'defence extension added: {"OK" if "DA_DefenceRules_Test" in back else "FAILED " + str(err)}; entries now {len(_entries(back))}')
    # 2. triggered abilities
    trig = list(dst.get_editor_property('triggered_abilities'))
    for cls in src.get_editor_property('triggered_abilities'):
        if cls not in trig:
            trig.append(cls)
            log(f'triggered ability added: {cls.get_name()}')
    dst.set_editor_property('triggered_abilities', trig)
    # 3. bindings
    binds = list(dst.get_editor_property('kit_bindings'))
    keys = {_binding_key(b) for b in binds}
    added = [b for b in src.get_editor_property('kit_bindings') if _binding_key(b) not in keys]
    if added:
        dst.set_editor_property('kit_bindings', binds + added)
    log(f'kit bindings added: {len(added)} ({", ".join(sorted({str(b.get_editor_property("command").get_editor_property("tag_name")) for b in added}))})')
    unreal.EditorAssetLibrary.save_loaded_asset(dst, False)
    # 4. movement preset stats (the QA script's own logic, pointed at this kit)
    qam.KIT = DST
    qam.run()
    log('preset stats: setup_qa_movement_1117.run() on ' + DST + ' (its log: Saved/QAM or its own output)')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w', encoding='utf-8').write('\n'.join(_lines))
    for line in _lines[:5]:
        print(line)


run()
