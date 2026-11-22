# Read-only: where Command.Movement.RunBoost is (and is not) among the kits' command sources. Writes Saved/Dash/inspect_slots.txt
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Dash', 'inspect_slots.txt')
lines = []
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for path in ('/Game/GASNPPTest',):
    for d in reg.get_assets_by_path(path, recursive=True):
        name = str(d.asset_name)
        if not (name.startswith('KIT_') or name.startswith('SS_') or name.startswith('BS_') or name.startswith('IC_')):
            continue
        a = d.get_asset()
        try:
            txt = a.export_text() if hasattr(a, 'export_text') else ''
        except Exception:  # noqa: BLE001
            txt = ''
        props = []
        for n in dir(a):
            if n.startswith('_') or not any(k in n.lower() for k in ('slot', 'command', 'binding', 'table', 'input')):
                continue
            try:
                v = a.get_editor_property(n)
            except Exception:  # noqa: BLE001
                continue
            s = v.export_text() if hasattr(v, 'export_text') else str(v)
            props.append((n, s))
        hit = [n for n, s in props if 'RunBoost' in s]
        lines.append(f'{d.package_name} ({a.get_class().get_name()}): RunBoost in {hit or "-"}; props ' +
                     '; '.join(f'{n}={s[:160]}' for n, s in props))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')
