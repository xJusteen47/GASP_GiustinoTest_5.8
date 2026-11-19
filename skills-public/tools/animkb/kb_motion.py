#!/usr/bin/env python3
"""kb_motion.py - measure "life" in a skeletal clip and draw annotated frame sheets.

Generic tool for the animation knowledge base (docs/knowledge). No engine, no project names.
Input: a BVH file (any skeleton; joints are matched by name patterns, see ROLE_PATTERNS).
FBX: convert first with fbx_to_bvh.py (Blender's bpy), then run this.

Commands
  metrics  <clip.bvh> [--out m.json]            life metrics (see docs/knowledge/L1-animation/09-measurement.md)
  lifeless <clip.bvh> --keys N --out x.bvh      rebuild the clip from N evenly spaced key poses with eased
                                                interpolation: the "script interpolated a few poses" failure,
                                                used as the counter-example in the figures
  figures  <clip.bvh> --kind K --out fig.png    K = spacing | overlap | arcs | contact | weight | strip
  compare  <a.bvh> <b.bvh> --out c.json         metrics side by side

Prints at most 5 lines; everything else goes to files.
"""
import argparse, json, math, re, sys
import numpy as np

# ---------------------------------------------------------------- BVH ---------------------------------------------
class Bvh:
    def __init__(self):
        self.names, self.parents, self.offsets, self.channels = [], [], [], []
        self.frames, self.dt = None, 1 / 30

    @staticmethod
    def load(path):
        b = Bvh(); txt = open(path).read().split('MOTION')
        toks = txt[0].replace('{', ' { ').replace('}', ' } ').split()
        stack, i, last = [], 0, -1
        while i < len(toks):
            t = toks[i]
            if t in ('ROOT', 'JOINT'):
                b.names.append(toks[i + 1]); b.parents.append(stack[-1] if stack else -1)
                b.offsets.append([0, 0, 0]); b.channels.append([]); last = len(b.names) - 1; i += 2; continue
            if t == 'End':  # end site: keep as a leaf joint so toe/finger tips exist
                b.names.append(b.names[stack[-1]] + '_End'); b.parents.append(stack[-1])
                b.offsets.append([0, 0, 0]); b.channels.append([]); last = len(b.names) - 1; i += 2; continue
            if t == '{': stack.append(last)
            elif t == '}': stack.pop()
            elif t == 'OFFSET': b.offsets[last] = [float(x) for x in toks[i + 1:i + 4]]; i += 3
            elif t == 'CHANNELS':
                n = int(toks[i + 1]); b.channels[last] = toks[i + 2:i + 2 + n]; i += 1 + n
            i += 1
        lines = [l for l in txt[1].strip().splitlines() if l.strip()]
        b.dt = float(lines[1].split(':')[1])
        b.frames = np.array([[float(x) for x in l.split()] for l in lines[2:]])
        b.offsets = np.array(b.offsets)
        return b

    def save(self, path, frames=None):
        frames = self.frames if frames is None else frames
        out, kids = ['HIERARCHY'], {}
        for j, p in enumerate(self.parents): kids.setdefault(p, []).append(j)
        def emit(j, d):
            ind = '\t' * d
            if self.names[j].endswith('_End') and not self.channels[j]:
                out.extend([f'{ind}End Site', f'{ind}{{', f'{ind}\tOFFSET ' + ' '.join(f'{v:.5f}' for v in self.offsets[j]), f'{ind}}}'])
                return
            out.append(f"{ind}{'ROOT' if self.parents[j] < 0 else 'JOINT'} {self.names[j]}"); out.append(f'{ind}{{')
            out.append(f'{ind}\tOFFSET ' + ' '.join(f'{v:.5f}' for v in self.offsets[j]))
            out.append(f'{ind}\tCHANNELS {len(self.channels[j])} ' + ' '.join(self.channels[j]))
            for k in kids.get(j, []): emit(k, d + 1)
            out.append(f'{ind}}}')
        emit(0, 0)
        out += ['MOTION', f'Frames: {len(frames)}', f'Frame Time: {self.dt:.7f}']
        out += [' '.join(f'{v:.5f}' for v in row) for row in frames]
        open(path, 'w').write('\n'.join(out) + '\n')

