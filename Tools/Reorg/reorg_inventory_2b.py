"""Phase 2b, step 1 on the desktop: list every asset still under /Game/GASNPPTest (tracked or not) and where it goes.

Git only knows the versioned files (Tools/Reorg/content-map-2b.csv, 276 rows). Threads also left assets on the desktop that were
never committed (Combat/Editor, DA_Minion_*, TestMaps/Level01, Camera...). This script asks the Asset Registry, applies the same
rules as the setup scripts (Tools/GAS/gstn_content.py) and appends the missing rows to the CSV, so reorg_apply.py moves them too.
A name no rule covers is NOT added: it is listed as NO RULE and the thread decides (add a rule, or leave it in _Archive).

Run headless: UnrealEditor-Cmd.exe GASP_Giustino2.uproject -run=pythonscript -script="<abs path>/Tools/Reorg/reorg_inventory_2b.py"
Env REORG_DRY=1: only report. Writes Saved/Logs/ci/reorg_inventory_2b.txt, prints one line.
"""
import csv
import os
import sys

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(ROOT, 'Tools', 'GAS'))
import gstn_content as gc  # noqa: E402

MAP = os.path.join(ROOT, 'Tools', 'Reorg', 'content-map-2b.csv')
OUT = os.path.join(ROOT, 'Saved', 'Logs', 'ci', 'reorg_inventory_2b.txt')
DRY = os.environ.get('REORG_DRY') == '1'

with open(MAP, newline='', encoding='utf-8') as f:
    known = {r['old'].strip() for r in csv.DictReader(f)}

ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Game/GASNPPTest'], True)
packages = sorted({str(d.package_name) for d in ar.get_assets_by_path('/Game/GASNPPTest', recursive=True)})

added, no_rule, lines = [], [], []
for pkg in packages:
    if pkg in known:
        continue
    name = pkg.rsplit('/', 1)[1]
    # An old folder with a fixed destination (Combat/Editor, TestMaps/Level01...) keeps its whole subtree; else the name rules.
    moved_folder = next((new + pkg[len(old):] for old, new in sorted(gc.FOLDERS.items(), key=lambda p: -len(p[0]))
                         if pkg.startswith(old + '/')), None)
    try:
        added.append((pkg, moved_folder or gc.path(name), 'owner' if name == 'DA_Locomotion_Default' else ''))
        lines.append(f'NUOVA RIGA {pkg} -> {added[-1][1]}')
    except KeyError:
        no_rule.append(pkg)
        lines.append(f'NO RULE {pkg}')

if added and not DRY:
    with open(MAP, 'a', newline='', encoding='utf-8') as f:
        csv.writer(f, lineterminator='\n').writerows(added)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
summary = f'INVENTARIO 2b: {len(packages)} asset in /Game/GASNPPTest, {len(added)} fuori da git aggiunti{" (dry)" if DRY else ""}, {len(no_rule)} senza regola'
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines + [summary]) + '\n')
print(summary + f' (dettagli {OUT})')
