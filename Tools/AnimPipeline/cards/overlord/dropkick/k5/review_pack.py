"""dropkick-k5 review pack: base vs K5 grids at the SAME anim frames (aligned on the dropkick montage start) and game-camera mp4s.
python review_pack.py <base_capture_dir> <k5_capture_dir> <out_dir>   (prints at most 5 lines)"""
import json, os, subprocess, sys
from PIL import Image, ImageDraw
import imageio_ffmpeg

base, k5, out = sys.argv[1:4]
os.makedirs(out, exist_ok=True)
FF = imageio_ffmpeg.get_ffmpeg_exe()


def start(cap, seq):
    for l in open(os.path.join(cap, seq, 'frames.jsonl')):
        d = json.loads(l)
        if d.get('montage') and 'OverlordDropkick' in d['montage'][0] and 'Bounce' not in d['montage'][0]:
            return d['f']


lines = []
for seq in ('dropkick_sweet_whiff', 'dropkick_sweet_hit'):
    s0 = {'base': start(base, seq), 'k5': start(k5, seq)}
    for view, crop in (('game', None), ('az90', (0, 0, 480, 360)), ('az315', (60, 20, 420, 380))):
        anim = list(range(0, 22))   # a0..a21: startup 0-15, loop/landing after
        C = 200
        G = Image.new('RGB', (len(anim) // 2 * C, 4 * C), 'white'); dr = ImageDraw.Draw(G)
        for r, (name, cap) in enumerate((('base', base), ('k5', k5))):
            for i, a in enumerate(anim):
                p = os.path.join(cap, seq, view, f'f{s0[name] + a:03d}.png')
                if not os.path.exists(p):
                    continue
                im = Image.open(p).convert('RGB')
                if crop:
                    im = im.crop(crop)
                im = im.resize((C, C))
                x, y = (i % 11) * C, (r * 2 + i // 11) * C
                G.paste(im, (x, y)); dr.rectangle([x, y, x + C, y + 13], fill='black')
                dr.text((x + 2, y + 1), f'{name} a{a} v{s0[name] + a}', fill='yellow' if name == 'k5' else 'white')
        g = os.path.join(out, f'grid_{seq}_{view}.jpg'); G.save(g, quality=85)
    for name, cap in (('base', base), ('k5', k5)):
        src = os.path.join(cap, seq, 'game', 'f%03d.png')
        for tag, rate in (('realspeed', 60), ('slow4x', 15)):
            mp4 = os.path.join(out, f'{name}_{seq}_{tag}.mp4')
            subprocess.run([FF, '-y', '-loglevel', 'error', '-framerate', str(rate), '-start_number', str(max(0, s0[name] - 20)), '-i', src,
                            '-frames:v', '70', '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2', '-c:v', 'libx264', '-pix_fmt', 'yuv420p', mp4], check=True)
    lines.append(f'{seq}: montage start base v{s0["base"]} k5 v{s0["k5"]}')
lines.append(f'out {out}')
for l in lines[:5]:
    print(l)
