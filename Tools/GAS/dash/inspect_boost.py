# Read-only: dumps what the L3 run boost depends on (kit locomotion entry, profile run stages, input config button/command) to
# Saved/Dash/inspect_boost.txt. Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file>
import os
import re
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Dash', 'inspect_boost.txt')
lines = []


def grab(label, text, pat):
    for m in re.finditer(pat, text):
        s = max(0, m.start() - 80)
        lines.append(f'{label}: ...{text[s:m.end() + 220]}...')


kit = unreal.load_asset('/Game/GASNPPTest/Combat/KIT_Test_Playtest')
for prop in ('extensions', 'kit_extensions'):
    try:
        ext = kit.get_editor_property(prop)
        lines.append(f'kit.{prop}: {len(ext)} entries')
        for e in ext:
            lines.append('  ' + str(e)[:400])
    except Exception as ex:  # noqa: BLE001
        lines.append(f'kit.{prop}: {ex!r}')
prof = unreal.load_asset('/Game/GASNPPTest/Combat/DA_Locomotion_Default')
rs = prof.get_editor_property('run_stages')
for name in ('boost_command', 'boost_mode', 'boost_window_frames', 'boost_target_stage', 'boost_dash', 'stick_deadzone', 'stages',
             'start_stamp'):
    try:
        v = rs.get_editor_property(name)
        lines.append(f'run_stages.{name} = {str(v)[:300]}')
    except Exception as ex:  # noqa: BLE001
        lines.append(f'run_stages.{name}: {ex!r}')
ic = unreal.load_asset('/Game/GASNPPTest/Input/IC_GASNPP_Test')
txt = unreal.SystemLibrary.get_object_name(ic)
for p in ('buttons', 'commands', 'command_set', 'presses'):
    try:
        v = ic.get_editor_property(p)
        s = str(v)
        lines.append(f'ic.{p}: len {len(v) if hasattr(v, "__len__") else "?"}')
        grab('ic.' + p, s, 'RunBoost')
        if p == 'buttons':
            lines.append('  order: ' + ', '.join(str(b.get_editor_property('tag').get_editor_property('tag_name')) for b in v))
    except Exception as ex:  # noqa: BLE001
        lines.append(f'ic.{p}: {ex!r}')
lines.append('boost_command tag_name = ' + str(rs.get_editor_property('boost_command').get_editor_property('tag_name')))
lines.append('kit ext export: ' + kit.get_editor_property('extensions').export_text()[:1500])
lines.append('ic props: ' + ', '.join(n for n in dir(ic) if not n.startswith('_'))[:1500])
for n in dir(ic):
    if 'command' in n.lower() or 'press' in n.lower() or 'slot' in n.lower():
        try:
            v = ic.get_editor_property(n)
            s = v.export_text() if hasattr(v, 'export_text') else str(v)
            lines.append(f'ic.{n}: ' + s[:300])
            grab('ic.' + n, s, 'RunBoost')
        except Exception as ex:  # noqa: BLE001
            lines.append(f'ic.{n}: {ex!r}')
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')
