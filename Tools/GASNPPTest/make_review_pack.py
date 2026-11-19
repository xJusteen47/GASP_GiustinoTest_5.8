"""python make_review_pack.py <capture_dir> <out_dir> <prefix> [seq,...]  : per sequence an mp4 (7 views) and one grid per view (side az90, behind az180, front 3/4 az45, game)
into out_dir; the frame range is the sequence's rows (first tap - 6 to the last row), at most 40 cells per grid (step grows)."""
import json, os, subprocess, sys
cap, out, prefix = sys.argv[1:4]
grid = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pie_capture_grid.py')
seqs = sys.argv[4].split(',') if len(sys.argv) > 4 else sorted(d for d in os.listdir(cap) if os.path.isdir(os.path.join(cap, d)) and os.path.exists(os.path.join(cap, d, 'frames.jsonl')))
os.makedirs(out, exist_ok=True)
for s in seqs:
    rows = [json.loads(l) for l in open(os.path.join(cap, s, 'frames.jsonl')) if l.strip()]
    taps = [r['f'] for r in rows if r.get('tap')]
    a, b = max(0, (taps[0] if taps else 20) - 6), rows[-1]['f']
    step = max(1, -(-(b - a) // 40))
    name = f'{prefix}_{s}'
    subprocess.run([sys.executable, grid, 'video', cap, s, os.path.join(out, name + '.mp4'), f'--from={a}', f'--to={b}'], capture_output=True)
    for view, label in (('az90', 'side'), ('az180', 'behind'), ('az45', 'front34'), ('game', 'game')):
        subprocess.run([sys.executable, grid, 'sheet', cap, s, view, os.path.join(out, f'{name}_{label}.jpg'), f'--from={a}', f'--to={b}', '--cols=8', '--cell=220', f'--step={step}'], capture_output=True)
    print(name, a, b, 'step', step)
