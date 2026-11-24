# capsula-a-terra (2026-11-24): read-only dump of the data behind the dropkick's landing hop on the kit a capture really uses.
# For KIT (env DUMP_KIT, default KIT_Test_Playtest): every binding whose ability mentions "Dropkick", the timeline the ability plays (its
# timeline asset, else its inline timeline), that timeline's Self Landing Bounce track (Mode, Height, BounceAbility), its
# Combat.Move.KnockdownOnLanding window and its landing tracks (Landing, Hold Until Landed); plus the kit's defence rules (GroundBounce block:
# GroundBounceGravityScale, SelfKnockdownImpactSpeed, heights). Nothing is saved.
# Output: Saved/Greybox/dump_dropkick_bounce.txt, then "ESITO: OK|FAIL" and QUIT_EDITOR (run under Tools/CI/Run-Timed.ps1 -Preset Capture,
# booted through a one-line %TEMP% file because of the space in the project path).
import os
import re
import traceback

import unreal

KIT_NAME = os.environ.get('DUMP_KIT', 'KIT_Test_Playtest')
KIT = f'/Game/GSTN/Characters/Overlord/Kits/{KIT_NAME}'
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Greybox', 'dump_dropkick_bounce.txt')
LANDING_KEYS = ('SelfLandingBounce', 'KnockdownOnLanding', 'TimelineTrack_Landing', 'HoldUntilLanded', 'GroundRule')
lines = []


def log(msg):
    lines.append(str(msg))


def _props(obj, word):
    """Editor property names of obj whose name contains word (from the class's exported text keys)"""
    try:
        text = obj.export_text() if hasattr(obj, 'export_text') else ''
    except Exception:  # noqa: BLE001
        text = ''
    return sorted({k for k in re.findall(r'(\w+)=', text) if word.lower() in k.lower()})


def _timelines(cls):
    """[(where, exported timeline data text)] for the timeline asset and the inline timeline of an ability class"""
    cdo = unreal.get_default_object(cls)
    out = []
    for prop in ('timeline_asset', 'timeline'):
        try:
            v = cdo.get_editor_property(prop)
        except Exception as e:  # noqa: BLE001
            out.append((f'{prop}: {e}', ''))
            continue
        if v is None:
            out.append((f'{prop}=None', ''))
        elif isinstance(v, unreal.Object):
            out.append((f'{prop}={v.get_path_name()}', v.get_editor_property('data').export_text()))
        else:
            out.append((f'{prop} (inline)', v.export_text() if hasattr(v, 'export_text') else str(v)))
    return out


def _dump_timeline(where, text):
    tf = re.search(r'TotalFrames=(\d+)', text)
    log(f'  {where}: total frames {tf.group(1) if tf else "?"}, {len(text)} chars')
    if not text:
        return
    for key in LANDING_KEYS:
        found = False
        for mm in re.finditer(re.escape(key), text):
            found = True
            start = text.rfind('/Script/', 0, mm.start())
            start = mm.start() - 150 if start < 0 or mm.start() - start > 400 else start
            log(f'    {key}: {text[start:mm.end() + 350]}')
        if not found:
            log(f'    {key}: none')


try:
    kit = unreal.load_asset(KIT)
    log(f'kit {KIT}')
    arrays = [('kit', kit.get_editor_property('kit_bindings'))]
    weapon = kit.get_editor_property('primary_weapon')
    w = weapon.load_synchronous() if weapon is not None and hasattr(weapon, 'load_synchronous') else weapon
    if w:
        log(f'primary weapon {w.get_path_name()}')
        arrays.append(('weapon', w.get_editor_property('bindings')))
    seen = set()
    for src, arr in arrays:
        for b in arr:
            try:
                ab = b.get_editor_property('ability')
            except Exception:  # noqa: BLE001
                continue
            cls = ab.load_synchronous() if hasattr(ab, 'load_synchronous') else ab
            if cls is None or 'Dropkick' not in cls.get_name() or cls.get_name() in seen:
                continue
            seen.add(cls.get_name())
            cmd = re.search(r'Command=\(TagName="([^"]+)"', b.export_text())
            log(f'{src} binding {cls.get_name()} ({cls.get_path_name()}), command {cmd.group(1) if cmd else "?"}')
            log(f'  timeline-like properties of the CDO: {_props(unreal.get_default_object(cls), "timeline")}')
            for where, text in _timelines(cls):
                _dump_timeline(where, text)
    tl = unreal.load_asset('/Game/GSTN/Characters/Overlord/Moves/Dropkick/TL_Overlord_Dropkick')
    if tl:
        log('direct TL_Overlord_Dropkick:')
        _dump_timeline('TL_Overlord_Dropkick.data', tl.get_editor_property('data').export_text())

    ext = kit.get_editor_property('extensions').export_text()
    m = re.search(r'Rules="?([^",)]+)', ext)
    log(f'defence rules ref: {m.group(1) if m else "none"}')
    if m and m.group(1) not in ('None', ''):
        ref = m.group(1).strip("'\"")
        path = ref.split("'")[1] if "'" in ref else ref
        rules = unreal.load_asset(path)
        log(f'  rules {rules.get_path_name()} ({rules.get_class().get_name()})')
        gb = None
        for name in ['ground_bounce'] + [n for n in dir(rules) if 'bounce' in n.lower() or n in ('rules', 'defence_rules')]:
            try:
                v = rules.get_editor_property(name)
            except Exception:  # noqa: BLE001
                continue
            try:
                v = v.get_editor_property('ground_bounce')
            except Exception:  # noqa: BLE001
                pass
            gb = v
            log(f'  GroundBounce read from property {name}')
            break
        if gb is None:
            log(f'  no ground bounce property; names: {[n for n in dir(rules) if not n.startswith("_")]}')
        else:
            log(f'  GroundBounce: {gb.export_text() if hasattr(gb, "export_text") else gb}'[:1500])
    status = 'OK'
except Exception:  # noqa: BLE001
    log(traceback.format_exc())
    status = 'FAIL'
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')
unreal.log('ESITO: ' + status)
unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')
