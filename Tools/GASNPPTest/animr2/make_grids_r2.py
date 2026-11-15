"""python Saved/AnimR2/make_grids_premerge.py : three comparison grids (variant game/az270, base game/az270), cropped around the character"""
import sys, os, json
from PIL import Image, ImageDraw
P = 'C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59'
J = P + '/Saved/AnimR2/jab'
OUT = P + '/docs/notes/reviews/overlord-jab-v3-r2'
# crop boxes in the 480x480 captures. az270: review camera, pawn at the centre, 1.52 px/cm, forward = image left, dummy 95 cm ahead.
# game: the player camera, the character sits at x 150-285, y 205-435 (motion extent over the chain).
CROPS = {'game': (100, 170, 340, 450), 'az270': (20, 70, 330, 410)}
CELL_W = 240
GRIDS = {'g1_jab1_hit': ('jab1_jab2_hit', [21, 22, 23, 24, 25, 26, 27, 28, 30]),
         'g2_jab2_hit': ('jab1_jab2_hit', [33, 34, 35, 36, 38, 40, 41, 42, 44]),
         'g3_jab2_faf': ('jab1_jab2_faf', [46, 49, 52, 54, 56, 57, 58, 59, 60]),
         'g4_end': ('jab1_jab2', [62, 66, 70, 74, 78, 80, 82, 88, 100])}
ROWS = [('var', 'variant', 'game'), ('var', 'variant', 'az270'), ('base', 'base', 'game'), ('base', 'base', 'az270')]

def load(cap, seq):
    with open(f'{cap}/{seq}/frames.jsonl') as f:
        return {r['f']: r for r in map(json.loads, (l for l in f if l.strip()))}

def cell(cap, seq, view, row):
    box = CROPS[view]
    w, h = box[2] - box[0], box[3] - box[1]
    path = f"{cap}/{seq}/{view}/f{row['f']:03d}.png"
    img = Image.open(path).convert('RGB').crop(box) if os.path.exists(path) else Image.new('RGB', (w, h), (60, 0, 0))
    img = img.resize((CELL_W, round(h * CELL_W / w)), Image.LANCZOS)
    return img

for name, (seq, frames) in GRIDS.items():
    data = {k: load(f'{J}/{d}', seq) for k, d in (('var', 'variant'), ('base', 'base'))}
    heights = [round((CROPS[v][3] - CROPS[v][1]) * CELL_W / (CROPS[v][2] - CROPS[v][0])) for _, _, v in ROWS]
    sheet = Image.new('RGB', (CELL_W * len(frames), sum(heights)))
    y = 0
    for (tag, d, view), hh in zip(ROWS, heights):
        for i, f in enumerate(frames):
            r = data[tag][f]
            c = cell(f'{J}/{d}', seq, view, r)
            m = r.get('montage')
            mt = f"{m[0].replace('AM_Overlord', '')} {m[1]:.2f}" if m and m[0] != 'error' else 'no montage'
            dr = ImageDraw.Draw(c)
            dr.rectangle([0, 0, CELL_W, 13], fill=(0, 0, 0))
            dr.text((3, 1), f'{tag} {view} v{f} {mt}', fill=(255, 255, 0))
            sheet.paste(c, (i * CELL_W, y))
        y += hh
    sheet.save(f'{OUT}/{name}.jpg', quality=85)
    print(name, sheet.size)

# g0_zoom: variant only, jab1_jab2_hit, tight crops, 400 px wide
ZC = {'game': (110, 180, 330, 345), 'az270': (40, 80, 300, 290)}
ZW = 400
rows_v = load(f'{J}/variant', 'jab1_jab2_hit')
fr = [20, 21, 22, 28, 33, 42]
hs = [round((ZC[v][3] - ZC[v][1]) * ZW / (ZC[v][2] - ZC[v][0])) for v in ('game', 'az270')]
sh = Image.new('RGB', (ZW * 6, sum(hs)))
y = 0
for v, hh in zip(('game', 'az270'), hs):
    for i, f in enumerate(fr):
        r = rows_v[f]; b = ZC[v]
        c = Image.open(f"{J}/variant/jab1_jab2_hit/{v}/f{f:03d}.png").convert('RGB').crop(b).resize((ZW, hh), Image.LANCZOS)
        m = r.get('montage'); mt = f"{m[0].replace('AM_Overlord', '')} {m[1]:.2f}" if m and m[0] != 'error' else 'no montage'
        dr = ImageDraw.Draw(c); dr.rectangle([0, 0, ZW, 13], fill=(0, 0, 0)); dr.text((3, 1), f'var {v} v{f} {mt}', fill=(255, 255, 0))
        sh.paste(c, (i * ZW, y))
    y += hh
sh.save(f'{OUT}/g0_zoom.jpg', quality=85)
print('g0_zoom', sh.size)
