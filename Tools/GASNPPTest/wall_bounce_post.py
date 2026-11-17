"""mp4 + grid of a pie_wall_bounce.py capture (plain Python outside the editor; Pillow + ffmpeg).
    python wall_bounce_post.py <capture_dir> <out_dir>
Per run (weak, strong): <out>/<run>.mp4 (60 fps, every frame labelled: f, wall distance, speed into the wall, tags, montage) and <out>/<run>_grid.png
(36 frames from 6 before the contact, 6 x 6). Prints one line per run."""
import json
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw


def label(row):
    m = row.get('montage')
    return [f"f{row['f']}  wall {row['wall_cm']:.0f} cm  vin {row['vin']}", ' '.join(row['tags']) or '-',
            (f"{m[0]} @{m[1]}" if m else 'no montage')]


def stamp(img, row, scale=1.0):
    d = ImageDraw.Draw(img)
    for i, t in enumerate(label(row)):
        y = 4 + i * 14
        d.text((5, y + 1), t, fill=(0, 0, 0))
        d.text((4, y), t, fill=(255, 255, 0) if i == 1 and 'Bump' in t else (255, 255, 255))
    return img


def main(cap, out):
    os.makedirs(out, exist_ok=True)
    for run in ('weak', 'strong'):
        folder = os.path.join(cap, run)
        if not os.path.isdir(folder):
            continue
        rows = [json.loads(x) for x in open(os.path.join(folder, 'frames.jsonl')) if x.strip()]
        summ = json.load(open(os.path.join(folder, 'summary.json')))
        tmp = tempfile.mkdtemp()
        for r in rows:
            src = os.path.join(folder, 'side', f"f{r['f']:03d}.png")
            if os.path.exists(src):
                stamp(Image.open(src).convert('RGB'), r).save(os.path.join(tmp, f"f{r['f']:03d}.png"))
        mp4 = os.path.join(out, f'{run}.mp4')
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', '60', '-i', os.path.join(tmp, 'f%03d.png'), '-vf', 'scale=trunc(iw/2)*2:trunc(ih/2)*2', '-c:v', 'libx264',
                        '-pix_fmt', 'yuv420p', '-crf', '18', mp4], check=True)
        c = summ.get('contact_frame') or rows[len(rows) // 2]['f']
        pick = [r for r in rows if c - 6 <= r['f'] < c + 30]
        cw, ch = 320, 180
        grid = Image.new('RGB', (cw * 6, ch * 6), (20, 20, 20))
        for i, r in enumerate(pick[:36]):
            p = os.path.join(tmp, f"f{r['f']:03d}.png")
            if os.path.exists(p):
                grid.paste(Image.open(p).resize((cw, ch)), ((i % 6) * cw, (i // 6) * ch))
        grid.save(os.path.join(out, f'{run}_grid.png'))
        shutil.copy(os.path.join(folder, 'summary.json'), os.path.join(out, f'{run}_summary.json'))
        shutil.copy(os.path.join(folder, 'frames.jsonl'), os.path.join(out, f'{run}_frames.jsonl'))
        shutil.rmtree(tmp, ignore_errors=True)
        print(run, json.dumps(summ))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
