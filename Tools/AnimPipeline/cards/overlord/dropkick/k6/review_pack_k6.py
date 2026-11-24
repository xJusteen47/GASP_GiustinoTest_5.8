"""dropkick K6 review pack (2026-11-21): K5 round 3 (approved candidate, same cameras) vs K6 (weight, arm press, strike settle, the
owner's Big Band line). Built on k5/review_pack_r3.py: every mp4 starts at a-20 with "a<N> v<frame>" burned in.
python review_pack_k6.py <k5_whiff> <k5_hit> <k5_high> <k6_whiff> <k6_hit> <k6_high> <out_dir>      (prints at most 5 lines)
a<N> = N frames after the dropkick montage starts (a0); K6 settle clip = a16-a27 (replaces the first loop repetition)."""
import json, os, subprocess, sys
from PIL import Image, ImageDraw
import imageio_ffmpeg, shutil, tempfile

k5w, k5h, k5hi, k6w, k6h, k6hi, out = sys.argv[1:8]
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


def mp4(src_dir, first, n, path, rate, a0=None):
    """a0 = capture frame of a0: every frame gets "a<N> v<frame>" burned in (review k5r2b: the offsets cost one image)"""
    tmp = tempfile.mkdtemp()
    first = max(0, first)
    for i in range(n):
        p = os.path.join(src_dir, f'f{first + i:03d}.png')
        if not os.path.exists(p):
            n = i; break
        im = Image.open(p).convert('RGB')
        if a0 is not None:
            label(im, f'a{first + i - a0} v{first + i} {os.path.basename(src_dir)}')
        im.save(os.path.join(tmp, f'f{i:03d}.png'))
    subprocess.run([FF, '-y', '-loglevel', 'error', '-framerate', str(rate), '-start_number', '0', '-i',
                    os.path.join(tmp, 'f%03d.png'), '-frames:v', str(n), '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2', '-c:v', 'libx264',
                    '-pix_fmt', 'yuv420p', path], check=True)
    shutil.rmtree(tmp, ignore_errors=True)


lines = []
CAPS = {'dropkick_sweet_whiff': {'k5': k5w, 'k6': k6w}, 'dropkick_sweet_hit': {'k5': k5h, 'k6': k6h}}
for seq, caps in CAPS.items():
    s0 = {n: start(c, seq) for n, c in caps.items() if os.path.isdir(os.path.join(c, seq))}
    if 'k6' not in s0:
        continue
    rows = [n for n in ('k5', 'k6') if n in s0]
    for view in ('az90', 'az315', 'game'):
        C = 240; anim = list(range(0, 22))
        G = Image.new('RGB', (11 * C, 2 * C * len(rows)), 'white')
        for r, n in enumerate(rows):
            for i, a in enumerate(anim):
                p = os.path.join(caps[n], seq, view, f'f{s0[n] + a:03d}.png')
                if os.path.exists(p):
                    G.paste(label(Image.open(p).convert('RGB').resize((C, C)), f'{n} a{a} v{s0[n] + a}', 'yellow' if n == 'k6' else 'white'),
                            ((i % 11) * C, (r * 2 + i // 11) * C))
        G.save(os.path.join(out, f'grid_{seq}_{view}.jpg'), quality=88)
        for n in s0:
            src = os.path.join(caps[n], seq, view)
            mp4(src, s0[n] - 20, 70, os.path.join(out, f'{n}_{seq}_{view}_realspeed.mp4'), 60, s0[n])
            mp4(src, s0[n] - 20, 70, os.path.join(out, f'{n}_{seq}_{view}_slow4x.mp4'), 15, s0[n])
    # big tiles: a4 (press), a8 (relax), a15 (strike), a17 (settle peak), K6 under K5
    for view in ('az90', 'az315'):
        T = Image.new('RGB', (4 * 480, 480 * len(rows)), 'white')
        for r, n in enumerate(rows):
            for i, a in enumerate((4, 8, 15, 17)):
                p = os.path.join(caps[n], seq, view, f'f{s0[n] + a:03d}.png')
                if os.path.exists(p):
                    T.paste(label(Image.open(p).convert('RGB'), f'{n} a{a} v{s0[n] + a} {view}'), (i * 480, r * 480))
        T.save(os.path.join(out, f'tiles_{seq}_{view}.jpg'), quality=90)
    lines.append(f'{seq}: montage start ' + ' '.join(f'{n} v{v}' for n, v in s0.items()))

# high press: the strike, the settle and the fall to the landing, K5 and K6
seq = 'dropkick_sweet_whiff'
hi = {}
for n, cap in (('k5', k5hi), ('k6', k6hi)):
    if not os.path.isdir(os.path.join(cap, seq)):
        continue
    h0 = start(cap, seq); h1 = land(cap, seq, h0); hi[n] = (h0, h1)
    for view in ('az90', 'az315', 'game'):
        src = os.path.join(cap, seq, view)
        k = (h1 + 15) - (h0 - 20)
        mp4(src, h0 - 20, k, os.path.join(out, f'{n}high_{seq}_{view}_realspeed.mp4'), 60, h0)
        mp4(src, h0 - 20, k, os.path.join(out, f'{n}high_{seq}_{view}_slow4x.mp4'), 15, h0)
# settle frame by frame on the high press (side and quarter): a14-a24, every frame, K5 over K6
C = 240; G = Image.new('RGB', (11 * C, C * 2 * len(hi)), 'white')
for r, n in enumerate(hi):
    cap = k5hi if n == 'k5' else k6hi; h0 = hi[n][0]
    for i, a in enumerate(range(14, 25)):
        for j, view in enumerate(('az90', 'az315')):
            p = os.path.join(cap, seq, view, f'f{h0 + a:03d}.png')
            if os.path.exists(p):
                G.paste(label(Image.open(p).convert('RGB').resize((C, C)), f'{n} a{a} v{h0 + a} {view}', 'yellow' if n == 'k6' else 'white'),
                        (i * C, (r * 2 + j) * C))
G.save(os.path.join(out, 'grid_high_settle.jpg'), quality=88)
with open(os.path.join(out, 'assert_lines.txt'), 'w') as fh:
    for name, cap in (('k6 whiff', k6w), ('k6 hit', k6h), ('k6 high', k6hi)):
        for fn in ('assert.txt', 'assert_offline.txt'):
            p = os.path.join(cap, fn)
            if os.path.exists(p):
                fh.writelines(f'{name} ({fn}): {l}' for l in open(p) if l.startswith('ASSERT'))
for n, (h0, h1) in hi.items():
    lines.append(f'{n} high: montage start v{h0}, landing v{h1}')
lines.append(f'out {out}')
for l in lines[:5]:
    print(l)
