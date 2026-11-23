"""A/B montage of two captures, mechanical (owner's "7 deterministic additions" no. 5, 2026-11-23): NO model looks at anything here.

Rule 10 says a variant is judged against its BASE on the SAME frames, and the frames that matter are the ones around the hit and the
FAF. Hand-made montages drifted by a frame or two (different frame lists per view, a grid built from one run and a video from another),
and a one-frame drift is exactly what a "the arm leads the turn" judgement hangs on. So this script takes the two capture directories
that capdet.py already wrote, picks ONE frame list, and uses it for every output: the grids and the video show the same frames.

    python Tools/AnimPipeline/capdet/montage.py <base_cap> <var_cap> <sequence> [options]

  --spec <spec.json>    the variant's capdet spec: takes "ab" from it ({"hit": f, "faf": f, "around": 3, "video": [first, last]})
                        and, when --frames is absent, the window from "expect"
  --frames a,b,c        the frame list, explicit; wins over --hit/--faf and the spec
  --hit N  --faf N      build the list as N-around .. N+around for each (the spec's "ab" gives the same)
  --around N            frames on each side of hit and FAF (default 3); 0 = the single frame
  --views a,b           views to use (default: every view both captures have, in capdet order behind, side, q34, close)
  --out <dir>           where the outputs go (default <var_cap>/<sequence>/ab)
  --cell N              one view's side in the grid and the video, px (default 360)
  --labels "base,var"   column labels (default: the two directory names)
  --no-video            grids only

Outputs, all named after the sequence: `grid_<view>.png` ONE GRID PER VIEW (rows = frames, columns = base | variant, every cell
labelled with its view, frame and side, so a grid cannot be mistaken for another view's) and `sb_<view>.mp4` side by side at 60 fps
over the video range (the spec's "ab.video", else the expect window). Without ffmpeg (the laptop has none) each video is an animated
WebP `sb_<view>.webp` instead, same frames, same 60 fps timing: post.make_video's fallback, so a machine without ffmpeg still gets a
video to look at. `montage.json` lists every output with the frame list it was built from.

A frame that one of the two captures does not have is a FAIL, never a silently shifted grid: both captures are deterministic
(capdet.py), so a missing frame means the capture was short or the two runs are not comparable. Exit 0 OK, 1 missing frames or no
views in common, 2 usage. Prints at most 5 lines.
"""
import json
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import post  # noqa: E402

BAR = 16          # label strip height in a cell, px
AROUND = 3


def frame_list(spec, hit=None, faf=None, around=None, explicit=None):
    """The ONE frame list every output of this run uses. Sorted, no duplicates (hit and FAF windows may overlap).

    An argument given here wins over the spec's "ab" block; AROUND is only the last resort, so a spec that says "around": 1 is not
    silently widened to 3 by the default.
    """
    if explicit:
        return sorted(set(int(x) for x in explicit))
    ab = (spec or {}).get('ab') or {}
    hit = ab.get('hit') if hit is None else hit
    faf = ab.get('faf') if faf is None else faf
    around = ab.get('around', AROUND) if around is None else around
    fs = set()
    for c in (hit, faf):
        if c is not None:
            fs.update(range(int(c) - int(around), int(c) + int(around) + 1))
    if not fs and spec and spec.get('expect', {}).get('window'):
        a, b = spec['expect']['window']           # no hit/FAF given: the ends and the middle of the window
        fs = {int(a), int((a + b) // 2), int(b)}
    return sorted(f for f in fs if f >= 0)


def video_range(spec, views_frames):
    """(first, last) for the side-by-side video: the spec's "ab.video", else the expect window, else every shared frame."""
    ab = (spec or {}).get('ab') or {}
    if ab.get('video'):
        return int(ab['video'][0]), int(ab['video'][1])
    if spec and spec.get('expect', {}).get('window'):
        return int(spec['expect']['window'][0]), int(spec['expect']['window'][1])
    allf = sorted(set.intersection(*[set(x) for x in views_frames.values()])) if views_frames else []
    return (allf[0], allf[-1]) if allf else (0, -1)


def _cell(path, cell, text):
    """One labelled cell. A missing frame never reaches here (checked first), so a cell is always a real rendered frame."""
    im = Image.new('RGB', (cell, cell), (0, 0, 0))
    im.paste(Image.open(path).convert('RGB').resize((cell, cell)), (0, 0))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, cell, BAR], fill=(16, 16, 16))
    d.text((4, 2), text, fill=(255, 255, 0))
    return im


def grid(frames_a, frames_b, view, frames, labels, cell, out):
    """rows = frames, columns = base | variant. One PNG per view: the view's name is in every cell."""
    img = Image.new('RGB', (2 * cell, max(1, len(frames)) * cell), (12, 12, 12))
    for r, f in enumerate(frames):
        for c, (fv, side) in enumerate(((frames_a, labels[0]), (frames_b, labels[1]))):
            img.paste(_cell(fv[f], cell, f'{side} | {view} | f{f}'), (c * cell, r * cell))
    img.save(out)
    return out


