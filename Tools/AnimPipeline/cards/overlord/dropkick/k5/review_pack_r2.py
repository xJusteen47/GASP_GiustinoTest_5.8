"""dropkick-k5 pose round 2 review pack (2026-11-21). Answers review k5b's capture notes: side and quarter mp4s (az90, az315) as well
as the game view, big tiles (480 px) of a3, a15 and two loop frames, and the high press (smug fall loop) as its own mp4 and grid.
python review_pack_r2.py <base_dir> <k5_whiff_dir> <k5_hit_dir> <k5_high_dir> <out_dir>      (prints at most 5 lines)
Whiff and hit come from separate Play sessions: in one session the hit's dummy stays and stands in front of the side camera.
a<N> = N frames after the dropkick montage starts (a0), the same anim frame for base and K5."""
import json, os, subprocess, sys
from PIL import Image, ImageDraw
import imageio_ffmpeg

base, k5w, k5h, high, out = sys.argv[1:6]
os.makedirs(out, exist_ok=True)
FF = imageio_ffmpeg.get_ffmpeg_exe()


def start(cap, seq):
    for l in open(os.path.join(cap, seq, 'frames.jsonl')):
        d = json.loads(l)
        if d.get('montage') and 'OverlordDropkick' in d['montage'][0] and 'Bounce' not in d['montage'][0]:
            return d['f']


def land(cap, seq, s0):
    """first frame after the startup whose montage is no longer the dropkick's (landing / bounce), else the last frame"""
    last = s0
    for l in open(os.path.join(cap, seq, 'frames.jsonl')):
        d = json.loads(l)
        if d['f'] <= s0 + 15:
            continue
        m = (d.get('montage') or [''])[0]
        if 'OverlordDropkick' not in m or 'Bounce' in m:
            return d['f']
        last = d['f']
    return last


def label(im, txt, col='yellow'):
    dr = ImageDraw.Draw(im); dr.rectangle([0, 0, im.width, 15], fill='black'); dr.text((3, 2), txt, fill=col); return im


def mp4(src_dir, first, n, path, rate):
    subprocess.run([FF, '-y', '-loglevel', 'error', '-framerate', str(rate), '-start_number', str(max(0, first)), '-i',
                    os.path.join(src_dir, 'f%03d.png'), '-frames:v', str(n), '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2', '-c:v', 'libx264',
                    '-pix_fmt', 'yuv420p', path], check=True)


lines = []
for seq in ('dropkick_sweet_whiff', 'dropkick_sweet_hit'):
    caps = {'base': base, 'k5': k5w if 'whiff' in seq else k5h}
    s0 = {n: start(c, seq) for n, c in caps.items() if os.path.isdir(os.path.join(c, seq))}
    if 'k5' not in s0:
        continue
    for view in ('az90', 'az315', 'game'):
        C = 240; anim = list(range(0, 22)); rows = [n for n in ('base', 'k5') if n in s0]
        G = Image.new('RGB', (11 * C, 2 * C * len(rows)), 'white')
        for r, n in enumerate(rows):
            for i, a in enumerate(anim):
                p = os.path.join(caps[n], seq, view, f'f{s0[n] + a:03d}.png')
                if os.path.exists(p):
                    G.paste(label(Image.open(p).convert('RGB').resize((C, C)), f'{n} a{a} v{s0[n] + a}', 'yellow' if n == 'k5' else 'white'),
                            ((i % 11) * C, (r * 2 + i // 11) * C))
        G.save(os.path.join(out, f'grid_{seq}_{view}.jpg'), quality=88)
        for n in s0:
            src = os.path.join(caps[n], seq, view)
            mp4(src, s0[n] - 20, 70, os.path.join(out, f'{n}_{seq}_{view}_realspeed.mp4'), 60)
            mp4(src, s0[n] - 20, 70, os.path.join(out, f'{n}_{seq}_{view}_slow4x.mp4'), 15)
    # big tiles: a3 (fold already reads), a15 (strike), a18 and a22 (loop), full 480 px, side and quarter, K5 over base
    for view in ('az90', 'az315'):
        T = Image.new('RGB', (4 * 480, 480 * len(s0)), 'white')
        for r, n in enumerate([n for n in ('base', 'k5') if n in s0]):
            for i, a in enumerate((3, 15, 18, 22)):
                p = os.path.join(caps[n], seq, view, f'f{s0[n] + a:03d}.png')
                if os.path.exists(p):
                    T.paste(label(Image.open(p).convert('RGB'), f'{n} a{a} v{s0[n] + a} {view}'), (i * 480, r * 480))
        T.save(os.path.join(out, f'tiles_{seq}_{view}.jpg'), quality=90)
    lines.append(f'{seq}: montage start ' + ' '.join(f'{n} v{v}' for n, v in s0.items()))

# high press: the smug fall, from the strike to the landing
seq = 'dropkick_sweet_whiff'
h0 = start(high, seq); h1 = land(high, seq, h0)
for view in ('az90', 'az315', 'game'):
    src = os.path.join(high, seq, view)
    n = (h1 + 15) - (h0 - 10)
    mp4(src, h0 - 10, n, os.path.join(out, f'k5high_{seq}_{view}_realspeed.mp4'), 60)
    mp4(src, h0 - 10, n, os.path.join(out, f'k5high_{seq}_{view}_slow4x.mp4'), 15)
C = 240; frames = list(range(h0 + 13, h1 + 3, 3))[:22]
G = Image.new('RGB', (11 * C, C * 2 * ((len(frames) + 10) // 11)), 'white')
for i, f in enumerate(frames):
    for j, view in enumerate(('az90', 'az315')):
        p = os.path.join(high, seq, view, f'f{f:03d}.png')
        if os.path.exists(p):
            G.paste(label(Image.open(p).convert('RGB').resize((C, C)), f'a{f - h0} v{f} {view}'), ((i % 11) * C, ((i // 11) * 2 + j) * C))
G.save(os.path.join(out, 'grid_k5high_fall.jpg'), quality=88)
lines.append(f'high: montage start v{h0}, landing v{h1} ({h1 - h0 - 16} loop frames)')
lines.append(f'out {out}')
for l in lines[:5]:
    print(l)
