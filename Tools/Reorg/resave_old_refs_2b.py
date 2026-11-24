"""Resave every asset that still names a /Game/GASNPPTest package moved by phase 2b, so it stores the new path.

An asset made on a branch from before the move (e.g. the greybox capsule's ABP_GSTN_GreyboxCapsule, merged after the move) points
at the old packages. Config/DefaultEngine.ini [CoreRedirects] maps them to the new ones at load; a resave writes the new path
into the file. Never resaves the owner's local files (DA_Locomotion_Default, CHT_*_PoseMatch), and skips anything under
/Game/GASNPPTest itself.

Run headless: UnrealEditor-Cmd.exe GASP_Giustino2.uproject -run=pythonscript -script="<abs>/Tools/Reorg/resave_old_refs_2b.py"
Env REORG_DRY=1: only report. Writes Saved/Logs/ci/resave_old_refs_2b.txt, prints one line.
"""
import csv
import os
import re

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
MAP = os.path.join(ROOT, 'Tools', 'Reorg', 'content-map-2b.csv')
OUT = os.path.join(ROOT, 'Saved', 'Logs', 'ci', 'resave_old_refs_2b.txt')
DRY = os.environ.get('REORG_DRY') == '1'
OWNER = re.compile(r'/(DA_Locomotion_Default|CHT_\w*_PoseMatch)$')

with open(MAP, newline='', encoding='utf-8') as f:
    moved = {r['old'].strip() for r in csv.DictReader(f) if (r.get('quando') or '').strip() != 'owner'}

ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Game'], True)
opts = unreal.AssetRegistryDependencyOptions()
lines, saved, failed = [], 0, 0
for data in ar.get_assets_by_path('/Game', recursive=True):
    pkg = str(data.package_name)
    if pkg.startswith('/Game/GASNPPTest/') or OWNER.search(pkg):
        continue
    old = sorted(str(d) for d in (ar.get_dependencies(pkg, opts) or []) if str(d) in moved)
    if not old:
        continue
    if DRY:
        lines.append(f'DRY {pkg}: {old[:3]}')
        continue
    if unreal.EditorAssetLibrary.load_asset(pkg) and unreal.EditorAssetLibrary.save_asset(pkg, only_if_is_dirty=False):
        saved += 1
        lines.append(f'RISALVATO {pkg} ({len(old)} vecchi percorsi)')
    else:
        failed += 1
        lines.append(f'NON SALVATO {pkg}')

summary = f'RESAVE 2b: {saved} risalvati, {failed} falliti, {sum(l.startswith("DRY") for l in lines)} da fare (dry)'
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines + [summary]) + '\n')
print(summary + f' (dettagli {OUT})')
