"""Move the assets listed in Tools/Reorg/content-map.csv and fix up the redirectors (idempotent).

Run headless (no map loaded, so maps can move too):
  UnrealEditor-Cmd.exe GASP_Giustino2.uproject -run=pythonscript -script="<abs path>/Tools/Reorg/reorg_apply.py"
Optional env: REORG_MAP (csv path), REORG_DRY=1 (only report), REORG_OWNER=1 (also the rows marked quando=owner: the owner's
local files, moved only with the owner present, decision 3A).
Writes Saved/Logs/ci/reorg_apply.txt and prints at most 5 lines.
"""
import csv
import os
import re

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
MAP = os.environ.get('REORG_MAP', os.path.join(ROOT, 'Tools', 'Reorg', 'content-map.csv'))
DRY = os.environ.get('REORG_DRY') == '1'
OUT = os.path.join(ROOT, 'Saved', 'Logs', 'ci', 'reorg_apply.txt')
OWNER = os.environ.get('REORG_OWNER') == '1'

eal = unreal.EditorAssetLibrary
lines = []
moved = skipped = failed = 0

with open(MAP, newline='', encoding='utf-8') as f:
    rows = list(csv.DictReader(f))

ar = unreal.AssetRegistryHelpers.get_asset_registry()


def exists(path):
    # A package copied on disk keeps its old asset name in the registry (DefaultLevel_Harness reports 'DefaultLevel'),
    # so does_asset_exist misses it: fall back to the package.
    # A redirector left at the old path of a done row is not the asset: without this a re-run reports CONFLITTO.
    data = ar.get_assets_by_package_name(path)
    if data and all(str(d.asset_class_path.asset_name) == 'ObjectRedirector' for d in data):
        return False
    return eal.does_asset_exist(path) or bool(data)


def rename(old, new):
    if eal.rename_asset(old, new):
        return True
    obj = unreal.load_asset(old)
    if not obj:
        return False
    d, n = new.rsplit('/', 1)
    return unreal.AssetToolsHelpers.get_asset_tools().rename_assets([unreal.AssetRenameData(obj, d, n)])


for r in rows:
    old, new = r['old'].strip(), r['new'].strip()
    if (r.get('quando') or '').strip() == 'owner' and not OWNER:
        skipped += 1
        lines.append(f'ASPETTA OWNER {old}')
        continue
    old_exists, new_exists = exists(old), exists(new)
    if new_exists and not old_exists:
        skipped += 1
        lines.append(f'GIA FATTO {old} -> {new}')
        continue
    if not old_exists:
        failed += 1
        lines.append(f'MANCA {old}')
        continue
    if new_exists:
        failed += 1
        lines.append(f'CONFLITTO {new} esiste gia')
        continue
    if DRY:
        lines.append(f'DRY {old} -> {new}')
        continue
    ok = rename(old, new)
    if ok:
        moved += 1
        lines.append(f'OK {old} -> {new}')
    else:
        failed += 1
        lines.append(f'FALLITO {old} -> {new}')

# Fix up the redirectors left at the old paths of the map.
# IAssetTools::FixupReferencers is not exposed to Python and DeleteAsset cannot load a redirector, so: resave every
# referencer (its load follows the redirector to the new path), then remove the redirector package once nothing points at it.
fixed = 0
if not DRY:
    olds = {r['old'].strip() for r in rows}
    resaved = set()
    opts = unreal.AssetRegistryDependencyOptions()
    for pkg in sorted(olds):
        data = ar.get_assets_by_package_name(pkg)
        if not data or str(data[0].asset_class_path.asset_name) != 'ObjectRedirector':
            continue
        for ref in [str(x) for x in (ar.get_referencers(pkg, opts) or [])]:
            if ref in resaved or ref in olds or not ref.startswith('/'):
                continue
            if re.search(r'/(DA_Locomotion_Default|CHT_\w*_PoseMatch)$', ref):
                lines.append(f'FILE LOCALE OWNER, NON RISALVATO {ref}')  # protected-paths.json: only with the owner present
                continue
            if eal.save_asset(ref, only_if_is_dirty=False):
                resaved.add(ref)
                lines.append(f'RISALVATO {ref}')
            else:
                lines.append(f'NON SALVATO {ref}')
        left = [str(x) for x in (ar.get_referencers(pkg, opts) or []) if str(x) not in olds and str(x) not in resaved]
        if left:
            lines.append(f'REDIRECTOR RESTA {pkg}: {left}')
            continue
        rel = pkg.replace('/Game/', 'Content/', 1)
        for ext in ('.uasset', '.umap'):
            f = os.path.join(ROOT, rel + ext)
            if os.path.exists(f):
                try:
                    os.remove(f)
                except PermissionError:
                    # The commandlet itself keeps the redirector package open once loaded: the caller deletes it on disk
                    # after the editor exits (every file listed here was checked to have no referencer left).
                    lines.append(f'REDIRECTOR DA TOGLIERE {f}')
                    continue
                fixed += 1
                lines.append(f'REDIRECTOR TOLTO {pkg}')
    for d in sorted({r['new'].strip().rsplit('/', 1)[0] for r in rows}):
        eal.save_directory(d, only_if_is_dirty=True, recursive=False)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
    f.write(f'\nRIASSUNTO: spostati {moved}, gia fatti {skipped}, falliti {failed}, redirector sistemati {fixed}\n')
print(f'REORG: spostati {moved}, gia fatti {skipped}, falliti {failed}, redirector {fixed} (dettagli {OUT})')
