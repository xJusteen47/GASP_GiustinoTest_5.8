"""Grids and mp4 from a Play capture of Tools/GASNPPTest/pie_capture_move.py (plain Python, outside the editor; needs Pillow and ffmpeg).
    python pie_capture_grid.py video <capture_dir> <sequence> <out.mp4> [--fps=60] [--from=F] [--to=F]
    python pie_capture_grid.py sheet <capture_dir> <sequence> <view> <out.png> --from=F --to=F [--cols=6] [--cell=256] [--step=1] [--crop=x0,y0,x1,y1]
    python pie_capture_grid.py state <capture_dir> <sequence>          (the combat state per frame, compressed to runs, at most 40 lines)
video: every frame as one image, the seven views side by side (4 x 2, the last cell is the frame's state), at 60 fps = real time; --fps=15 is 0.25x.
sheet: one view, consecutive frames left to right, top to bottom; every cell carries its label.
Labels: v<video frame>, m<timeline index>:<timeline frame>, a<animation frame the montage shows, 60 fps from 0>, H while the attacker is in
hitstop, T on the frame a press was sent.
Video frames are rendered frames at 60 fps; timeline frames are the simulation's (62.5 Hz), so now and then one video frame spans two of them."""
import json
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

VIEWS = ['az0', 'az45', 'az90', 'game', 'az180', 'az270', 'az315']


def rows_of(cap, seq):
    """The rows of a sequence, each with 'tl' (index of the running timeline in the kit's table, None when idle), 'tlf' (its frame: simulation
    frames since the first row that showed it, minus the frames it was paused by hitstop; within a frame of the real one) and 'hitstop'"""
    with open(os.path.join(cap, seq, 'frames.jsonl')) as f:
        rows = [json.loads(line) for line in f if line.strip()]
    current, start, paused = None, 0, 0
    for r in rows:
        c = r.get('combat') or {}
        tl = c.get('timeline') if c.get('inTimeline') else None
        if tl != current:
            current, start, paused = tl, c.get('historyFrame', 0), 0
        r['tl'] = tl
        r['tlf'] = c.get('historyFrame', 0) - start - c.get('timelinePaused', 0) if tl is not None else None
        r['hitstop'] = tl is not None and c.get('timelinePaused', 0) > paused
        paused = c.get('timelinePaused', 0)
    return rows


def label(row):
    text = f"v{row['f']:03d}"
    if row['tl'] is not None:
        text += f" m{row['tl']}:{row['tlf']}"
    m = row.get('montage')
    if m and m[0] != 'error':
        text += f" a{m[1]:.1f}"
    if row['hitstop']:
        text += ' H'
    if row.get('seg') and row['seg'] != 'idle':   # pie_capture_react.py rows: segment and frame in it
        text += f" {row['seg']}:{row['segf']}"
    if row.get('tap'):
        text += ' T'
    return text


def cell(cap, seq, view, row, size, crop=None):
    """One frame of one view, labelled. crop (x0, y0, x1, y1) in source pixels keeps that part at its own resolution instead of resizing"""
    path = os.path.join(cap, seq, view, f"f{row['f']:03d}.png")
    if not os.path.exists(path):
        img = Image.new('RGB', (size, size), (60, 0, 0))
    elif crop:
        img = Image.open(path).convert('RGB').crop(crop)
    else:
        img = Image.open(path).convert('RGB').resize((size, size), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, img.size[0], 13], fill=(0, 0, 0))
    d.text((3, 1), f'{view} {label(row)}', fill=(255, 255, 0))
    return img


def views_of(cap, seq):
    """The views a capture really has, in VIEWS order (a capture of pie_capture_react.py has three), then any other folder"""
    have = [d for d in os.listdir(os.path.join(cap, seq)) if os.path.isdir(os.path.join(cap, seq, d))]
    return [v for v in VIEWS if v in have] + sorted(d for d in have if d not in VIEWS) or VIEWS


def opts():
    return dict((a[2:].split('=', 1) + [''])[:2] for a in sys.argv[1:] if a.startswith('--'))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    o = opts()
    mode, cap, seq = args[0], args[1], args[2]
    rows = rows_of(cap, seq)
    lo, hi = int(o.get('from', 0)), int(o.get('to', len(rows) - 1))
    rows = [r for r in rows if lo <= r['f'] <= hi][::int(o.get('step', 1))]
    if mode == 'state':
        runs = []
        for r in rows:
            key = label(r)[5:].rsplit(':', 1)[0] if ':' in label(r) else label(r)[5:]
            if runs and runs[-1][0] == key:
                runs[-1][2] = r['f']
            else:
                runs.append([key, r['f'], r['f']])
        for key, a, b in runs[:38]:
            print(f'v{a:03d}-v{b:03d} {key or "idle"}')
        first = next((r for r in rows if r.get('combat')), None)
        print('combat keys:', sorted((first or {}).get('combat', {}).keys())[:30])
    elif mode == 'sheet':
        view, out = args[3], args[4]
        cols, size = int(o.get('cols', 6)), int(o.get('cell', 256))
        crop = tuple(int(v) for v in o['crop'].split(',')) if o.get('crop') else None
        w, hgt = (crop[2] - crop[0], crop[3] - crop[1]) if crop else (size, size)
        sheet = Image.new('RGB', (cols * w, ((len(rows) + cols - 1) // cols) * hgt), (20, 20, 20))
        for i, r in enumerate(rows):
            sheet.paste(cell(cap, seq, view, r, size, crop), ((i % cols) * w, (i // cols) * hgt))
        os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
        sheet.save(out)
        print(f'{out}: {len(rows)} frames, {sheet.size[0]}x{sheet.size[1]}')
    elif mode == 'video':
        out, size = args[3], 360
        views = views_of(cap, seq)
        cols = 4 if len(views) > 3 else 2          # the last cell is the frame's text
        cells = len(views) + 1
        tmp = tempfile.mkdtemp(prefix='pie_capture_')
        for i, r in enumerate(rows):
            frame = Image.new('RGB', (cols * size, ((cells + cols - 1) // cols) * size), (20, 20, 20))
            for j, view in enumerate(views):
                frame.paste(cell(cap, seq, view, r, size), ((j % cols) * size, (j // cols) * size))
            d = ImageDraw.Draw(frame)
            d.text(((cells - 1) % cols * size + 12, (cells - 1) // cols * size + 20), f'{seq}\n{label(r)}\n\nv = video frame (60 fps)\nname:N = timeline frame\nH = hitstop, T = press', fill=(255, 255, 255))
            frame.save(os.path.join(tmp, f'{i:04d}.png'))
        os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
        cmd = ['ffmpeg', '-y', '-loglevel', 'error', '-framerate', o.get('fps', '60'), '-i', os.path.join(tmp, '%04d.png'), '-c:v', 'libx264',
               '-pix_fmt', 'yuv420p', '-crf', '18', out]
        code = subprocess.call(cmd)
        print(f'{out}: {len(rows)} frames, ffmpeg exit {code}')


if __name__ == '__main__':
    main()
