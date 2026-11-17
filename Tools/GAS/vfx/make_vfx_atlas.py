"""Polish M2: the packed shape atlas of the VFX kit (style sheet handoffs/stile-sheet-vfx-review.md, A "Materiale").

1024x512, 4x2 cells of 256. Channels: R = shape coverage (anti-aliased edge), G = fake light (1 on the lit top-left part, 0 in the
shade crescent: two flat tones, no gradient), B = erosion field (1 at the shape's deepest point, 0 on its edge: the master material
cuts B below a threshold that rises with age, so a shape dies by eroding from its edge, never by fading).
Cells: 0 ball, 1 ring, 2 star 4 points, 3 star 5 points, 4 drop (point up), 5 tapered streak (along V), 6 thin ring, 7 ball (no shade).
Stdlib only (zlib PNG writer): python Tools/GAS/vfx/make_vfx_atlas.py  -> Tools/GAS/vfx/T_GSTN_VFX_Atlas.png
"""
import math
import os
import struct
import zlib

CELL = 256
COLS, ROWS = 4, 2
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "T_GSTN_VFX_Atlas.png")


def poly_sdf(points):
    edges = list(zip(points, points[1:] + points[:1]))

    def sdf(x, y):
        best = 1e9
        inside = False
        for (ax, ay), (bx, by) in edges:
            ex, ey = bx - ax, by - ay
            wx, wy = x - ax, y - ay
            t = max(0.0, min(1.0, (wx * ex + wy * ey) / (ex * ex + ey * ey)))
            dx, dy = wx - ex * t, wy - ey * t
            best = min(best, dx * dx + dy * dy)
            if (ay > y) != (by > y) and x < ax + (y - ay) * ex / (ey if ey else 1e-9):
                inside = not inside
        d = math.sqrt(best)
        return -d if inside else d
    return sdf


def circle(r, cx=0.0, cy=0.0):
    return lambda x, y: math.hypot(x - cx, y - cy) - r


def ring(r_mid, half):
    return lambda x, y: abs(math.hypot(x, y) - r_mid) - half


def star(n, r_out, r_in, rot=-math.pi / 2):
    pts = []
    for i in range(2 * n):
        r = r_out if i % 2 == 0 else r_in
        a = rot + i * math.pi / n
        pts.append((r * math.cos(a), r * math.sin(a)))
    return poly_sdf(pts)


def drop():
    # Round bottom, point on top (y grows downwards in the image)
    pts = []
    r, cy = 0.52, 0.28
    for i in range(33):
        a = math.radians(-30 + 240 * i / 32)  # the bottom arc
        pts.append((r * math.cos(a), cy + r * math.sin(a)))
    pts.append((0.0, -0.92))
    return poly_sdf(pts)


def streak():
    # Tapered along V: wide rounded head at the bottom, needle tail at the top
    pts = []
    for i in range(17):
        a = math.radians(0 + 180 * i / 16)
        pts.append((0.22 * math.cos(a), 0.62 + 0.22 * math.sin(a)))
    pts.append((-0.02, -0.94))
    pts.append((0.02, -0.94))
    pts.reverse()
    return poly_sdf(pts)


# Shapes in cell units: -1..1 across the cell. Light offset: the lit part is the shape moved up-left by this much
SHAPES = [
    (circle(0.92), 0.16),
    (ring(0.78, 0.15), 0.0),
    (star(4, 0.95, 0.30, rot=-math.pi / 2), 0.10),
    (star(5, 0.95, 0.42), 0.12),
    (drop(), 0.14),
    (streak(), 0.0),
    (ring(0.86, 0.07), 0.0),
    (circle(0.92), 0.0),
]


def render_cell(sdf, light):
    px = 2.0 / CELL
    values = []
    deepest = 1e-6
    for j in range(CELL):
        y = -1 + (j + 0.5) * px
        row = []
        for i in range(CELL):
            x = -1 + (i + 0.5) * px
            d = sdf(x, y)
            lit = sdf(x + light * 0.7, y + light) if light > 0 else -1.0
            row.append((d, lit))
            deepest = max(deepest, -d)
        values.append(row)
    out = []
    for row in values:
        line = []
        for d, lit in row:
            cover = max(0.0, min(1.0, 0.5 - d / (1.5 * px)))
            g = 1.0 if lit < 0 else 0.0
            b = max(0.0, min(1.0, -d / deepest))
            line.append((int(cover * 255 + 0.5), int(g * 255), int(b * 255 + 0.5)))
        out.append(line)
    return out


def write_png(path, width, height, rows):
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in rows)
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    cells = [render_cell(sdf, light) for sdf, light in SHAPES]
    rows = []
    for r in range(ROWS):
        for j in range(CELL):
            line = []
            for c in range(COLS):
                line += cells[r * COLS + c][j]
            rows.append(line)
    write_png(OUT, CELL * COLS, CELL * ROWS, rows)
    print(OUT)


if __name__ == "__main__":
    main()
