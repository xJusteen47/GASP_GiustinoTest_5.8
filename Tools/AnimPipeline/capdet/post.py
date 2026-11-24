"""After the capture (plain Python, no editor; needs Pillow, ffmpeg optional): frame checks, fingerprint, split-screen video, one ASSERT.

Layout written by Tools/GASNPPTest/pie_capture_move.py: <cap>/<sequence>/<view>/f<NNN>.png (one per rendered 60 fps frame) and
<cap>/<sequence>/frames.jsonl (one row per frame). Views of the deterministic set (PIE_CAPTURE_VIEWSET=det): behind, side, q34, close.

  python post.py <cap_dir> <sequence> <spec.json>             checks + video + fingerprint, prints <= 5 lines, first = verdict
  python post.py --verify <cap_dir_a> <cap_dir_b> <sequence>   same frames in two runs? (fingerprint.json of both)

ASSERT (owner 2026-11-22, "7 deterministic additions" no. 3), the verdict is OK only if ALL hold:
  clip     the expected clip owns the pose (weight >= 0.8 on >= 90 % of the window), capture_assert.check_rows, the same code the editor runs
  count    rows of frames.jsonl == PNGs of every view == spec "frames" (the duration is right: frames / 60 s)
  black    no frame of any view with mean luminance < BLACK_LUMA (a scene capture that did not render)
  dup      no two consecutive IDENTICAL frames inside the window (a dropped frame shown twice), unless spec "hold_ok" is true
           (a move that holds its last pose on purpose)
Prints "ASSERT: OK" or "ASSERT: FAIL <first reason>" then at most 3 more lines. The numbers are in <cap>/<sequence>/post.json.

Video: the views as a 2x2 split screen at 60 fps (frame duration 1000/60 ms, alternated 17/17/16 so the length is exact). ffmpeg (PATH, env
FFMPEG, imageio_ffmpeg) -> split.mp4; without ffmpeg an animated WebP (split.webp, Pillow) so a machine without it still gets a video.
Fingerprint: sha256 of every frame of every view (downscaled to 64x64 gray, immune to PNG encoder differences) -> fingerprint.json.
"""
import glob
import hashlib
import json
import os
import shutil
import subprocess
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import capture_assert as CA  # noqa: E402

BLACK_LUMA = 3.0       # 0-255 mean; a lit mannequin on the hub floor is > 30
VIEW_ORDER = ['behind', 'side', 'q34', 'close']
FPS = 60


def _views(seq_dir, want=None):
    names = [d for d in sorted(os.listdir(seq_dir)) if os.path.isdir(os.path.join(seq_dir, d))]
    if want:
        names = [v for v in want if v in names]
    elif any(v in names for v in VIEW_ORDER):
        names = [v for v in VIEW_ORDER if v in names]
    return names


def _frames(view_dir):
    out = {}
    for p in glob.glob(os.path.join(view_dir, 'f*.png')):
        try:
            out[int(os.path.basename(p)[1:-4])] = p
        except ValueError:
            pass
    return out


def _tiny(path):
    return Image.open(path).convert('L').resize((64, 64), Image.BILINEAR)


def analyse(seq_dir, spec):
    """per view: frame numbers, mean luma, tiny-image hash per frame. Returns the dict of everything the verdict needs."""
    res = {'views': {}}
    for v in _views(seq_dir, spec.get('views_list')):
        fr = _frames(os.path.join(seq_dir, v))
        info = {'n': len(fr), 'first': min(fr) if fr else None, 'last': max(fr) if fr else None, 'luma': {}, 'hash': {}}
        for f in sorted(fr):
            t = _tiny(fr[f]); b = t.tobytes()
            info['luma'][f] = sum(b) / float(len(b))
            info['hash'][f] = hashlib.sha256(b).hexdigest()[:16]
        res['views'][v] = info
    return res


def verdict(seq_dir, spec, ana):
    """returns (ok, [reason lines]); the first reason is the first thing to fix"""
    why, win = [], spec['expect']['window']
    rows = CA.load_rows(os.path.join(seq_dir, 'frames.jsonl')) if os.path.exists(os.path.join(seq_dir, 'frames.jsonl')) else {}
    ce = spec['expect']
    r = CA.check_rows(rows, ce['clip'], ce['window'], ce.get('min_weight', CA.MIN_WEIGHT), ce.get('coverage', CA.COVERAGE),
                      False, ce.get('additive'))
    if not r['ok']:
        why.append(CA.line(r)[len('ASSERT: FAIL '):].strip())
    want = spec.get('frames')
    if not ana['views']:
        why.append('no view folders (capture did not shoot)')
    for v, i in ana['views'].items():
        if want and (i['n'] != want or len(rows) != want):
            why.append(f'count {v}: {i["n"]} png, {len(rows)} rows, expected {want} (={want / FPS:.2f} s)')
            break
    if want and not any(x.startswith('count') for x in why) and len(rows) != want and ana['views']:
        why.append(f'count: {len(rows)} rows, expected {want}')
    for v, i in ana['views'].items():
        dark = [f for f, l in i['luma'].items() if l < BLACK_LUMA]
        if dark:
            why.append(f'black {v}: {len(dark)} frames, first f{dark[0]}')
            break
    if not spec.get('hold_ok'):
        for v, i in ana['views'].items():
            fs = [f for f in sorted(i['hash']) if win[0] <= f <= win[1]]
            dup = [f for a, f in zip(fs, fs[1:]) if i['hash'][a] == i['hash'][f]]
            if dup:
                why.append(f'duplicate {v}: {len(dup)} frames repeat the previous one, first f{dup[0]}')
                break
    return not why, why