def _rot(axis, deg):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    if axis == 'X': return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
    if axis == 'Y': return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])

def local_rots(b):
    """per frame, per joint: 3x3 local rotation and local translation"""
    F, J = len(b.frames), len(b.names)
    R = np.tile(np.eye(3), (F, J, 1, 1)); T = np.tile(b.offsets, (F, 1, 1)).astype(float)
    col = 0
    for j, ch in enumerate(b.channels):
        for c in ch:
            v = b.frames[:, col]
            if c.endswith('position'):
                T[:, j, 'XYZ'.index(c[0])] = v + (b.offsets[j]['XYZ'.index(c[0])] if False else 0)
            else:
                ax = c[0]
                for f in range(F): R[f, j] = R[f, j] @ _rot(ax, v[f])
            col += 1
    return R, T

def world(b, R=None, T=None):
    if R is None: R, T = local_rots(b)
    F, J = R.shape[:2]
    WR = np.zeros_like(R); WP = np.zeros((F, J, 3))
    for j, p in enumerate(b.parents):
        if p < 0: WR[:, j] = R[:, j]; WP[:, j] = T[:, j]
        else:
            WR[:, j] = WR[:, p] @ R[:, j]
            WP[:, j] = WP[:, p] + np.einsum('fij,fj->fi', WR[:, p], T[:, j])
    return WP, WR

# ------------------------------------------------------------- roles -----------------------------------------------
ROLE_PATTERNS = {  # first match wins; covers CMU/Mixamo/UE-mannequin naming
    'pelvis': [r'^hips$', r'pelvis', r'hip$'],
    'spine':  [r'^spine$', r'spine_?0?1', r'spine1?$'],
    'chest':  [r'spine_?0?[3-5]', r'spine2', r'chest', r'^spine1$'],
    'head':   [r'^head$'],
    'l_shoulder': [r'^left_?arm$', r'upperarm_l', r'l_?upperarm', r'leftarm$'],
    'l_elbow': [r'leftforearm', r'lowerarm_l', r'forearm_l'],
    'l_wrist': [r'^lefthand$', r'hand_l$'],
    'r_shoulder': [r'^right_?arm$', r'upperarm_r', r'rightarm$'],
    'r_elbow': [r'rightforearm', r'lowerarm_r', r'forearm_r'],
    'r_wrist': [r'^righthand$', r'hand_r$'],
    'l_hip': [r'leftupleg', r'thigh_l'], 'l_knee': [r'^leftleg$', r'calf_l'], 'l_ankle': [r'^leftfoot$', r'foot_l$'],
    'l_toe': [r'lefttoebase$', r'ball_l$'],
    'r_hip': [r'rightupleg', r'thigh_r'], 'r_knee': [r'^rightleg$', r'calf_r'], 'r_ankle': [r'^rightfoot$', r'foot_r$'],
    'r_toe': [r'righttoebase$', r'ball_r$'],
}
def roles(b):
    out = {}
    low = [n.lower() for n in b.names]
    for r, pats in ROLE_PATTERNS.items():
        for p in pats:
            hit = [i for i, n in enumerate(low) if re.search(p, n)]
            if hit: out[r] = hit[0]; break
    return out

BONES = [('pelvis', 'spine'), ('spine', 'chest'), ('chest', 'head'), ('chest', 'l_shoulder'), ('l_shoulder', 'l_elbow'),
         ('l_elbow', 'l_wrist'), ('chest', 'r_shoulder'), ('r_shoulder', 'r_elbow'), ('r_elbow', 'r_wrist'),
         ('pelvis', 'l_hip'), ('l_hip', 'l_knee'), ('l_knee', 'l_ankle'), ('l_ankle', 'l_toe'),
         ('pelvis', 'r_hip'), ('r_hip', 'r_knee'), ('r_knee', 'r_ankle'), ('r_ankle', 'r_toe')]

