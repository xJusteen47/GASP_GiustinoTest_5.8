"""Side-by-side review grids of two Play captures (pie_capture_move.py): the older version on top, the new one below, the same MOVE frames in the
same columns (plain Python, outside the editor; Pillow). Each block is aligned on its own first frame of the move.
    python pie_capture_compare.py <old_capture> <new_capture> <out_dir> [--old-label=v2] [--new-label=v3]
Makes the six cmp_*.jpg of the jab review (see GRIDS). A grid whose old frames are missing is made with the new version only.
Cells: the same crop box for every cell of a grid (the union of what moves in the view, in both versions), JPEG quality 85, at most 1560 x 1560.
Labels: "f<N>" = frame of the move (the running timeline's frame), "v<N>" = video frame (60 fps), "a<N>" = animation frame the montage shows."""
import os
import sys

from PIL import Image, ImageChops, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pie_capture_grid as pg   # noqa: E402

MAX_PX, QUALITY, PAD = 1560, 85, 14
PROFILE, GAME = 'az270', 'game'
# name, sequence, timeline index of the jab in the kit's table, view, first and last move frame, step, columns
GRIDS = [
    ('cmp_jab1_profile', 'jab1', 0, PROFILE, 0, 28, 1, 10),
    ('cmp_jab1_game', 'jab1', 0, GAME, 0, 28, 1, 10),
    ('cmp_jab2_profile', 'chain', 1, PROFILE, 0, 27, 1, 10),
    ('cmp_jab2_game', 'chain', 1, GAME, 0, 27, 1, 10),
    ('cmp_jab2_tail', 'chain', 1, PROFILE, 26, 52, 2, 7),
]


# Down-air set (--set=dair): the owner judges from BEHIND (the game camera) and from the SIDE, so every case has both. The aerial captures follow
# the pawn (it stays in the centre of the review cameras), so the crop is a fixed box round the centre, not the moved pixels (the floor moves).
# name, sequence, timeline index, view, first and last move frame, step, columns, crop (None = the whole picture)
BODY = (105, 60, 375, 420)
DAIR_GRIDS = [
    ('cmp_whiff_side', 'dair_whiff', 0, PROFILE, 0, 42, 1, 11, BODY),
    ('cmp_whiff_behind', 'dair_whiff', 0, 'az180', 0, 42, 1, 11, BODY),
    ('cmp_whiff_game', 'dair_whiff', 0, GAME, 0, 42, 1, 11, None),
    ('cmp_strike_side', 'dair_whiff', 0, PROFILE, 10, 21, 1, 12, BODY),
    ('cmp_strike_behind', 'dair_whiff', 0, 'az180', 10, 21, 1, 12, BODY),
    ('cmp_strike_front34', 'dair_whiff', 0, 'az45', 10, 21, 1, 12, BODY),
    ('cmp_land_side', 'dair_land', 0, PROFILE, 10, 42, 1, 11, BODY),
    ('cmp_land_game', 'dair_land', 0, GAME, 10, 42, 1, 11, None),
]
# name, view, crop: video frames round the hitstop of dair_hit
DAIR_HIT_GRIDS = [('cmp_hit_side', PROFILE, BODY), ('cmp_hit_behind', 'az180', BODY), ('cmp_hit_game', GAME, None)]


def move_rows(cap, seq, tl, lo, hi, step):
    """One row per wanted move frame (the nearest earlier row when the simulation skipped that frame), None when the capture has none"""
    if not os.path.exists(os.path.join(cap, seq, 'frames.jsonl')):
        return None
    rows = [r for r in pg.rows_of(cap, seq) if r['tl'] == tl]
    out = []
    for k in range(lo, hi + 1, step):
        best = None
        for r in rows:
            if r['tlf'] <= k and (best is None or r['tlf'] > best['tlf']):
                best = r
        out.append((k, best))
    return out


def hit_rows(cap, seq, tl, before=3, after=3):
    """Video frames from `before` before the first hitstop frame of the timeline `tl` to `after` after the hitstop ends"""
    rows = pg.rows_of(cap, seq)
    held = [r['f'] for r in rows if r['tl'] == tl and r['hitstop']]
    if not held:
        return None
    first, last = held[0], held[-1]
    by_f = {r['f']: r for r in rows}
    return [(f, by_f[f]) for f in range(first - 1 - before, last + 1 + after) if f in by_f]


def _path(cap, seq, view, row):
    return os.path.join(cap, seq, view, f"f{row['f']:03d}.png")


def _moved_box(cap, seq, view, rows):
    """Bounding box of the pixels that change against the capture's first image (the character; the review cameras do not move)"""
    ref = Image.open(os.path.join(cap, seq, view, 'f000.png')).convert('RGB')
    box = None
    for _, r in rows:
        if r is None or not os.path.exists(_path(cap, seq, view, r)):
            continue
        diff = ImageChops.difference(ref, Image.open(_path(cap, seq, view, r)).convert('RGB')).convert('L').point(lambda v: 255 if v > 40 else 0)
        b = diff.getbbox()
        if b:
            box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    return box


def _union(a, b):
    return b if a is None else a if b is None else (min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3]))


def _crop_box(boxes, size):
    box = None
    for b in boxes:
        box = _union(box, b)
    if box is None:
        return (0, 0, size, size)
    return (max(0, box[0] - PAD), max(0, box[1] - PAD), min(size, box[2] + PAD), min(size, box[3] + PAD))