def fingerprint(ana):
    return {v: {'frames': i['n'], 'all': hashlib.sha256(''.join(i['hash'][f] for f in sorted(i['hash'])).encode()).hexdigest()[:24],
                'hash': [i['hash'][f] for f in sorted(i['hash'])]} for v, i in ana['views'].items()}


def find_ffmpeg():
    for c in (os.environ.get('FFMPEG'), shutil.which('ffmpeg')):
        if c and os.path.exists(c):
            return c
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        return None


def _tile(frames_by_view, f, views, cell, label=True):
    cols = 2 if len(views) > 1 else 1
    rows = (len(views) + cols - 1) // cols
    img = Image.new('RGB', (cols * cell, rows * cell), (0, 0, 0))
    for k, v in enumerate(views):
        p = frames_by_view[v].get(f)
        if p:
            img.paste(Image.open(p).convert('RGB').resize((cell, cell)), ((k % cols) * cell, (k // cols) * cell))
        if label:
            ImageDraw.Draw(img).text(((k % cols) * cell + 4, (k // cols) * cell + 4), f'{v}  f{f}', fill=(255, 255, 0))
    return img


def make_video(seq_dir, out_base, views, cell=480, first=None, last=None):
    """2x2 split screen, 60 fps. Returns the written path or None."""
    fv = {v: _frames(os.path.join(seq_dir, v)) for v in views}
    allf = sorted(set.intersection(*[set(x) for x in fv.values()])) if fv else []
    if first is not None:
        allf = [f for f in allf if first <= f <= last]
    if not allf:
        return None
    ff = find_ffmpeg()
    if ff:
        tmp = out_base + '_tmp'
        os.makedirs(tmp, exist_ok=True)
        for n, f in enumerate(allf):
            _tile(fv, f, views, cell).save(os.path.join(tmp, f'{n:05d}.png'))
        out = out_base + '.mp4'
        subprocess.run([ff, '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', os.path.join(tmp, '%05d.png'), '-c:v', 'libx264',
                        '-pix_fmt', 'yuv420p', '-crf', '16', out], check=True)
        shutil.rmtree(tmp, ignore_errors=True)
        return out
    out = out_base + '.webp'
    tiles = [_tile(fv, f, views, cell // 2) for f in allf]
    dur = [17, 17, 16] * (len(tiles) // 3 + 1)   # 1000/60 ms = 16.67: three frames take 50 ms
    tiles[0].save(out, save_all=True, append_images=tiles[1:], duration=dur[:len(tiles)], loop=0, quality=80)
    return out


def run(cap, seq, spec):
    seq_dir = os.path.join(cap, seq)
    ana = analyse(seq_dir, spec)
    ok, why = verdict(seq_dir, spec, ana)
    fp = fingerprint(ana)
    with open(os.path.join(seq_dir, 'fingerprint.json'), 'w', encoding='utf-8') as fh:
        json.dump(fp, fh, indent=0)
    win = spec['expect']['window']
    video = None
    try:
        video = make_video(seq_dir, os.path.join(seq_dir, 'split'), list(ana['views']))
    except Exception as e:   # a video that cannot be made is not a pass
        why.append(f'video: {type(e).__name__} {e}')
        ok = False
    rep = {'ok': ok, 'why': why, 'video': video, 'window': win, 'frames': {v: i['n'] for v, i in ana['views'].items()}}
    with open(os.path.join(seq_dir, 'post.json'), 'w', encoding='utf-8') as fh:
        json.dump(rep, fh, indent=1)
    head = 'ASSERT: OK' if ok else 'ASSERT: FAIL ' + why[0]
    return ok, [head] + why[1:4] + ([f'video: {video}'] if video else [])


def verify(cap_a, cap_b, seq):
    with open(os.path.join(cap_a, seq, 'fingerprint.json'), encoding='utf-8') as fa, open(os.path.join(cap_b, seq, 'fingerprint.json'), encoding='utf-8') as fb:
        a, b = json.load(fa), json.load(fb)
    bad = []
    for v in sorted(set(a) & set(b)):
        if a[v]['all'] != b[v]['all']:
            diff = [i for i, (x, y) in enumerate(zip(a[v]['hash'], b[v]['hash'])) if x != y]
            bad.append(f'{v}: {len(diff)}/{a[v]["frames"]} frames differ, first index {diff[0] if diff else "length"}')
    if set(a) != set(b):
        bad.append(f'views differ: {sorted(set(a) ^ set(b))}')
    return not bad, (['DETERMINISM: OK identical frames in both runs'] if not bad else ['DETERMINISM: FAIL ' + bad[0]] + bad[1:3])


def main(argv):
    if argv[:1] == ['--verify'] and len(argv) == 4:
        ok, ls = verify(argv[1], argv[2], argv[3]); print('\n'.join(ls)); return 0 if ok else 1
    if len(argv) != 3 or not os.path.exists(argv[2]):
        print('usage: post.py <cap_dir> <sequence> <spec.json> | --verify <cap_a> <cap_b> <sequence>'); return 2
    ok, ls = run(argv[0], argv[1], json.load(open(argv[2], encoding='utf-8')))
    print('\n'.join(ls[:5]))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
