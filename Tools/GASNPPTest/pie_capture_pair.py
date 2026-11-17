"""Base vs variant on the SAME video frames (project rule 10): two pie_capture_grid sheets of one view stacked, base on top, variant below.
    python pie_capture_pair.py <base_capture_dir> <variant_capture_dir> <sequence> <view> <out.png> --from=F --to=F [--step=1] [--cell=200] [--vshift=N]
--vshift: the variant's frames are F+N (the same move frame when the press came N video frames later)
Prints one line. Needs Pillow and Tools/GASNPPTest/pie_capture_grid.py next to it."""
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw


def main(argv):
    base, var, seq, view, out = argv[:5]
    opts = dict(a[2:].split('=', 1) for a in argv[5:] if a.startswith('--'))
    f0, f1, step, cell = int(opts['from']), int(opts['to']), int(opts.get('step', 1)), int(opts.get('cell', 200))
    cols = (f1 - f0) // step + 1
    shift = int(opts.get('vshift', 0))
    grid = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pie_capture_grid.py')
    imgs = []
    with tempfile.TemporaryDirectory() as tmp:
        for tag, cap, d in (('BASE', base, 0), ('VARIANT', var, shift)):
            png = os.path.join(tmp, tag + '.png')
            subprocess.run([sys.executable, grid, 'sheet', cap, seq, view, png, f'--from={f0 + d}', f'--to={f1 + d}', f'--cols={cols}',
                            f'--cell={cell}', f'--step={step}'], check=True, stdout=subprocess.DEVNULL)
            im = Image.open(png).convert('RGB')
            band = Image.new('RGB', (im.width, 22), (20, 20, 20))
            ImageDraw.Draw(band).text((6, 4), f'{tag}  {seq}  {view}  v{f0 + d}-v{f1 + d}', fill=(255, 220, 0))
            both = Image.new('RGB', (im.width, im.height + 22))
            both.paste(band, (0, 0))
            both.paste(im, (0, 22))
            imgs.append(both)
    w = max(i.width for i in imgs)
    sheet = Image.new('RGB', (w, sum(i.height for i in imgs)), (0, 0, 0))
    y = 0
    for i in imgs:
        sheet.paste(i, (0, y))
        y += i.height
    sheet.save(out, quality=88) if out.lower().endswith('.jpg') else sheet.save(out)
    print(f'{out}: {cols} frames x 2 rows, {sheet.width}x{sheet.height}')


if __name__ == '__main__':
    main(sys.argv[1:])