def up_axis(WP, rl):
    d = WP[0, rl['head']] - (WP[0, rl['l_ankle']] + WP[0, rl['r_ankle']]) / 2
    return int(np.argmax(np.abs(d)))

# ------------------------------------------------------------ metrics ----------------------------------------------
def resample(WP, dt, fps=60.0):
    """resample positions to a common rate so frame-based numbers mean the same thing for every clip"""
    F = len(WP); t = np.arange(F) * dt; n = int(t[-1] * fps) + 1; tn = np.arange(n) / fps
    out = np.empty((n,) + WP.shape[1:])
    for idx in np.ndindex(WP.shape[1:]): out[(slice(None),) + idx] = np.interp(tn, t, WP[(slice(None),) + idx])
    return out

def life_metrics(b, fps=60.0):
    WP, WR = world(b); rl = roles(b); up = up_axis(WP, rl)
    P = resample(WP, b.dt, fps)
    height = np.ptp(P[0, :, up]) or 1.0
    unit = 100.0 / height  # % of body height
    hz = [a for a in range(3) if a != up]
    m = {'frames_60fps': len(P), 'units': '% of standing height; frames at 60 fps'}

    def speed(j): return np.linalg.norm(np.diff(P[:, j], axis=0), axis=1) * unit
    # limb paths are judged relative to the pelvis: steady travel of the whole body is not "linear spacing"
    Prel = P - P[:, rl['pelvis']:rl['pelvis'] + 1]
    def rspeed(j): return np.linalg.norm(np.diff(Prel[:, j], axis=0), axis=1) * unit
    # 1 spacing: longest run of near-equal steps of a moving effector (linear interpolation signature)
    sp = {}
    for r in ('l_wrist', 'r_wrist', 'head', 'l_ankle', 'r_ankle', 'pelvis'):
        if r not in rl: continue
        s = speed(rl[r]) if r == 'pelvis' else rspeed(rl[r]); moving = s > 0.15
        best = run = 0
        for k in range(1, len(s)):
            if moving[k] and moving[k - 1] and abs(s[k] - s[k - 1]) <= 0.06 * max(s[k], s[k - 1]): run += 1
            else: run = 0
            best = max(best, run)
        sp[r] = int(best + 1 if best else 0)
    m['even_spacing_run_max'] = sp
    # 2 arcs: straightness of effector paths between speed minima (1 = straight line)
    arcs = {}
    for r in ('l_wrist', 'r_wrist', 'head'):
        if r not in rl: continue
        p = Prel[:, rl[r]]; s = rspeed(rl[r])
        mins = [0] + [k for k in range(1, len(s) - 1) if s[k] < s[k - 1] and s[k] <= s[k + 1]] + [len(s)]
        devs = []
        for a, c in zip(mins[:-1], mins[1:]):
            seg = p[a:c + 1]; ch = seg[-1] - seg[0]; L = np.linalg.norm(ch)
            if L * unit < 3: continue
            u = ch / L; d = seg - seg[0]; perp = d - np.outer(d @ u, u)
            devs.append(float(np.max(np.linalg.norm(perp, axis=1)) / L))
        arcs[r] = round(float(np.median(devs)), 3) if devs else None
    m['arc_bulge_median'] = arcs
    # 3 overlap: lag (frames) of angular-speed peaks along chains; ~0 everywhere = everything moves together
    def ang_speed(j):
        # the joint's own rotation (relative to its parent): successive breaking of joints shows here
        p = b.parents[j]
        Rw = WR[:, j] if p < 0 else np.einsum('fji,fjk->fik', WR[:, p], WR[:, j]); ang = []
        for f in range(1, len(Rw)):
            d = Rw[f - 1].T @ Rw[f]; ang.append(math.degrees(math.acos(max(-1, min(1, (np.trace(d) - 1) / 2)))))
        a = np.array(ang) / b.dt * (1 / fps)
        t = np.arange(len(a)) * b.dt; tn = np.arange(int(t[-1] * fps) + 1) / fps
        return np.interp(tn, t, a)
    chains = {'arm_r': ['pelvis', 'chest', 'r_shoulder', 'r_elbow', 'r_wrist'],
              'arm_l': ['pelvis', 'chest', 'l_shoulder', 'l_elbow', 'l_wrist'],
              'spine': ['pelvis', 'spine', 'chest', 'head']}
    ov = {}
    for cname, ch in chains.items():
        if not all(c in rl for c in ch): continue
        curves = [ang_speed(rl[c]) for c in ch]
        lags = []
        for a, c in zip(curves[:-1], curves[1:]):
            a0 = a - a.mean(); c0 = c - c.mean(); n = len(a0); best, bl = -1e9, 0
            for L in range(-12, 13):
                x = a0[max(0, -L):n - max(0, L)]; y = c0[max(0, L):n - max(0, -L)]
                v = float(np.dot(x, y) / (np.linalg.norm(x) * np.linalg.norm(y) + 1e-9))
                if v > best: best, bl = v, L
            lags.append(bl)
        ov[cname] = lags
    m['overlap_lag_frames'] = ov
    # 4 synchrony: share of adjacent chain links that move with zero lag (lockstep = interpolated look)
    allz = [l for v in ov.values() for l in v]
    m['chain_zero_lag_share'] = round(float(np.mean([l == 0 for l in allz])), 2) if allz else None
    big = [rl[r] for r in ('pelvis', 'chest', 'head', 'l_elbow', 'r_elbow', 'l_wrist', 'r_wrist', 'l_knee', 'r_knee') if r in rl]
    # 4b stop synchrony: frames where most joints reach a speed minimum together (every joint eases into the
    # same key = pose-interpolated clip). Mocap and good hand animation stagger their stops.
    jl = [rl[r] for r in ('chest', 'head', 'l_shoulder', 'r_shoulder', 'l_elbow', 'r_elbow', 'l_wrist', 'r_wrist',
                          'l_hip', 'r_hip', 'l_knee', 'r_knee', 'l_ankle', 'r_ankle') if r in rl]
    mins = []
    for j in jl:
        a = np.convolve(ang_speed(j), np.ones(3) / 3, 'same'); thr = 0.35 * np.percentile(a, 90)
        mins.append({k for k in range(1, len(a) - 1) if a[k] <= a[k - 1] and a[k] <= a[k + 1] and a[k] < thr})
    shared = 0; total = 0
    for i, ms in enumerate(mins):
        for k in ms:
            total += 1
            n = sum(1 for o, mo in enumerate(mins) if o != i and any(abs(k - x) <= 1 for x in mo))
            shared += n >= 0.6 * (len(mins) - 1)
    m['stop_sync_share'] = round(shared / total, 2) if total else None
    # 5 holds: frames where the whole body is near-still; dead (no motion at all) vs moving hold
    body = np.mean([speed(j) for j in big], axis=0)
    m['dead_frames'] = int(np.sum(body < 0.02)); m['moving_hold_frames'] = int(np.sum((body >= 0.02) & (body < 0.15)))
    # 6 contacts and foot slide
    cont = {}
    for side in ('l', 'r'):
        k = f'{side}_toe' if f'{side}_toe' in rl else f'{side}_ankle'
        if k not in rl: continue
        h = (P[:, rl[k], up] - P[:, rl[k], up].min()) * unit; hs = np.linalg.norm(np.diff(P[:, rl[k]][:, hz], axis=0), axis=1) * unit
        c = (h[1:] < 2.0) & (hs < 0.6)
        slide = []
        k0 = None
        for f, v in enumerate(list(c) + [False]):
            if v and k0 is None: k0 = f
            if not v and k0 is not None:
                if f - k0 >= 4: slide.append(float(np.linalg.norm(P[f, rl[k]][hz] - P[k0 + 1, rl[k]][hz]) * unit))
                k0 = None
        cont[side] = {'contacts': len(slide), 'max_slide_pct_height': round(max(slide), 2) if slide else 0.0}
    m['foot_contacts'] = cont
    # 7 pops: acceleration spikes of the pelvis relative to the clip median
    acc = np.linalg.norm(np.diff(P[:, rl['pelvis']], 2, axis=0), axis=1) * unit
    m['pelvis_pop_ratio'] = round(float(acc.max() / (np.median(acc) + 1e-6)), 1)
    # 8 weight: pelvis vertical range and lateral shift (rhythm of weight transfer)
    pel = P[:, rl['pelvis']]
    m['pelvis_vertical_range'] = round(float(np.ptp(pel[:, up]) * unit), 2)
    return m

