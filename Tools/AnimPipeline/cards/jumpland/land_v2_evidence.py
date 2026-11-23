"""Land v2 evidence (land-v2-1): one grid per view with base (stock, top) and v2 (bottom) on the SAME capture frames, plus the mp4s.
    python land_v2_evidence.py <base_cap> <v2_cap> <out_dir> [--from=100 --to=196 --step=6]
Uses Tools/GASNPPTest/pie_capture_grid.py (Pillow + ffmpeg). Prints <= 5 lines."""
import os
import subprocess
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
GRID = os.path.join(HERE, '..', '..', '..', 'GASNPPTest', 'pie_capture_grid.py')
base, v2, out = sys.argv[1:4]
opt = dict(a[2:].split('=', 1) for a in sys.argv[4:] if a.startswith('--'))
f0, f1, step = int(opt.get('from', 100)), int(opt.get('to', 196)), int(opt.get('step', 6))
cols = (f1 - f0) // step + 1
os.makedirs(out, exist_ok=True)
tmp = os.path.join(out, '_tmp')
os.makedirs(tmp, exist_ok=True)
for view in ('az90', 'az45', 'game'):
    imgs = []
    for tag, cap in (('base', base), ('v2', v2)):
        p = os.path.join(tmp, f'{tag}_{view}.png')
        subprocess.run([sys.executable, GRID, 'sheet', cap, 'ld_jump_idle', view, p, f'--from={f0}', f'--to={f1}', f'--step={step}',
                        f'--cols={cols}', '--cell=200'], check=True, capture_output=True)
        imgs.append((tag, Image.open(p).convert('RGB')))
    w = max(i.width for _, i in imgs)
    sheet = Image.new('RGB', (w + 70, sum(i.height for _, i in imgs)), 'white')
    y = 0
    for tag, i in imgs:
        sheet.paste(i, (70, y))
        ImageDraw.Draw(sheet).text((8, y + i.height // 2), tag.upper(), fill='black')
        y += i.height
    sheet.save(os.path.join(out, f'grid_{view}.png'))
for tag, cap, seq in (('base', base, 'ld_jump_idle'), ('v2', v2, 'ld_jump_idle'), ('base', base, 'ld_jump_fwd'), ('v2', v2, 'ld_jump_fwd')):
    subprocess.run([sys.executable, GRID, 'video', cap, seq, os.path.join(out, f'{tag}_{seq}.mp4'), '--from=80', '--to=240'],
                   check=True, capture_output=True)
for f in os.listdir(tmp):
    os.remove(os.path.join(tmp, f))
os.rmdir(tmp)
print('evidence: ' + ', '.join(sorted(os.listdir(out))))
