"""getup-momentum-1: review mp4s (get-up input -10 .. +70) and one sheet per view (get-up +12 .. +41, around FAF) for every get-up capture.
    python Tools/GASNPPTest/getup_review_media.py <out_dir> <capture_dir>:<tag> [<capture_dir>:<tag> ...]
Needs ffmpeg on PATH (pie_capture_grid.py). Prints one line."""
import json
import os
import subprocess
import sys

GRID = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pie_capture_grid.py')
out = sys.argv[1]
n = 0
for job in sys.argv[2:]:
    cap, tag = job.rsplit(':', 1)
    for seq in sorted(os.listdir(cap)):
        rows_path = os.path.join(cap, seq, 'frames.jsonl')
        if not os.path.exists(rows_path):
            continue
        rows = [json.loads(line) for line in open(rows_path)]
        gu = next((r['f'] for r in rows if 'video +' in r.get('tap', '')), None)
        if gu is None:
            continue
        folder = os.path.join(out, tag)
        os.makedirs(folder, exist_ok=True)
        subprocess.run([sys.executable, GRID, 'video', cap, seq, os.path.join(folder, seq + '.mp4'), f'--from={gu - 10}', f'--to={gu + 70}'],
                       capture_output=True)
        for view in ('az180', 'az90'):
            subprocess.run([sys.executable, GRID, 'sheet', cap, seq, view, os.path.join(folder, f'{seq}_{view}.png'), f'--from={gu + 12}',
                            f'--to={gu + 41}', '--cols=6', '--cell=200'], capture_output=True)
        n += 1
print(f'{n} sequences -> {out}')