def side_by_side(frames_a, frames_b, view, first, last, labels, cell, out_base):
    """base | variant, 60 fps, over [first, last]. mp4 with ffmpeg, animated WebP without it (post.make_video's own fallback)."""
    fs = [f for f in sorted(set(frames_a) & set(frames_b)) if first <= f <= last]
    if not fs:
        return None
    tiles = []
    for f in fs:
        im = Image.new('RGB', (2 * cell, cell), (0, 0, 0))
        im.paste(_cell(frames_a[f], cell, f'{labels[0]} | {view} | f{f}'), (0, 0))
        im.paste(_cell(frames_b[f], cell, f'{labels[1]} | {view} | f{f}'), (cell, 0))
        tiles.append(im)
    ff = post.find_ffmpeg()
    if ff:
        import shutil
        import subprocess
        tmp = out_base + '_tmp'
        os.makedirs(tmp, exist_ok=True)
        for n, im in enumerate(tiles):
            im.save(os.path.join(tmp, f'{n:05d}.png'))
        out = out_base + '.mp4'
        subprocess.run([ff, '-y', '-loglevel', 'error', '-framerate', str(post.FPS), '-i', os.path.join(tmp, '%05d.png'),
                        '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '16', out], check=True)
        shutil.rmtree(tmp, ignore_errors=True)
        return out
    out = out_base + '.webp'
    dur = [17, 17, 16] * (len(tiles) // 3 + 1)    # 1000/60 ms = 16.67: three frames take 50 ms, so the length stays exact
    tiles[0].save(out, save_all=True, append_images=tiles[1:], duration=dur[:len(tiles)], loop=0, quality=80)
    return out


def run(base_cap, var_cap, seq, spec=None, frames=None, views=None, out=None, cell=360, labels=None, video=True):
    """Returns (ok, [lines], report). Mechanical: no judgement, only "the same frames, side by side"."""
    da, db = os.path.join(base_cap, seq), os.path.join(var_cap, seq)
    labels = labels or [os.path.basename(os.path.normpath(base_cap)), os.path.basename(os.path.normpath(var_cap))]
    out = out or os.path.join(db, 'ab')
    for d in (da, db):
        if not os.path.isdir(d):
            return False, [f'MONTAGE: FAIL {d} does not exist (run capdet.py on both)'], {}
    shared = [v for v in post._views(da) if os.path.isdir(os.path.join(db, v))]
    if views:
        shared = [v for v in views if v in shared]
    if not shared:
        return False, [f'MONTAGE: FAIL no view in common between {da} and {db}'], {}
    fa = {v: post._frames(os.path.join(da, v)) for v in shared}
    fb = {v: post._frames(os.path.join(db, v)) for v in shared}
    want = frame_list(spec, explicit=frames)
    if not want:
        return False, ['MONTAGE: FAIL no frames to show (give --frames, or --hit/--faf, or an "expect.window" in the spec)'], {}
    missing = [(side, v, f) for side, fv in (('base', fa), ('var', fb)) for v in shared for f in want if f not in fv[v]]
    if missing:
        s, v, f = missing[0]
        return False, [f'MONTAGE: FAIL {len(missing)} frames missing, first {s}/{v} f{f} (captures not comparable)'], {}
    os.makedirs(out, exist_ok=True)
    rep = {'frames': want, 'views': shared, 'labels': labels, 'grids': {}, 'videos': {}}
    for v in shared:
        rep['grids'][v] = grid(fa[v], fb[v], v, want, labels, cell, os.path.join(out, f'grid_{v}.png'))
    if video:
        first, last = video_range(spec, fb)
        rep['video_range'] = [first, last]
        for v in shared:
            p = side_by_side(fa[v], fb[v], v, first, last, labels, cell, os.path.join(out, f'sb_{v}'))
            if p:
                rep['videos'][v] = p
    with open(os.path.join(out, 'montage.json'), 'w', encoding='utf-8') as fh:
        json.dump(rep, fh, indent=1)
    lines = [f'MONTAGE: OK {len(shared)} grids ({", ".join(shared)}), frames {want[0]}-{want[-1]} ({len(want)})']
    if rep['videos']:
        ext = os.path.splitext(next(iter(rep['videos'].values())))[1]
        lines.append(f'video: {len(rep["videos"])}x side-by-side{ext} f{rep["video_range"][0]}-f{rep["video_range"][1]}'
                     + ('' if ext == '.mp4' else ' (no ffmpeg: animated WebP)'))
    lines.append(f'out: {out}')
    return True, lines, rep


def _opt(argv, name, cast=str, default=None):
    if name in argv:
        i = argv.index(name); v = argv[i + 1]; del argv[i:i + 2]
        return cast(v)
    return default


def main(argv):
    argv = list(argv)
    spec_p = _opt(argv, '--spec')
    frames = _opt(argv, '--frames')
    hit = _opt(argv, '--hit', int)
    faf = _opt(argv, '--faf', int)
    around = _opt(argv, '--around', int)
    views = _opt(argv, '--views')
    out = _opt(argv, '--out')
    cell = _opt(argv, '--cell', int, 360)
    labels = _opt(argv, '--labels')
    video = '--no-video' not in argv
    if not video:
        argv.remove('--no-video')
    if len(argv) != 3 or (spec_p and not os.path.exists(spec_p)):
        print('usage: montage.py <base_cap> <var_cap> <sequence> [--spec s.json] [--frames a,b] [--hit N] [--faf N] [--around N]'
              ' [--views a,b] [--out d] [--cell N] [--labels "base,var"] [--no-video]')
        return 2
    spec = json.load(open(spec_p, encoding='utf-8')) if spec_p else {}
    if hit is not None or faf is not None or around is not None:
        ab = dict(spec.get('ab') or {})
        if hit is not None: ab['hit'] = hit
        if faf is not None: ab['faf'] = faf
        if around is not None: ab['around'] = around
        spec = dict(spec, ab=ab)
    ok, lines, _ = run(argv[0], argv[1], argv[2], spec,
                       frames=frames.split(',') if frames else None,
                       views=views.split(',') if views else None,
                       out=out, cell=cell, labels=labels.split(',') if labels else None, video=video)
    print('\n'.join(lines[:5]))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
