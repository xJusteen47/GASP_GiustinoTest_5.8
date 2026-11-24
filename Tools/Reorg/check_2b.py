"""Phase 2b consistency check, plain Python (no editor): the move map and the setup scripts agree, and no live text still names
/Game/GASNPPTest. Run before and after the desktop move: python Tools/Reorg/check_2b.py   (exit 1 on a problem, at most 10 lines)
"""
import csv
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'Tools', 'GAS'))
import gstn_content as gc  # noqa: E402

problems = []
with open(os.path.join(ROOT, 'Tools', 'Reorg', 'content-map-2b.csv'), newline='', encoding='utf-8') as f:
    rows = list(csv.DictReader(f))
FOLDERS = sorted(gc.FOLDERS.items(), key=lambda p: -len(p[0]))
for r in rows:
    old, name = r['old'], r['old'].rsplit('/', 1)[1]
    # Same choice as reorg_inventory_2b: a subtree with a fixed destination, else the name rule.
    by_folder = next((new + old[len(o):] for o, new in FOLDERS if old.startswith(o + '/')), None)
    if r['new'] not in (gc.path(name), by_folder):
        problems.append(f'MAPPA {name}: csv {r["new"]} ma gstn_content {gc.path(name)}')
if len({r['new'] for r in rows}) != len(rows):
    problems.append('MAPPA: due righe con la stessa destinazione')

# History (docs/notes, handoffs, Artifacts, plans) keeps the old paths on purpose; the reorg tools name them by design.
SKIP = ('docs/notes/', 'docs/architecture/plans/', 'handoffs/', 'Artifacts/', 'Tools/Reorg/', 'Tools/GAS/gstn_content.py',
        'Tools/CI/Test-GitGuard.ps1', 'Content/')
out = subprocess.run(['git', 'grep', '-n', '-I', '/Game/GASNPPTest'], cwd=ROOT, capture_output=True, text=True).stdout.splitlines()
# [CoreRedirects] PackageRedirects in DefaultEngine.ini name the old packages by design (old -> new, kept for pre-move branches).
problems += [f'TESTO {line[:160]}' for line in out
             if not line.startswith(SKIP) and not (line.startswith('Config/DefaultEngine.ini:') and ('+PackageRedirects=(OldName="/Game/GASNPPTest/' in line or line.split(':', 2)[2].lstrip().startswith(';')))]

print('\n'.join(problems[:10]))
print(f'CHECK 2b: {len(rows)} righe, {len(problems)} problemi')
sys.exit(1 if problems else 0)
