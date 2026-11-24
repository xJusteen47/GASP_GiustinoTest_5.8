"""Hitboxes drawn INTO the review pictures of a Play capture (plain Python, outside the editor; Pillow only).

Why: the scene captures of pie_capture_move.py do not render debug lines, so a run with gstn.Combat.Debug.Hitboxes 1 showed no hitbox
(dropkick K7 review, 2026-11-23). The capture now logs per frame the hitbox shapes (UGSTN_CombatSimComponent::GetDebugHitboxShapes,
world space on the presented mesh) as row['hb'] and every view's camera as row['cams'][view] = [[x, y, z], [pitch, yaw, roll], fov, size];
this module projects the shapes with that camera and draws them on the frame. Colours as the in-game draw: one per hitbox space, the
faded linger of a closed window at half brightness. A row without cameras or shapes draws nothing (has_overlay() says why).
"""
from __future__ import annotations

import math

SPACE_COLOURS = [(255, 0, 0), (255, 0, 255), (0, 255, 255), (128, 0, 128)]


def _axes(pitch: float, yaw: float, roll: float = 0.0):
    """Forward, right, up unit vectors of a UE rotator (degrees; UE world: X forward, Y right, Z up, left-handed)."""
    p, y, r = math.radians(pitch), math.radians(yaw), math.radians(roll)
    cp, sp, cy, sy, cr, sr = math.cos(p), math.sin(p), math.cos(y), math.sin(y), math.cos(r), math.sin(r)
    fwd = (cp * cy, cp * sy, sp)
    right = (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp)
    up = (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp)
    return fwd, right, up


def project(point, cam):
    """World point -> (x, y, depth) in pixels of a square capture of `size` (horizontal fov), or None behind the camera."""
    (loc, rot, fov, size) = cam[0], cam[1], float(cam[2]), int(cam[3])
    fwd, right, up = _axes(*rot)
    d = [point[i] - loc[i] for i in range(3)]
    x = sum(d[i] * fwd[i] for i in range(3))
    if x <= 1e-3:
        return None
    focal = (size / 2.0) / math.tan(math.radians(fov) / 2.0)
    return (size / 2.0 + focal * sum(d[i] * right[i] for i in range(3)) / x,
            size / 2.0 - focal * sum(d[i] * up[i] for i in range(3)) / x, x)


def _rotate(q, v):
    """Rotate v by the quaternion q = [x, y, z, w]."""
    qx, qy, qz, qw = q
    tx, ty, tz = 2 * (qy * v[2] - qz * v[1]), 2 * (qz * v[0] - qx * v[2]), 2 * (qx * v[1] - qy * v[0])
    return (v[0] + qw * tx + (qy * tz - qz * ty), v[1] + qw * ty + (qz * tx - qx * tz), v[2] + qw * tz + (qx * ty - qy * tx))


def shape_2d(shape, cam):
    """2D primitives of one logged shape: [('circle', cx, cy, r) | ('line', x0, y0, x1, y1) | ('poly', [(x, y)...])], pixels."""
    t, c, q, e = int(shape.get("t", 0)), shape["c"], shape.get("q", [0, 0, 0, 1]), shape["e"]
    size, fov = int(cam[3]), float(cam[2])
    focal = (size / 2.0) / math.tan(math.radians(fov) / 2.0)
    out = []
    if t == 0:
        p = project(c, cam)
        if p:
            out.append(("circle", p[0], p[1], focal * e[0] / p[2]))
    elif t == 1:
        half = max(0.0, e[2] - e[0])            # the segment between the two hemisphere centres
        a = _rotate(q, (0.0, 0.0, half))
        ends = [project([c[i] + a[i] for i in range(3)], cam), project([c[i] - a[i] for i in range(3)], cam)]
        ends = [p for p in ends if p]
        for p in ends:
            out.append(("circle", p[0], p[1], focal * e[0] / p[2]))
        if len(ends) == 2:
            (x0, y0, d0), (x1, y1, d1) = ends
            n = math.hypot(x1 - x0, y1 - y0) or 1.0
            nx, ny = -(y1 - y0) / n, (x1 - x0) / n
            r0, r1 = focal * e[0] / d0, focal * e[0] / d1
            out.append(("line", x0 + nx * r0, y0 + ny * r0, x1 + nx * r1, y1 + ny * r1))
            out.append(("line", x0 - nx * r0, y0 - ny * r0, x1 - nx * r1, y1 - ny * r1))
    else:
        pts = []
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    o = _rotate(q, (sx * e[0], sy * e[1], sz * e[2]))
                    p = project([c[i] + o[i] for i in range(3)], cam)
                    if p:
                        pts.append((p[0], p[1]))
        if len(pts) >= 3:
            out.append(("poly", _hull(pts)))
    return out


def _hull(pts):
    P = sorted(set(pts))
    if len(P) <= 2:
        return P

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, up = [], []
    for p in P:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], p) <= 0:
            lo.pop()
        lo.append(p)
    for p in reversed(P):
        while len(up) >= 2 and cross(up[-2], up[-1], p) <= 0:
            up.pop()
        up.append(p)
    return lo[:-1] + up[:-1]


def has_overlay(row, view):
    """(True, n shapes) when the row can be drawn on this view, else (False, reason)."""
    if row.get("hb_err"):
        return False, "editor without GetDebugHitboxShapes: rebuild"
    if not (row.get("cams") or {}).get(view):
        return False, "no camera logged for this view"
    return bool(row.get("hb")), len(row.get("hb") or [])


def draw(img, row, view, src_size=None):
    """Draw the row's hitboxes on a PIL image of this view (resized from the capture's `size`: the scale is applied). Returns count."""
    from PIL import ImageDraw
    cam = (row.get("cams") or {}).get(view)
    shapes = row.get("hb") or []
    if not cam or not shapes:
        return 0
    k = img.size[0] / float(src_size or cam[3])
    d = ImageDraw.Draw(img)
    n = 0
    for sh in shapes:
        col = SPACE_COLOURS[min(int(sh.get("s", 0)), 3)]
        width = 1 if sh.get("f") else 2
        if sh.get("f"):
            col = tuple(v // 2 for v in col)
        for prim in shape_2d(sh, cam):
            if prim[0] == "circle":
                _, x, y, r = prim
                d.ellipse([(x - r) * k, (y - r) * k, (x + r) * k, (y + r) * k], outline=col, width=width)
            elif prim[0] == "line":
                d.line([prim[1] * k, prim[2] * k, prim[3] * k, prim[4] * k], fill=col, width=width)
            else:
                d.polygon([(x * k, y * k) for x, y in prim[1]], outline=col)
        n += 1
    return n
