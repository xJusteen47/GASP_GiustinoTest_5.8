# LANCI-1A2A (2026-11-24), read only: which knockback model GSTN_CombatRules_Standard plays and its reaction tuning (KnockbackGravityScale,
# LaunchDeceleration, thresholds). Headless: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<proj>/Tools/GAS/lanci_rules_check.py" -unattended -nullrhi
# Out: Saved/Lanci/rules_check.log
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Lanci')
os.makedirs(OUT, exist_ok=True)
lines = []
rules = unreal.load_asset('/GSTNCombat/Rules/GSTN_CombatRules_Standard')
model = rules.get_editor_property('knockback_model')
lines.append(f'rules: {rules.get_path_name()}')
lines.append(f'knockback model: {model.get_class().get_name() if model else None}')
if model:
    for prop in ('tuning', 'percent_growth', 'weight_curve'):
        try:
            val = model.get_editor_property(prop)
        except Exception as e:  # noqa: BLE001
            continue
        if prop == 'tuning':
            names = [n for n in dir(val) if not n.startswith('_') and n not in ('cast', 'copy', 'assign', 'export_text', 'import_text',
                                                                                  'get_editor_property', 'set_editor_property', 'to_tuple',
                                                                                  'set_editor_properties', 'static_struct')]
            for n in names:
                try:
                    lines.append(f'  tuning.{n} = {val.get_editor_property(n)}')
                except Exception:  # noqa: BLE001
                    pass
        else:
            lines.append(f'  {prop} = {val}')
with open(os.path.join(OUT, 'rules_check.log'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')
for ln in lines[:3]:
    unreal.log(f'[lanci_rules] {ln}')
