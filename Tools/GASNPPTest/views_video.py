"""Side-by-side review video of a pie_capture_move.py sequence: chosen views in one row, each frame labelled (dropkick-tb-4, 2026-11-21).

Why: pie_capture_grid.py video puts seven views in a 4 x 2 mosaic; the dropkick tech bounce review r3 judged a 0.67 s mosaic where the body was
a few pixels tall (review-f4.md). This keeps three views large, labels every frame with its capture number (v<N>) and the playing montage and
frame, and writes a real-speed and a 4x slow copy.

    python views_video.py <capture_dir> <sequence> <out_stem> [--views=az90,az180,game] [--from=F] [--to=F]
Writes <out_stem>_realspeed.mp4 (60 fps) and <out_stem>_slow4x.mp4 (15 fps). ffmpeg must be on PATH. Prints one line.
"""
import json
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw


def main(argv):
    pos = [a for a in argv if not a.startswith('--')]
    opt = dict(a[2:].split('=', 1) for a in argv if a.startswith('--') and '=' in a)
    cap, seq, stem = pos[0], pos[1], pos[2]
    views = opt.get('views', 'az90,az180,game').split(',')
    rows = {}
    with open(os.path.join(cap, seq, 'frames.jsonl')) as fh:
        for line in fh:
            d = json.loads(line)
            rows[d['f']] = d
    first, last = int(opt.get('from', min(rows))), int(opt.get('to', max(rows)))
    tmp = tempfile.mkdtemp(prefix='vv_')
    n = 0
    for f in range(first, last + 1):
        tiles = []
        for v in views:
            p = os.path.join(cap, seq, v, f'f{f:03d}.png')
            if not os.path.exists(p):
                p = os.path.join(cap, seq, v, f'f{f}.png')
            tiles.append(Image.open(p).convert('RGB') if os.path.exists(p) else Image.new('RGB', (480, 480)))
        w, h = sum(t.width for t in tiles), max(t.height for t in tiles)
        img = Image.new('RGB', (w, h + 28))
        x = 0
        for v, t in zip(views, tiles):
            img.paste(t, (x, 28))
            ImageDraw.Draw(img).text((x + 6, 32), v, fill=(255, 255, 0))
            x += t.width
        m = rows.get(f, {}).get('montage')
        label = f'v{f}   ' + (f'{m[0]} f{m[1]:.1f}' if m else 'no montage') + f"   pelvis {rows.get(f, {}).get('body', {}).get('pelvis_z', 0):.0f} cm"
        ImageDraw.Draw(img).text((8, 8), label, fill=(255, 255, 255))
        img.save(os.path.join(tmp, f'{n:05d}.png'))
        n += 1
    outs = []
    for suffix, fps in (('realspeed', 60), ('slow4x', 15)):
        out = f'{stem}_{suffix}.mp4'
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(fps), '-i', os.path.join(tmp, '%05d.png'),
                        '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2', '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '20', out], check=True)
        outs.append(out)
    print(f'views_video: {n} frames v{first}-v{last}, {len(views)} views -> {", ".join(os.path.basename(o) for o in outs)}')


if __name__ == '__main__':
    main(sys.argv[1:])