# ----------------------------------------------------------- lifeless ---------------------------------------------
def make_lifeless(b, keys, every=None):
    """N evenly spaced key poses; every channel eased between keys at the same time (smoothstep): the classic
    'script interpolated a few poses' clip. Root translation eased the same way."""
    F = len(b.frames)
    if every: keys = max(2, int(round((F - 1) * b.dt / every)) + 1)
    idx = np.linspace(0, F - 1, keys).round().astype(int)
    out = np.empty_like(b.frames)
    for a, c in zip(idx[:-1], idx[1:]):
        for f in range(a, c + 1):
            t = (f - a) / max(1, c - a); s = t * t * (3 - 2 * t)
            d = (b.frames[c] - b.frames[a] + 180) % 360 - 180
            pos_cols = _pos_cols(b)
            row = b.frames[a] + s * d
            row[pos_cols] = b.frames[a][pos_cols] + s * (b.frames[c][pos_cols] - b.frames[a][pos_cols])
            out[f] = row
    return out

def _pos_cols(b):
    cols, col = [], 0
    for ch in b.channels:
        for c in ch:
            if c.endswith('position'): cols.append(col)
            col += 1
    return cols

# ----------------------------------------------------------- figures ----------------------------------------------
def _plane(P, rl, up):
    hz = [a for a in range(3) if a != up]
    travel = P[-1, rl['pelvis']][hz] - P[0, rl['pelvis']][hz]
    if np.linalg.norm(travel) < 1e-3:  # in place: use the axis the wrists move most along
        w = np.concatenate([P[:, rl['r_wrist']][:, hz], P[:, rl['l_wrist']][:, hz]]); w = w - w.mean(0)
        travel = np.linalg.svd(w, full_matrices=False)[2][0]
    u = travel / np.linalg.norm(travel)
    def proj(x): return np.stack([x[..., hz[0]] * u[0] + x[..., hz[1]] * u[1], x[..., up]], -1)
    return proj

