"""dropkick-k5 pose round 3 review pack (2026-11-21), from review_pack_r2.py with review k5r2b's capture fixes: every mp4 starts at
a-20 and has "a<N> v<frame>" burned in, the high press (side cameras at 160 cm) gives a head-and-arms close-up of two loop frames
(closeup_loop.jpg), and the assert lines are copied into the pack (assert_lines.txt). Round 2 docstring follows.
dropkick-k5 pose round 2 review pack (2026-11-21). Answers review k5b's capture notes: side and quarter mp4s (az90, az315) as well
as the game view, big tiles (480 px) of a3, a15 and two loop frames, and the high press (smug fall loop) as its own mp4 and grid.
python review_pack_r2.py <base_dir> <k5_whiff_dir> <k5_hit_dir> <k5_high_dir> <out_dir>      (prints at most 5 lines)
Whiff and hit come from separate Play sessions: in one session the hit's dummy stays and stands in front of the side camera.
a<N> = N frames after the dropkick montage starts (a0), the same anim frame for base and K5."""
import json, os, subprocess, sys
from PIL import Image, ImageDraw
import imageio_ffmpeg, shutil, tempfile

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
            mp4(src, s0[n] - 20, 70, os.path.join(out, f'{n}_{seq}_{view}_realspeed.mp4'), 60, s0[n])
            mp4(src, s0[n] - 20, 70, os.path.join(out, f'{n}_{seq}_{view}_slow4x.mp4'), 15, s0[n])
    # big tiles: a3 (fold already reads), a15 (strike), a18 and a22 (loop), full 480 px, side and quarter, K5 over base
    for view in ('az90', 'az315'):
        T = Image.new('RGB', (4 * 480, 480 * len(s0)), 'white')
        for r, n in enumerate([n for n in ('base', 'k5') if n in s0]):
            for i, a in enumerate((3, 14, 15, 18)):
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
    n = (h1 + 15) - (h0 - 20)
    mp4(src, h0 - 20, n, os.path.join(out, f'k5high_{seq}_{view}_realspeed.mp4'), 60, h0)
    mp4(src, h0 - 20, n, os.path.join(out, f'k5high_{seq}_{view}_slow4x.mp4'), 15, h0)
C = 240; frames = list(range(h0 + 13, h1 + 3, 3))[:22]
G = Image.new('RGB', (11 * C, C * 2 * ((len(frames) + 10) // 11)), 'white')
for i, f in enumerate(frames):
    for j, view in enumerate(('az90', 'az315')):
        p = os.path.join(high, seq, view, f'f{f:03d}.png')
        if os.path.exists(p):
            G.paste(label(Image.open(p).convert('RGB').resize((C, C)), f'a{f - h0} v{f} {view}'), ((i % 11) * C, ((i // 11) * 2 + j) * C))
G.save(os.path.join(out, 'grid_k5high_fall.jpg'), quality=88)
# close-up: two loop frames of the high press (cameras at 160 cm), side and quarter, cropped on the upper body (CROP env: x0,y0,size)
cx, cy, cs = (int(v) for v in os.environ.get('CROP', '120,120,240').split(','))
CL = Image.new('RGB', (2 * 480, 2 * 480), 'white')
for i, a in enumerate((20, 26)):
    for j, view in enumerate(('az90', 'az315')):
        p = os.path.join(high, seq, view, f'f{h0 + a:03d}.png')
        if os.path.exists(p):
            CL.paste(label(Image.open(p).convert('RGB').crop((cx, cy, cx + cs, cy + cs)).resize((480, 480)), f'k5high a{a} v{h0 + a} {view} close-up'), (i * 480, j * 480))
CL.save(os.path.join(out, 'closeup_loop.jpg'), quality=90)
with open(os.path.join(out, 'assert_lines.txt'), 'w') as fh:
    for name, cap in (('k5 whiff', k5w), ('k5 hit', k5h), ('k5 high', high), ('base whiff', base)):
        for fn in ('assert.txt', 'assert_offline.txt'):
            p = os.path.join(cap, fn)
            if os.path.exists(p):
                fh.writelines(f'{name} ({fn}): {l}' for l in open(p) if l.startswith('ASSERT'))
lines.append(f'high: montage start v{h0}, landing v{h1} ({h1 - h0 - 16} loop frames)')
lines.append(f'out {out}')
for l in lines[:5]:
    print(l)
