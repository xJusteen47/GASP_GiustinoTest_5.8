"""Land recovery time from a PIE capture (land-v2-1): reads <capture>/<sequence>/frames.jsonl of pie_capture_move.py.

  land     = first row where the sim jump phase goes back to 0 after the air (combat.jump.phase >= 1 -> 0)
  recovery = first row after the land whose heaviest clip (combat.anim, heaviest first, lean blend spaces BS_* skipped) is neither
             the standing land (name with "Land" and "Stand") nor a jump take-off/fall clip: the state machine has handed the pose to
             idle or to locomotion. A run/walk land (Land_Run_*, Land_Walk_*: GASP's land WITH input, it runs from the first frame) counts
             as locomotion.
Rows are rendered 60 fps frames, so ms = frames * 1000 / 60.
    python land_recovery.py <capture_dir> <sequence> [<sequence> ...]
Prints one line per sequence (max 5).
"""
import json
import os
import sys


def dominant(row):
    for e in (row.get('combat') or {}).get('anim') or []:
        name = str(e).rsplit(' ', 2)[0]
        if not name.startswith('BS_'):
            return name
    return ''


def measure(cap, seq):
    rows = [json.loads(l) for l in open(os.path.join(cap, seq, 'frames.jsonl'), encoding='utf-8') if l.strip()]
    air, land = False, None
    for r in rows:
        ph = ((r.get('combat') or {}).get('jump') or {}).get('phase', 0)
        if ph and ph >= 1:
            air = True
        elif air and land is None:
            land = r['f']
    if land is None:
        return f'{seq}: no land in the capture'
    for r in rows:
        if r['f'] <= land:
            continue
        d = dominant(r)
        lo = d.lower()
        if d and not ('land' in lo and 'stand' in lo) and not ('jump' in lo and 'land' not in lo):
            fr = r['f'] - land
            return f'{seq}: land v{land} -> {d} owns the pose v{r["f"]} = {fr} frames = {fr * 1000 / 60:.0f} ms'
    fr = rows[-1]['f'] - land
    return f'{seq}: land v{land} -> still a land/jump clip at the end v{rows[-1]["f"]} = > {fr * 1000 / 60:.0f} ms'


if __name__ == '__main__':
    for s in sys.argv[2:][:5]:
        print(measure(sys.argv[1], s))
