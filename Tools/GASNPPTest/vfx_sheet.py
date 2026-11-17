"""Contact sheets and videos of a VFX capture (Tools/GASNPPTest/pie_capture_vfx.py). Plain Python + Pillow; ffmpeg for the mp4.
    python vfx_sheet.py sheet <cap_dir> <sequence> <view> <out.png> --from=F --to=F [--step=1] [--cols=6] [--cell=240]
    python vfx_sheet.py pair <off_dir> <on_dir> <sequence> <view> <out.png> --from=F --to=F [--step=1] [--cols=6] [--cell=200]
        (each frame: VFX off above, VFX on below, same frame number: the rule 10 off/on comparison)
    python vfx_sheet.py video <off_dir> <on_dir> <sequence> <view> <out.mp4> [--fps=30]   (off | on side by side, every frame)
    python vfx_sheet.py gif <off_dir> <on_dir> <sequence> <view> <out.gif> --from=F --to=F [--cell=360] [--ms=33]   (no ffmpeg)
Prints one line.
"""
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw


def _opts(args):
    out = {}
    for a in args:
        if a.startswith('--') and '=' in a:
            k, v = a[2:].split('=', 1)
            out[k] = v
    return out


def _frame(cap, seq, view, f):
    path = os.path.join(cap, seq, view, f'f{f:03d}.png')
    return Image.open(path).convert('RGB') if os.path.exists(path) else None


def _label(img, text):
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, 9 * len(text) + 8, 18), fill=(0, 0, 0))
    d.text((4, 3), text, fill=(255, 255, 255))
    return img


def sheet(cap, seq, view, out, o):
    frames = list(range(int(o['from']), int(o['to']) + 1, int(o.get('step', 1))))
    cols, cell = int(o.get('cols', 6)), int(o.get('cell', 240))
    rows = (len(frames) + cols - 1) // cols
    canvas = Image.new('RGB', (cols * cell, rows * cell), (20, 20, 20))
    for i, f in enumerate(frames):
        img = _frame(cap, seq, view, f)
        if img:
            canvas.paste(_label(img.resize((cell, cell)), f'v{f}'), ((i % cols) * cell, (i // cols) * cell))
    canvas.save(out)
    return f'{out}: {len(frames)} frames'


def pair(off, on, seq, view, out, o):
    frames = list(range(int(o['from']), int(o['to']) + 1, int(o.get('step', 1))))
    cols, cell = int(o.get('cols', 6)), int(o.get('cell', 200))
    rows = (len(frames) + cols - 1) // cols
    canvas = Image.new('RGB', (cols * cell, rows * cell * 2 + rows * 6), (20, 20, 20))
    for i, f in enumerate(frames):
        x, y = (i % cols) * cell, (i // cols) * (cell * 2 + 6)
        a, b = _frame(off, seq, view, f), _frame(on, seq, view, f)
        if a:
            canvas.paste(_label(a.resize((cell, cell)), f'v{f} OFF'), (x, y))
        if b:
            canvas.paste(_label(b.resize((cell, cell)), f'v{f} ON'), (x, y + cell))
    canvas.save(out)
    return f'{out}: {len(frames)} frame pairs'


def video(off, on, seq, view, out, o):
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        return 'ffmpeg not found'
    names = sorted(n for n in os.listdir(os.path.join(on, seq, view)) if n.endswith('.png'))
    tmp = tempfile.mkdtemp()
    for i, n in enumerate(names):
        f = int(n[1:4])
        a, b = _frame(off, seq, view, f), _frame(on, seq, view, f)
        w = (a or b).width
        img = Image.new('RGB', (w * 2, (a or b).height))
        if a:
            img.paste(_label(a, f'v{f} OFF'), (0, 0))
        if b:
            img.paste(_label(b, f'v{f} ON'), (w, 0))
        img.save(os.path.join(tmp, f'{i:04d}.png'))
    subprocess.run([ffmpeg, '-y', '-loglevel', 'error', '-framerate', o.get('fps', '30'), '-i', os.path.join(tmp, '%04d.png'),
                    '-pix_fmt', 'yuv420p', '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2', out], check=True)
    shutil.rmtree(tmp, ignore_errors=True)
    return f'{out}: {len(names)} frames'


def gif(off, on, seq, view, out, o):
    """off | on side by side, every frame from..to, at half speed (33 ms a frame): the stand-in for the mp4 where ffmpeg is missing"""
    frames = range(int(o.get('from', 0)), int(o['to']) + 1)
    side = int(o.get('cell', 360))
    images = []
    for f in frames:
        a, b = _frame(off, seq, view, f), _frame(on, seq, view, f)
        if not (a and b):
            continue
        img = Image.new('RGB', (side * 2, side))
        img.paste(_label(a.resize((side, side)), f'v{f} OFF'), (0, 0))
        img.paste(_label(b.resize((side, side)), f'v{f} ON'), (side, 0))
        images.append(img.quantize(colors=128))
    images[0].save(out, save_all=True, append_images=images[1:], duration=int(o.get('ms', 33)), loop=0, optimize=True)
    return f'{out}: {len(images)} frames'


def main():
    a = sys.argv[1:]
    o = _opts(a)
    a = [x for x in a if not x.startswith('--')]
    if a[0] == 'sheet':
        print(sheet(a[1], a[2], a[3], a[4], o))
    elif a[0] == 'pair':
        print(pair(a[1], a[2], a[3], a[4], a[5], o))
    elif a[0] == 'gif':
        print(gif(a[1], a[2], a[3], a[4], a[5], o))
    elif a[0] == 'video':
        print(video(a[1], a[2], a[3], a[4], a[5], o))


if __name__ == '__main__':
    main()
