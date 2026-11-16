"""python Saved/AnimR2/make_grid_dropkick.py : Saved/AnimR2/dropkick/grid_premerge.jpg, 2 rows (game=behind, az90=side) x 12 columns, 200 px cells, crop follows the pawn"""
import json, numpy as np
from PIL import Image, ImageDraw
P = 'C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59/Saved/AnimR2/dropkick'
CAP = P + '/variant_premerge/dropkick_whiff'
rows = {r['f']: r for r in map(json.loads, (l for l in open(CAP + '/frames.jsonl') if l.strip()))}
mont = {f: r['montage'][1] for f, r in rows.items() if r.get('montage') and r['montage'][0] != 'error'}
V0 = min(mont)
S, L, LAND, LREP = 15, 11, 59, 4                      # startup frames, loop frames, first land frame (15 + 4 x 11 = 59)
TARGETS = [0, 2, 4, 12, 14, 15, 31, 58, 59, 61, 65, 75]
CW, CH, CROP_W, CROP_H = 200, 220, 260, 286

def label(m):
    r = int(round(m))
    if r < S: return f'S f{r}'
    if r == S: return 'S f15=L1 f0'
    if r < LAND: return f'L{(r - S) // L + 1} f{(r - S) % L}'
    return f'LAND f{r - LAND}'

def player_centres():
    """game camera is not at the pawn: grey-pixel (character) bbox in the strip x 120-260, horizon lines removed, median-smoothed"""
    out = {}
    for f in sorted(rows):
        a = np.array(Image.open(f'{CAP}/game/f{f:03d}.png').convert('RGB')).astype(int)
        m = ((a.max(2) - a.min(2)) <= 24) & (a.max(2) >= 25) & (a.max(2) < 235)
        m[:, :120] = False; m[:, 260:] = False
        m[m.sum(1) > 100] = False
        ys, xs = np.nonzero(m)
        out[f] = ((np.percentile(xs, 2) + np.percentile(xs, 98)) / 2, (np.percentile(ys, 2) + np.percentile(ys, 98)) / 2) if len(xs) > 200 else None
    fs = sorted(rows); res = {}
    for i, f in enumerate(fs):
        win = [out[g] for g in fs[max(0, i - 2):i + 3] if out[g]]
        res[f] = (float(np.median([w[0] for w in win])), float(np.median([w[1] for w in win])))
    return res

gc = player_centres()
cols = []
for t in TARGETS:
    v = min(mont, key=lambda f: abs(mont[f] - t))
    cols.append((v, mont[v]))
sheet = Image.new('RGB', (CW * len(cols), CH * 2))
for ri, view in enumerate(('game', 'az90')):
    for ci, (v, m) in enumerate(cols):
        cx, cy = gc[v] if view == 'game' else (240, 262)
        img = Image.open(f'{CAP}/{view}/f{v:03d}.png').convert('RGB')
        big = Image.new('RGB', (480 + 2 * CROP_W, 480 + 2 * CROP_H))
        big.paste(img, (CROP_W, CROP_H))
        x0, y0 = int(round(cx)) - CROP_W // 2 + CROP_W, int(round(cy)) - CROP_H // 2 + CROP_H
        cell = big.crop((x0, y0, x0 + CROP_W, y0 + CROP_H)).resize((CW, CH), Image.LANCZOS)
        d = ImageDraw.Draw(cell)
        d.rectangle([0, 0, CW, 24], fill=(0, 0, 0))
        d.text((3, 1), f'{view} v{v}  {label(m)}', fill=(255, 255, 0))
        d.text((3, 12), f'm {m:.1f}  h {rows[v]["h"]}', fill=(255, 255, 0))
        sheet.paste(cell, (ci * CW, ri * CH))
sheet.save(P + '/grid_premerge.jpg', quality=88)
print(sheet.size, [c[0] for c in cols])
