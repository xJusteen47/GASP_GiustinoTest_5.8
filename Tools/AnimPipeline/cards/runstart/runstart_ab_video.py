# A/B videos of two in-game captures, same frames side by side (runstart-v23-desktop, 2026-11-21).
# usage: python runstart_ab_video.py <capA dir> <capB dir> <out dir> --a=22 --b=23 [--seq=mv_run_start_v5] [--from=77 --to=197]
#        [--views=az180,az90,game] [--fps=60] [--pivot-from=84 --pivot-to=116 --pivot-step=4]
# Writes ab_<a>_vs_<b>_<view>_real.mp4 (60 fps) and _slow4x.mp4 (15 fps) per view, plus grid_ab_<view>.jpg (every 15th frame,
# A over B) and grid_pivot_v<pf>_<pt>.jpg (dense pivot strip across views). Prints <= 3 lines.
import os, subprocess, sys
from PIL import Image, ImageDraw

a_dir, b_dir, out = sys.argv[1:4]
opt = dict((x[2:].split('=', 1) + [''])[:2] for x in sys.argv[4:] if x.startswith('--'))
la, lb = opt.get('a', 'A'), opt.get('b', 'B')
seq = opt.get('seq', 'mv_run_start_v5')
f0, f1 = int(opt.get('from', 77)), int(opt.get('to', 197))
pf0, pf1 = int(opt.get('pivot-from', 84)), int(opt.get('pivot-to', 116))
pstep = max(1, int(opt.get('pivot-step', 4)))
views = opt.get('views', 'az180,az90,game').split(',')
fps = int(opt.get('fps', 60))
os.makedirs(out, exist_ok=True)
tmp = os.path.join(out, '_ab_tmp'); os.makedirs(tmp, exist_ok=True)
made = []
pivot_rows = []
for v in views:
    pa, pb = os.path.join(a_dir, seq, v), os.path.join(b_dir, seq, v)
    frames = []
    for f in range(f0, f1 + 1):
        ia, ib = os.path.join(pa, 'f%03d.png' % f), os.path.join(pb, 'f%03d.png' % f)
        if not (os.path.exists(ia) and os.path.exists(ib)):
            continue
        A, B = Image.open(ia).convert('RGB'), Image.open(ib).convert('RGB')
        W = Image.new('RGB', (A.width + B.width, max(A.height, B.height)))
        W.paste(A, (0, 0)); W.paste(B, (A.width, 0))
        d = ImageDraw.Draw(W)
        d.text((8, 8), '%s  v%d  %s' % (la, f, v), fill=(255, 255, 0))
        d.text((A.width + 8, 8), '%s  v%d  %s' % (lb, f, v), fill=(255, 255, 0))
        p = os.path.join(tmp, '%s_%05d.png' % (v, len(frames)))
        W.save(p); frames.append((f, A, B))
    if not frames:
        continue
    for tag, r in (('real', fps), ('slow4x', max(1, fps // 4))):
        mp4 = os.path.join(out, 'ab_%s_vs_%s_%s_%s.mp4' % (la, lb, v, tag))
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(r), '-i', os.path.join(tmp, v + '_%05d.png'),
                        '-vf', 'pad=ceil(iw/2)*2:ceil(ih/2)*2', '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '20', mp4], check=True)
        made.append(mp4)
    pick = frames[::15]
    cw, ch = pick[0][1].width // 2, pick[0][1].height // 2
    G = Image.new('RGB', (cw * len(pick), ch * 2))
    d = ImageDraw.Draw(G)
    for i, (f, A, B) in enumerate(pick):
        G.paste(A.resize((cw, ch)), (i * cw, 0)); G.paste(B.resize((cw, ch)), (i * cw, ch))
        d.text((i * cw + 4, 4), '%s v%d' % (la, f), fill=(255, 255, 0)); d.text((i * cw + 4, ch + 4), '%s v%d' % (lb, f), fill=(255, 255, 0))
    G.save(os.path.join(out, 'grid_ab_%s.jpg' % v), quality=85)
    ppick = [(f, A, B) for f, A, B in frames if pf0 <= f <= pf1 and (f - pf0) % pstep == 0]
    if ppick:
        pivot_rows.append((v, ppick))
if pivot_rows:
    cw, ch = pivot_rows[0][1][0][1].width // 2, pivot_rows[0][1][0][1].height // 2
    cols = max(len(pp) for _, pp in pivot_rows)
    PG = Image.new('RGB', (cw * cols, ch * 2 * len(pivot_rows)))
    pd = ImageDraw.Draw(PG)
    for r_idx, (v, pp) in enumerate(pivot_rows):
        y0 = r_idx * ch * 2
        for i, (f, A, B) in enumerate(pp):
            PG.paste(A.resize((cw, ch)), (i * cw, y0))
            PG.paste(B.resize((cw, ch)), (i * cw, y0 + ch))
            pd.text((i * cw + 4, y0 + 4), '%s v%d %s' % (la, f, v), fill=(255, 255, 0))
            pd.text((i * cw + 4, y0 + ch + 4), '%s v%d %s' % (lb, f, v), fill=(255, 255, 0))
    PG.save(os.path.join(out, 'grid_pivot_v%d_%d.jpg' % (pf0, pf1)), quality=85)
for fn in os.listdir(tmp):
    os.remove(os.path.join(tmp, fn))
os.rmdir(tmp)
print('%d videos in %s' % (len(made), out))
