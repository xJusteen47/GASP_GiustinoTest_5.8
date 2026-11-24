"""Rewrite /Game/... paths in text files (C++, ini, Tools, plugins) from Tools/Reorg/content-map.csv.

Redirectors only fix asset-to-asset references; paths written in code, config and scripts are fixed here.
Docs, handoffs and Artifacts are history and stay as they are. EULA folders (Plugins/GASNPP, Plugins/EngineOverrides) are skipped.
Usage: python Tools/Reorg/reorg_rewrite.py [--dry]   (prints at most 5 lines; full list in Saved/Logs/ci/reorg_rewrite.txt)
"""
import csv
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MAP = os.path.join(ROOT, 'Tools', 'Reorg', 'content-map.csv')
OUT = os.path.join(ROOT, 'Saved', 'Logs', 'ci', 'reorg_rewrite.txt')
DRY = '--dry' in sys.argv
SKIP_PREFIX = ('docs/', 'handoffs/', 'Artifacts/', 'Plugins/GASNPP/', 'Plugins/EngineOverrides/', 'Tools/Reorg/content-map.csv')
EXT = ('.py', '.ps1', '.cpp', '.h', '.ini', '.cs', '.json', '.md', '.txt', '.bat', '.yml', '.yaml')

pairs = []
with open(MAP, newline='', encoding='utf-8') as f:
    for r in csv.DictReader(f):
        pairs.append((r['old'].strip(), r['new'].strip()))
# folders whose content moved as a whole (subpaths follow)
pairs += [
    ('/Game/Blueprints/Animation/Ported57', '/Game/GSTN/Animation/Ported57'),
    ('/Game/Blueprints/Animation/RunStop', '/Game/GSTN/Animation/RunStop'),
    ('/Game/Blueprints/DepthMarker', '/Game/GSTN/DepthMarker'),
]
# bare folder variables in one-shot setup scripts (file -> (old, new))
BARE = {
    'Tools/GAS/root_fix_skid_180.py': ("ANIM = '/Game/Blueprints/Animation'", "ANIM = '/Game/GSTN/Animation'"),
    'Tools/GAS/run_skid_root180.py': ("'/Game/Blueprints/Animation'", "'/Game/GSTN/Animation/Layers'"),
    'Tools/GAS/setup_wall_anim_layer.py': ("ANIM_DIR = '/Game/Blueprints/Animation'", "ANIM_DIR = '/Game/GSTN/Animation/Layers'"),
    'Tools/GAS/setup_locomotion_profile.py': ('(assets in /Game/Blueprints/Animation)', '(assets in /Game/GSTN/Animation/Layers)'),
    'Tools/GAS/setup_skid_anim_layer.py': ('(assets in /Game/Blueprints/Animation)', '(assets in /Game/GSTN/Animation/Layers)'),
    'Tools/GASNPPTest/camera_anchor_setup.py': ("DA_DIR = '/Game/Blueprints/Data'", "DA_DIR = '/Game/GSTN/Data/Camera'"),
    'Tools/GASNPPTest/ground_contact_setup.py': ("DA_DIR = '/Game/Blueprints/Data'", "DA_DIR = '/Game/GSTN/Data/Movement'"),
}
pairs.sort(key=lambda p: -len(p[0]))  # longest first: DA_AnimSet_Wall_Heavy before DA_AnimSet_Wall
rx = re.compile('(' + '|'.join(re.escape(o) for o, _ in pairs) + r')(?![A-Za-z0-9_])')
lookup = dict(pairs)

files = subprocess.run(['git', 'ls-files'], cwd=ROOT, capture_output=True, text=True, check=True).stdout.splitlines()
changed, total = [], 0
for rel in files:
    if rel.startswith(SKIP_PREFIX) or not rel.endswith(EXT):
        continue
    path = os.path.join(ROOT, rel)
    try:
        with open(path, encoding='utf-8', newline='') as f:
            text = f.read()
    except (UnicodeDecodeError, FileNotFoundError):
        continue
    new, n = rx.subn(lambda m: lookup[m.group(1)], text)
    if rel in BARE and BARE[rel][0] in new:
        new = new.replace(BARE[rel][0], BARE[rel][1])
        n += 1
    if n:
        changed.append(f'{rel}: {n}')
        total += n
        if not DRY:
            with open(path, 'w', encoding='utf-8', newline='') as f:
                f.write(new)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(changed) + f'\nTOTALE: {len(changed)} file, {total} sostituzioni{" (dry)" if DRY else ""}\n')
print(f'REWRITE: {len(changed)} file, {total} sostituzioni{" (dry)" if DRY else ""} (dettagli {OUT})')
