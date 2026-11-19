"""Stick-body renders of an offline dash take (frames.json from dash_ganon_gen.py), no GPU: one grid per view + a real-speed mp4.

    python dash_ganon_plot.py <outdir>          (needs numpy, matplotlib; ffmpeg on PATH for the mp4)

Views: side (camera on the character's LEFT, the dash goes to the right), back 3/4 (from behind-left, above), front.
The camera follows the root (the body stays in frame); ground ticks every 25 cm are fixed in the WORLD, so a planted foot must
stay on its tick. Writes grid_side.png, grid_q34.png, grid_front.png, dash_side_x1.mp4, dash_side_x4.mp4; prints max 3 lines.
"""
import json, os, subprocess, sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

OUT = sys.argv[1]
D = json.load(open(os.path.join(OUT, 'frames.json')))
FR = {int(k): {n: np.array(v) for n, v in j.items()} for k, j in D['frames'].items()}
ROOT = D['root']
N = len(FR)
SEG = [('pelvis', 'spine_03', 9), ('spine_03', 'spine_05', 11), ('spine_05', 'neck_01', 8), ('neck_01', 'head', 6)]
for s in 'lr':
    SEG += [('spine_05', f'clavicle_{s}', 5), (f'clavicle_{s}', f'upperarm_{s}', 5), (f'upperarm_{s}', f'lowerarm_{s}', 6),
            (f'lowerarm_{s}', f'hand_{s}', 5), ('pelvis', f'thigh_{s}', 7), (f'thigh_{s}', f'calf_{s}', 8),
            (f'calf_{s}', f'foot_{s}', 6), (f'foot_{s}', f'ball_{s}', 4)]


def world(j, i):
    """Blender world of the take (root at origin) -> world with the root's travel (forward = -Y)."""
    return {n: p + np.array([0, -ROOT[i], 0]) for n, p in j.items()}


def proj(view, p, i):
    x, y, z = p
    f, l = -y - ROOT[i], x                       # character axes, relative to the root (camera follows)
    if view == 'side':
        return f, z
    if view == 'front':
        return -l, z
    a = np.radians(35); e = np.radians(20)       # q34: from behind-left, above
    u = f * np.sin(a) - l * np.cos(a)            # forward goes right and away, the left side comes toward the camera
    v = z * np.cos(e) + (f * np.cos(a) + l * np.sin(a)) * np.sin(e)
    return u, v


def depth(view, n):
    side = n[-1] if n[-2:] in ('_l', '_r') else 'c'
    if view == 'side':       # camera on the left: left limbs in front
        return {'l': 3, 'c': 2, 'r': 1}[side]
    return {'l': 2, 'c': 2, 'r': 2}[side]


def draw(ax, view, i, title=True):
    j = world(FR[i], i)
    # ground ticks fixed in the world (every 25 cm)
    for k in range(-4, 20):
        wf = k * 0.25
        if view == 'side':
            ax.plot([wf - ROOT[i]] * 2, [0, -0.03], color='0.55', lw=1)
    ax.axhline(0, color='0.4', lw=1)
    segs = sorted(SEG, key=lambda s: depth(view, s[1]))
    for a, b, w in segs:
        pa, pb = proj(view, j[a], i), proj(view, j[b], i)
        side = b[-1] if b[-2:] in ('_l', '_r') else 'c'
        col = {'l': '#2f6db5', 'r': '#c0452f', 'c': '#555555'}[side]
        ax.plot([pa[0], pb[0]], [pa[1], pb[1]], color=col, lw=w * 0.9, solid_capstyle='round')
    h = proj(view, j['head'], i)
    ax.add_patch(plt.Circle((h[0], h[1] + 0.09), 0.11, color='#555555'))
    if view == 'side':  # nose: which way the head looks
        n0 = np.array(h) + np.array([0, 0.09])
        ax.plot([n0[0], n0[0] + 0.15 * np.cos(np.radians(-10))], [n0[1], n0[1] - 0.02], color='#555555', lw=3)
    for s in 'lr':
        p = proj(view, j[f'hand_{s}'], i)
        ax.add_patch(plt.Circle(p, 0.05, color={'l': '#2f6db5', 'r': '#c0452f'}[s]))
    ax.set_xlim(-1.0, 1.0); ax.set_ylim(-0.1, 1.75); ax.set_aspect('equal'); ax.set_xticks([]); ax.set_yticks([])
    if title:
        ax.set_title(f'f{i + 1}', fontsize=9, pad=2)


def grid(view):
    cols = 5; rows = (N + cols - 1) // cols
    fig, axs = plt.subplots(rows, cols, figsize=(cols * 2.2, rows * 2.0))
    for k, ax in enumerate(axs.flat):
        if k < N:
            draw(ax, view, k)
        else:
            ax.axis('off')
    fig.suptitle(f'dash r: {view} (blue = left, red = right; frames from 1, 60 fps)', fontsize=10)
    fig.tight_layout(); fig.savefig(os.path.join(OUT, f'grid_{view}.png'), dpi=110); plt.close(fig)


for v in ('side', 'q34', 'front'):
    grid(v)
cells = os.path.join(OUT, 'cells'); os.makedirs(cells, exist_ok=True)
for i in range(N):
    fig, ax = plt.subplots(figsize=(6.4, 6.4))
    draw(ax, 'side', i); fig.tight_layout(); fig.savefig(os.path.join(cells, f'side_{i:02d}.png'), dpi=150); plt.close(fig)
for rate, tag in ((60, 'x1'), (15, 'x4')):
    # 2 s of idle before (frame 1 held) so the start reads, last frame held 0.5 s
    lst = os.path.join(cells, f'list_{tag}.txt')
    with open(lst, 'w') as fh:
        fh.write(f"file 'side_00.png'\nduration {30 / rate:.4f}\n")
        for i in range(N):
            fh.write(f"file 'side_{i:02d}.png'\nduration {1 / rate:.4f}\n")
        fh.write(f"file 'side_{N - 1:02d}.png'\nduration 0.5\nfile 'side_{N - 1:02d}.png'\n")
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'concat', '-safe', '0', '-i', lst, '-vf', 'fps=60,format=yuv420p',
                    '-c:v', 'libx264', '-crf', '18', os.path.join(OUT, f'dash_side_{tag}.mp4')], check=False)
print('grids + mp4 in', OUT)