def _cell(cap, seq, view, row, crop, cw, ch, text):
    path = _path(cap, seq, view, row) if row else None
    if path and os.path.exists(path):
        img = Image.open(path).convert('RGB').crop(crop).resize((cw, ch), Image.LANCZOS)
    else:
        img = Image.new('RGB', (cw, ch), (60, 0, 0))
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, cw, 10], fill=(0, 0, 0))
    d.text((2, 0), text, fill=(255, 255, 0))
    return img


def build(blocks, seq_view, cols, out, crop='moved'):
    """blocks: [(title, capture dir, sequence, [(label text, row)])]; the crop is shared by all of them ('moved' = the box of what moves,
    None = the whole picture, or a fixed box)"""
    seq_view_first = Image.open(os.path.join(blocks[0][1], blocks[0][2], seq_view, 'f000.png'))
    size = seq_view_first.size[0]
    if crop == 'moved':
        crop = _crop_box([_moved_box(cap, seq, seq_view, rows) for _, cap, seq, rows in blocks], size)
    elif crop is None:
        crop = (0, 0, size, size)
    w, h = crop[2] - crop[0], crop[3] - crop[1]
    cw = MAX_PX // cols
    ch = round(cw * h / w)
    title_h, sheet_h = 14, 0
    layout = []
    for title, cap, seq, rows in blocks:
        n_rows = (len(rows) + cols - 1) // cols
        layout.append((sheet_h, n_rows))
        sheet_h += title_h + n_rows * ch
    if sheet_h > MAX_PX:   # too tall: smaller cells (still at most MAX_PX wide)
        scale = MAX_PX / sheet_h
        cw, ch = int(cw * scale), int(ch * scale)
        title_h, sheet_h, layout = 14, 0, []
        for title, cap, seq, rows in blocks:
            n_rows = (len(rows) + cols - 1) // cols
            layout.append((sheet_h, n_rows))
            sheet_h += title_h + n_rows * ch
    sheet = Image.new('RGB', (cols * cw, sheet_h), (20, 20, 20))
    d = ImageDraw.Draw(sheet)
    for (title, cap, seq, rows), (y0, _) in zip(blocks, layout):
        d.rectangle([0, y0, cols * cw, y0 + title_h - 1], fill=(40, 40, 120))
        d.text((4, y0 + 1), f'{title}  ({seq}, {seq_view}, capture {os.path.basename(cap)})', fill=(255, 255, 255))
        for i, (text, row) in enumerate(rows):
            sheet.paste(_cell(cap, seq, seq_view, row, crop, cw, ch, text), ((i % cols) * cw, y0 + title_h + (i // cols) * ch))
    sheet.save(out, quality=QUALITY)
    print(f'{os.path.basename(out)}: {sheet.size[0]}x{sheet.size[1]}, crop {crop}, blocks {[(b[0], len(b[3])) for b in blocks]}')


def _label(row, k=None, video=False):
    m = row.get('montage')
    a = f' a{m[1]:.1f}' if m and m[0] != 'error' else ''
    return (f'v{row["f"]}' if video else f'f{k}') + a + (' H' if row['hitstop'] else '')


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    o = pg.opts()
    old, new, out_dir = args
    ol, nl = o.get('old-label', 'v2'), o.get('new-label', 'v3')
    os.makedirs(out_dir, exist_ok=True)
    if o.get('set') == 'dair':
        for name, seq, tl, view, lo, hi, step, cols, crop in DAIR_GRIDS:
            blocks = []
            for title, cap in ((ol, old), (nl, new)):
                rows = move_rows(cap, seq, tl, lo, hi, step)
                if rows is None or not os.path.isdir(os.path.join(cap, seq, view)):
                    print(f'{name}: no {title} frames for {seq}, grid without them')
                    continue
                blocks.append((title, cap, seq, [(f'{title} {_label(r, k)}' if r else f'{title} f{k} -', r) for k, r in rows]))
            build(blocks, view, cols, os.path.join(out_dir, name + '.jpg'), crop)
        for name, view, crop in DAIR_HIT_GRIDS:
            blocks = []
            for title, cap in ((ol, old), (nl, new)):
                rows = hit_rows(cap, 'dair_hit', 0, before=5, after=8)
                if rows:
                    blocks.append((title, cap, 'dair_hit', [(f'{title} {_label(r, video=True)}', r) for _, r in rows]))
            if blocks:
                build(blocks, view, min(max(len(b[3]) for b in blocks), 12), os.path.join(out_dir, name + '.jpg'), crop)
            else:
                print(f'{name}: no hitstop frame in dair_hit of either capture (the hit did not connect)')
        return
    for name, seq, tl, view, lo, hi, step, cols in GRIDS:
        blocks = []
        for title, cap in ((ol, old), (nl, new)):
            rows = move_rows(cap, seq, tl, lo, hi, step)
            if rows is None or not os.path.isdir(os.path.join(cap, seq, view)):
                print(f'{name}: no {title} frames for {seq}, grid without them')
                continue
            blocks.append((title, cap, seq, [(f'{title} {_label(r, k)}' if r else f'{title} f{k} -', r) for k, r in rows]))
        build(blocks, view, cols, os.path.join(out_dir, name + '.jpg'))
    blocks = []
    for title, cap in ((ol, old), (nl, new)):
        rows = hit_rows(cap, 'chain_hit', 1) if os.path.isdir(os.path.join(cap, 'chain_hit', PROFILE)) else None
        if rows:
            blocks.append((title, cap, 'chain_hit', [(f'{title} {_label(r, video=True)}', r) for _, r in rows]))
    cols = max(len(b[3]) for b in blocks)
    build(blocks, PROFILE, min(cols, 12), os.path.join(out_dir, 'cmp_hit_profile.jpg'))


if __name__ == '__main__':
    main()