def figure(b, kind, out, title='', frames=None, compare=None, labels=('mocap', 'lifeless')):
    import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
    clips = [b] + ([compare] if compare is not None else [])
    rows = 2 if kind == 'spacing' else 1
    fig, axes = plt.subplots(len(clips), 1, figsize=(13, 3.2 * len(clips)), squeeze=False) if kind in ('spacing', 'strip') else plt.subplots(1, len(clips), figsize=(7 * len(clips), 5), squeeze=False)
    axes = axes.T if kind in ('spacing', 'strip') else axes
    for ci, (ax, c) in enumerate(zip(axes[0], clips)):
        WP, WR = world(c); rl = roles(c); up = up_axis(WP, rl); P = resample(WP, c.dt)
        unit = 100.0 / (np.ptp(P[0, :, up]) or 1); proj = _plane(P, rl, up)
        Q = proj(P) * unit; Q[..., 1] -= Q[..., 1].min()
        f0, f1 = frames if frames else (0, len(P) - 1)
        lab = labels[ci] if compare is not None else ''
        if kind == 'strip':
            step = max(1, (f1 - f0) // 9)
            xs = Q[f0:f1 + 1, :, 0]; span = np.ptp(xs) + 1e-6
            for n, f in enumerate(range(f0, f1 + 1, step)):
                off = 0 if span > 60 else n * 45 - Q[f, rl['pelvis'], 0]
                for a, z in BONES:
                    if a in rl and z in rl:
                        pa, pz = Q[f, rl[a]], Q[f, rl[z]]
                        col = '#c0392b' if a.startswith('r_') or z.startswith('r_') else ('#2471a3' if a.startswith('l_') or z.startswith('l_') else '#333')
                        ax.plot([pa[0] + off, pz[0] + off], [pa[1], pz[1]], color=col, lw=1.6)
                ax.text(Q[f, rl['pelvis'], 0] + off, -8, str(f - f0 + 1), ha='center', fontsize=7)
            ax.set_title(f'{title} {lab} - side view, every {step} f (60 fps, frame 1 = first)')
        elif kind == 'spacing':
            # 1-D spacing ruler: one tick per frame at the cumulative distance travelled by each effector
            # (relative to the pelvis). Even ticks = linear; ticks crowding at both ends of every segment = eased keys.
            for row, (r, col) in enumerate((('r_wrist', '#c0392b'), ('l_wrist', '#2471a3'), ('head', '#333'))):
                if r not in rl: continue
                rel = (P[f0:f1 + 1, rl[r]] - P[f0:f1 + 1, rl['pelvis']]) * unit
                d = np.r_[0, np.cumsum(np.linalg.norm(np.diff(rel, axis=0), axis=1))]
                ax.plot(d, np.full_like(d, -row), '|', color=col, ms=14, label=r)
                ax.plot([d[0], d[-1]], [-row, -row], color=col, lw=0.5)
                for k in range(0, len(d), 10): ax.text(d[k], -row + 0.25, str(k + 1), fontsize=6, ha='center')
            ax.set_ylim(-2.7, 0.8); ax.set_yticks([]); ax.legend(fontsize=7, loc='lower right')
            ax.set_xlabel('distance travelled relative to pelvis, % of height (one tick per frame; numbers = frame)')
            ax.set_title(f'{title} {lab}: spacing ruler')
        elif kind == 'arcs':
            for r, col in (('r_wrist', '#c0392b'), ('l_wrist', '#2471a3'), ('head', '#333'), ('r_ankle', '#e67e22'), ('l_ankle', '#16a085')):
                if r not in rl: continue
                q = Q[f0:f1 + 1, rl[r]]
                ax.plot(q[:, 0], q[:, 1], '-', color=col, lw=0.8, alpha=0.6)
                ax.plot(q[:, 0], q[:, 1], 'o', color=col, ms=2.0, label=r)
            ax.set_title(f'{title} {lab}: paths, one dot per frame')
            ax.legend(fontsize=7)
        elif kind == 'overlap':
            names = ['pelvis', 'chest', 'r_shoulder', 'r_elbow', 'r_wrist']
            for r in names:
                if r not in rl: continue
                Rw = WR[:, rl[r]]
                a = [0.0] + [math.degrees(math.acos(max(-1, min(1, (np.trace(Rw[f - 1].T @ Rw[f]) - 1) / 2)))) for f in range(1, len(Rw))]
                t = np.arange(len(a)) * c.dt; tn = np.arange(len(P)) / 60.0
                a = np.interp(tn, t, np.array(a) / c.dt / 60.0)[f0:f1 + 1]
                ax.plot(np.arange(len(a)) + 1, a, label=r); ax.plot(int(np.argmax(a)) + 1, a.max(), 'k|', ms=14)
            ax.set_xlabel('frame (60 fps)'); ax.set_ylabel('deg / frame (world)'); ax.legend(fontsize=7)
            ax.set_title(f'{title} {lab} - angular speed along the arm chain (| = peak)')
        elif kind == 'contact':
            for side, col in (('l', '#2471a3'), ('r', '#c0392b')):
                k = f'{side}_toe' if f'{side}_toe' in rl else f'{side}_ankle'
                h = (P[f0:f1 + 1, rl[k], up] - P[:, rl[k], up].min()) * unit
                ax.plot(np.arange(len(h)) + 1, h, color=col, label=f'{side} toe height')
                hs = np.r_[0, np.linalg.norm(np.diff(Q[f0:f1 + 1, rl[k], 0:1], axis=0), axis=1)]
                ax.plot(np.arange(len(hs)) + 1, hs * 5, color=col, ls=':', label=f'{side} toe fwd speed x5')
            pel = Q[f0:f1 + 1, rl['pelvis'], 1]
            ax.plot(np.arange(len(pel)) + 1, pel - pel.min(), 'k', lw=1, label='pelvis height (rel.)')
            ax.set_xlabel('frame (60 fps)'); ax.set_ylabel('% of height'); ax.legend(fontsize=7)
            ax.set_title(f'{title} {lab} - contacts: plant = height ~0 AND speed ~0')
        elif kind == 'weight':
            com = np.mean([Q[:, rl[r]] for r in ('pelvis', 'chest', 'spine') if r in rl], axis=0)
            fl = Q[:, rl['l_ankle']]; fr = Q[:, rl['r_ankle']]; x = np.arange(f1 - f0 + 1) + 1
            ax.fill_between(x, np.minimum(fl[f0:f1 + 1, 0], fr[f0:f1 + 1, 0]), np.maximum(fl[f0:f1 + 1, 0], fr[f0:f1 + 1, 0]),
                            color='#bbb', alpha=0.5, label='span between feet (forward axis)')
            ax.plot(x, com[f0:f1 + 1, 0], 'k', label='trunk mass centre (approx.)')
            ax.plot(x, fl[f0:f1 + 1, 0], color='#2471a3', lw=0.8, label='l ankle'); ax.plot(x, fr[f0:f1 + 1, 0], color='#c0392b', lw=0.8, label='r ankle')
            ax.set_xlabel('frame (60 fps)'); ax.set_ylabel('forward position, % of height'); ax.legend(fontsize=7)
            ax.set_title(f'{title} {lab} - mass centre against the base of support')
        if kind in ('strip', 'arcs'): ax.set_aspect('equal'); ax.axhline(0, color='#999', lw=0.5)
    fig.tight_layout(); fig.savefig(out, dpi=110); plt.close(fig)

# ------------------------------------------------------------- main -----------------------------------------------
def main():
    ap = argparse.ArgumentParser(); ap.add_argument('cmd'); ap.add_argument('clip'); ap.add_argument('clip2', nargs='?')
    ap.add_argument('--out'); ap.add_argument('--keys', type=int, default=5); ap.add_argument('--every', type=float); ap.add_argument('--kind', default='strip')
    ap.add_argument('--frames'); ap.add_argument('--title', default=''); ap.add_argument('--vs')
    ap.add_argument('--labels', default='mocap,lifeless')
    a = ap.parse_args(); b = Bvh.load(a.clip)
    fr = tuple(int(x) for x in a.frames.split('-')) if a.frames else None
    if a.cmd == 'metrics':
        m = life_metrics(b); json.dump(m, open(a.out or '/dev/stdout', 'w'), indent=1)
    elif a.cmd == 'lifeless':
        b.save(a.out, make_lifeless(b, a.keys, a.every)); print('wrote', a.out)
    elif a.cmd == 'figures':
        figure(b, a.kind, a.out, a.title, fr, Bvh.load(a.vs) if a.vs else None, tuple(a.labels.split(','))); print('wrote', a.out)
    elif a.cmd == 'compare':
        c = Bvh.load(a.clip2); r = {'a': life_metrics(b), 'b': life_metrics(c)}
        json.dump(r, open(a.out, 'w'), indent=1); print('wrote', a.out)

if __name__ == '__main__':
    main()
