"""Clip-in-isolation check: judge ONE animation clip on its own, before any in-game work (owner 2026-11-19: a clip that fails
alone never passes in the full setup, however much the setup is iterated, and may break clips that work).

Offline, no editor, no GPU: needs `pip install bpy numpy matplotlib` (Blender module). Works in the cloud.

    python clip_check.py <clips.json> <outdir> [clip_name ...]

clips.json lists the clips (see clips.json next to this file). Per clip:
  "fbx":      path to the FBX (UE export, or our offline authoring output). Relative paths = relative to clips.json.
  "meta":     optional sidecar JSON written by export_clips_ue.py (curves, notifies, sync markers, root motion flags).
  "kind":     loop | start | stop | pivot | dash | crouch_enter | crouch_idle | crouch_exit | land | air  (picks required curves)
  "root_motion": true when the game plays the clip's root motion; false = in place (the capsule moves on its own).
  "game_speed_cms": for in-place clips, the capsule speed the clip plays under (number, or [[t_s, cm/s], ...]).
  "loop":     true = first and last pose must join.
  "air":      true = airborne clip (no plant / floor checks).
  "start_ref" / "end_ref": {"fbx": path, "frame": "first" | "last" | int (from 1) | "nearest"} pose the clip takes over
              from / hands to; "nearest" = the reference's closest frame (a loop can be entered at any phase).
Outputs per clip in <outdir>/<clip>/: metrics.json (every number), grid.png (side sticks + foot/pop strips). Prints <= 5 lines.

What is measured (cm, deg, frames counted FROM 1 at the clip's own fps):
  tracks     bones whose local transform changes over the clip. 0 = the clip is empty (pose comes from elsewhere).
  plant      a foot is planted while its ball bone is within PLANT_CM of its standing height and moves < PLANT_VZ vertically.
  slide      root-motion clips: horizontal travel of the planted ball in the world during one plant (should be ~0).
             in-place clips: the planted ball must move backward at the capsule speed; slide = error vs game_speed_cms,
             lateral = sideways drift (always wrong).
  ground     lowest ball / foot height vs its standing height (negative = through the floor).
  pops       per-bone angular acceleration spikes (deg/s^2) that stand out from that bone's own motion; pelvis position jolts.
  seam       loop clips: pose and velocity gap between the last and the first frame.
  handoff    max local rotation gap on the main bones + feet position gap (relative to root) vs the reference pose.
  curves     required GASP curves/notifies for the kind (docs/notes/gasp-curves.md); needs the sidecar.
"""
import json, math, os, sys

import numpy as np

PLANT_CM = 3.0          # ball within 3 cm of its standing height
PLANT_VZ = 25.0         # and vertical speed under 25 cm/s
# Feet, calibrated on 13 GASP clips (idles, run loop, stops, start, pivots, landings; 2026-11-19 anim-setup-3):
# height alone calls a low GASP shuffle (ball +2 cm, 200 cm/s) a plant, so contact also needs a slow ball.
CONTACT_BAND_CM = 1.0   # ball within 1 cm of its standing height ...
CONTACT_VH = 30.0       # ... and slower than 30 cm/s over the ground = planted
SLIDE_FAIL_CM = 3.5     # planted ball creeping > 3.5 cm (GASP max 3.0: landing settle, pivot plant) ...
DRIFT_WINDOW_S = 0.25   # ... within 0.25 s (GASP landings settle 3 cm over 1.7 s: not a slide)
SKATE_FAIL_S = 0.2      # flat on the floor and moving for >= 0.2 s = skating (GASP max: 0.17 s, landing heavy shuffle)
SKATE_WARN_CM = 5.0     # shorter floor slides >= 5 cm = touchdown/lift-off sweeps (GASP pivots: up to 71 cm in 0.13 s): WARN
PEN_FAIL_CM = -1.5      # below standing height
STAND_BALL_CM = 3.2     # UEFN mannequin, foot flat on the floor: ball bone height (dash r3 f1, run start r3 left foot)
STAND_FOOT_CM = 9.4     # same, foot (ankle) bone height. Override per clip with "stand_ball_cm"/"stand_foot_cm".
SPIKE_DEG = 6.0         # pose off its neighbours' midway by > 6 deg (60 fps): WARN, one-frame extreme, look at it
FLIP_DEG = 45.0         # > 45 deg off (any fps): FAIL, a flick / flip no cycle needs
SNAP_DEG = 20.0         # one-frame step > 20 deg (60 fps) ...
SNAP_REL = 3.0          # ... and > 3x the steps before and after
PELVIS_JOLT = 25000.0   # pelvis position acceleration cm/s^2 at 30 fps (GASP landing impact: 20.7k) ...
POP_REL = 5.0           # ... and > 5x its median
POP_ABS = 6000.0        # plot reference line only (deg/s^2)
SEAM_FAIL_DEG = 3.0
HANDOFF_WARN_DEG = 10.0  # the blend into/out of the clip has to hide this: look at it
HANDOFF_WARN_CM = 5.0
HANDOFF_FAIL_DEG = 35.0  # GASP pivots end 27-32 deg / 44 cm off their run loop (motion matching blends it): beyond = a pop
HANDOFF_FAIL_CM = 45.0

FINGERS = ('thumb_', 'index_', 'middle_', 'ring_', 'pinky_')
KEY_BONES = ['pelvis', 'spine_01', 'spine_03', 'spine_05', 'neck_01', 'head', 'clavicle_l', 'upperarm_l', 'lowerarm_l', 'hand_l',
             'clavicle_r', 'upperarm_r', 'lowerarm_r', 'hand_r', 'thigh_l', 'calf_l', 'foot_l', 'ball_l', 'thigh_r', 'calf_r',
             'foot_r', 'ball_r']
SEG = [('pelvis', 'spine_03'), ('spine_03', 'spine_05'), ('spine_05', 'neck_01'), ('neck_01', 'head')]
for _s in 'lr':
    SEG += [('spine_05', f'clavicle_{_s}'), (f'clavicle_{_s}', f'upperarm_{_s}'), (f'upperarm_{_s}', f'lowerarm_{_s}'),
            (f'lowerarm_{_s}', f'hand_{_s}'), ('pelvis', f'thigh_{_s}'), (f'thigh_{_s}', f'calf_{_s}'), (f'calf_{_s}', f'foot_{_s}'),
            (f'foot_{_s}', f'ball_{_s}')]
REQUIRED = {  # docs/notes/gasp-curves.md: a missing curve reads 0
    # MoveData_Speed is NOT required: on our clips it makes Get_DynamicPlayRate play them at capsule speed / clip speed
    # (skid stalled at speed 0, landing-sync-1 item 3). GASP's own clips carry it; ours must not.
    'loop': ['contact_l', 'contact_r', 'phase'],
    'start': ['contact_l', 'contact_r', 'phase'],
    'stop': ['contact_l', 'contact_r'],
    'pivot': ['contact_l', 'contact_r', 'phase'],
    'dash': ['contact_l', 'contact_r'],
    'land': ['contact_l', 'contact_r'],
}


# ---------------------------------------------------------------- sampling (bpy)
def sample_fbx(path):
    """World positions (cm) of KEY_BONES + local quaternions/translations of every bone, every frame."""
    import bpy
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
    sc = bpy.context.scene
    act = arm.animation_data.action if arm.animation_data else None
    f0, f1 = (int(round(act.frame_range[0])), int(round(act.frame_range[1]))) if act else (1, 1)
    fps = sc.render.fps / sc.render.fps_base
    names = [b.name for b in arm.data.bones]
    rest = {}
    for n in KEY_BONES:
        if n in arm.data.bones:
            rest[n] = np.array(arm.matrix_world @ arm.data.bones[n].head_local) * 100.0
    out = {'fps': fps, 'frames': [], 'names': names, 'rest': rest, 'first_key': f0}
    for f in range(f0, f1 + 1):
        sc.frame_set(f)
        mw = arm.matrix_world
        fr = {'root': np.array(mw.translation) * 100.0, 'root_yaw': mw.to_euler().z, 'pos': {}, 'q': {}, 't': {}}
        for pb in arm.pose.bones:
            if pb.name in KEY_BONES:
                fr['pos'][pb.name] = np.array(mw @ pb.head) * 100.0
            m = pb.matrix if pb.parent is None else pb.parent.matrix.inverted() @ pb.matrix
            q = m.to_quaternion()
            fr['q'][pb.name] = np.array([q.w, q.x, q.y, q.z])
            fr['t'][pb.name] = np.array(m.translation)
        out['frames'].append(fr)
    return out


_REF_CACHE = {}


def ref_frames(spec, base):
    path = spec['fbx'] if os.path.isabs(spec['fbx']) else os.path.join(base, spec['fbx'])
    if path not in _REF_CACHE:
        _REF_CACHE[path] = sample_fbx(path)['frames']
    return _REF_CACHE[path]


def pose_gap(R, fr):
    """(max key-bone local rotation gap deg, bone, top3, feet gap cm relative to the root)"""
    g = sorted(((qangle(R['q'][n], fr['q'][n]), n) for n in KEY_BONES if n in R['q'] and n not in ('ball_l', 'ball_r')),
               reverse=True)
    fg = max(np.linalg.norm((fr['pos'][f'foot_{s}'] - fr['root']) - (R['pos'][f'foot_{s}'] - R['root'])) for s in 'lr')
    return g[0][0], g[0][1], [[n, round(a, 1)] for a, n in g[:3]], float(fg)


# ---------------------------------------------------------------- math helpers
def qangle(a, b):
    d = abs(float(np.dot(a, b)))
    return math.degrees(2 * math.acos(min(1.0, d)))


def qlog_step(a, b):
    """rotation vector (deg) taking a to b, in a's frame"""
    aw, ax, ay, az = a
    inv = np.array([aw, -ax, -ay, -az])
    w1, x1, y1, z1 = inv
    w2, x2, y2, z2 = b
    d = np.array([w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2, w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
                  w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2, w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2])
    if d[0] < 0:
        d = -d
    s = np.linalg.norm(d[1:])
    if s < 1e-9:
        return np.zeros(3)
    ang = 2 * math.atan2(s, d[0])
    return d[1:] / s * math.degrees(ang)


def runs(mask):
    out, start = [], None
    for i, m in enumerate(list(mask) + [False]):
        if m and start is None:
            start = i
        elif not m and start is not None:
            out.append((start, i - 1)); start = None
    return out


def speed_at(spec, t):
    if spec is None:
        return None
    if isinstance(spec, (int, float)):
        return float(spec)
    ts, vs = zip(*spec)
    return float(np.interp(t, ts, vs))


# ---------------------------------------------------------------- checks
def check(name, cfg, base, outdir):
    fbx = cfg['fbx'] if os.path.isabs(cfg['fbx']) else os.path.join(base, cfg['fbx'])
    S = sample_fbx(fbx)
    F, fps, N = S['frames'], S['fps'], len(S['frames'])
    dt = 1.0 / fps
    fails, warns, M = [], [], {'clip': name, 'fbx': cfg['fbx'], 'fps': fps, 'frames': N, 'kind': cfg.get('kind')}
    root = np.array([f['root'] for f in F])
    travel = root[-1] - root[0]
    M['root_travel_cm'] = [round(float(x), 1) for x in travel]
    rm = cfg.get('root_motion', bool(np.linalg.norm(travel[:2]) > 5))
    M['root_motion'] = rm
    # forward axis from the rest foot -> ball (character facing)
    if 'forward' in cfg:
        fwd = np.array(cfg['forward'], float)
    elif rm and np.linalg.norm(travel[:2]) > 5:
        fwd = travel.copy()
    else:   # perpendicular to the hips (thigh_l -> thigh_r) on frame 1, signed by the feet (foot -> ball)
        h = F[0]['pos']['thigh_r'] - F[0]['pos']['thigh_l']
        fwd = np.array([-h[1], h[0], 0.0])
        toes = sum(F[0]['pos'][f'ball_{s}'] - F[0]['pos'][f'foot_{s}'] for s in 'lr')
        if fwd @ toes < 0:
            fwd = -fwd
    fwd[2] = 0; fwd /= np.linalg.norm(fwd)
    lat = np.array([-fwd[1], fwd[0], 0.0])
    M['_fwd'] = fwd.tolist()

    # tracks
    moving = [n for n in S['names'] if max(qangle(F[0]['q'][n], f['q'][n]) for f in F) > 0.1
              or max(np.linalg.norm(F[0]['t'][n] - f['t'][n]) for f in F) > 0.001]
    M['animated_bones'] = len(moving)
    if not moving and np.linalg.norm(travel) < 0.1:
        fails.append('no bone tracks: the clip is empty')

    # feet: plant, slide, ground
    feet = {}
    for s in 'lr':
        ball = np.array([f['pos'][f'ball_{s}'] for f in F]); foot = np.array([f['pos'][f'foot_{s}'] for f in F])
        rb, rf = cfg.get('stand_ball_cm', STAND_BALL_CM), cfg.get('stand_foot_cm', STAND_FOOT_CM)
        hb = ball[:, 2] - rb
        vz = np.gradient(ball[:, 2]) * fps if N > 1 else np.zeros(N)
        plant = (hb < PLANT_CM) & (np.abs(vz) < PLANT_VZ)
        info = {'min_ball_cm': round(float(hb.min()), 2), 'min_ball_frame': int(hb.argmin()) + 1,
                'min_heel_cm': round(float((foot[:, 2] - rf).min()), 2), 'plants': []}
        if cfg.get('air'):   # airborne clip: the floor is not under the feet, plants and ground do not apply
            info['plants'] = []; plant[:] = False
            feet[s] = {'h': hb, 'plant': plant, 'ball': ball}; M[f'foot_{s}'] = info
            continue
        # Judge the feet against the GROUND, not the clip root: root-motion clips already move in the world; an in-place
        # clip is shifted by the capsule's travel (game_speed_cms along fwd), so a ball that should be planted stands still.
        g = ball.copy()
        if not rm:
            v_game = cfg.get('game_speed_cms')
            if v_game is None:
                plant[:] = False   # no capsule speed: plants cannot be judged (warned below)
            else:
                trav = np.cumsum([0.0] + [speed_at(v_game, (i - 0.5) * dt) * dt for i in range(1, N)])
                g = g + np.outer(trav, fwd)
        vh = np.r_[0.0, np.linalg.norm(np.diff(g[:, :2], axis=0), axis=1) * fps] if N > 1 else np.zeros(N)
        low = (hb < CONTACT_BAND_CM) & plant
        contact = low & (vh <= CONTACT_VH)
        plant = contact
        win = max(int(round(DRIFT_WINDOW_S * fps)), 1)
        ok_win = [(w[0], w[1]) for w in cfg.get('slide_ok', [])]
        def allowed(a, b):
            return any(a + 1 >= w0 and b + 1 <= w1 for w0, w1 in ok_win)
        for a, b in runs(contact):   # drift: a planted ball creeping (window, so a slow weight settle is not a slide)
            seg = g[a:b + 1, :2]
            d = max(np.linalg.norm(seg[i:i + win + 1] - seg[i], axis=1).max() for i in range(len(seg)))
            rec = {'frames': [a + 1, b + 1], 'drift_cm': round(float(d), 2)}
            if d > SLIDE_FAIL_CM:
                (warns if allowed(a, b) else fails).append(
                    f'foot {s} drifts {d:.1f} cm in {DRIFT_WINDOW_S:g} s while planted (f{a + 1}-{b + 1})')
            info['plants'].append(rec)
        for a, b in runs(low & (vh > CONTACT_VH)):   # skate: flat on the floor and travelling
            dur = (b - a + 1) / fps
            d = float(np.linalg.norm(g[b, :2] - g[max(a - 1, 0), :2]))
            if d < SKATE_WARN_CM:
                continue
            rec = {'frames': [a + 1, b + 1], 'skate_cm': round(d, 1), 'skate_s': round(dur, 3), 'vmax_cms': round(float(vh[a:b + 1].max()))}
            info.setdefault('skates', []).append(rec)
            msg = f'foot {s} skates {d:.1f} cm over {dur:.2f} s on the floor (f{a + 1}-{b + 1})'
            if dur >= SKATE_FAIL_S and not allowed(a, b):
                fails.append(msg)
            else:
                warns.append(msg + (' (slide_ok)' if allowed(a, b) else ', touchdown/lift-off length'))
        if hb.min() < PEN_FAIL_CM:
            fails.append(f'ball {s} {hb.min():.1f} cm through the floor at f{int(hb.argmin()) + 1}')
        if info['min_heel_cm'] < PEN_FAIL_CM - 1:
            fails.append(f'heel {s} {info["min_heel_cm"]:.1f} cm through the floor')
        feet[s] = {'h': hb, 'plant': plant, 'ball': g}
        M[f'foot_{s}'] = info
    if not rm and cfg.get('game_speed_cms') is None and not cfg.get('air'):
        warns.append('in-place clip without game_speed_cms: feet not judged against the ground')

    # pops: per-bone angular acceleration
    jerk = np.zeros((N, len(S['names'])))
    if N >= 3:
        for j, n in enumerate(S['names']):
            w = [qlog_step(F[i - 1]['q'][n], F[i]['q'][n]) * fps for i in range(1, N)]   # deg/s, between i-1 and i
            for i in range(1, N - 1):
                jerk[i, j] = np.linalg.norm(w[i] - w[i - 1]) * fps                    # deg/s^2 at frame i
    # pops = what the eye reads as a glitch, not fast motion:
    #   spike: frame i sits off the midway pose of its neighbours (one-frame flick out and back)
    #   snap:  the step into frame i is much bigger than the steps around it (a jump, e.g. a toe bend released in 1 frame)
    k = 60.0 / fps                                    # thresholds are for 60 fps; a 30 fps clip moves 2x as far per frame
    pops = []
    for n in S['names']:
        q = [f['q'][n] for f in F]
        st = [0.0] + [qangle(q[i - 1], q[i]) for i in range(1, N)]
        for i in range(1, N - 1):
            mid = q[i - 1] + q[i + 1] * (1 if np.dot(q[i - 1], q[i + 1]) >= 0 else -1)
            mid = mid / np.linalg.norm(mid)
            dev = qangle(mid, q[i])
            if dev > SPIKE_DEG * k:
                pops.append((dev, i + 1, n, 'spike'))
        for i in range(1, N):
            nb = max(st[i - 1] if i > 1 else 0.0, st[i + 1] if i + 1 < N else 0.0)
            if st[i] > SNAP_DEG * k and st[i] > SNAP_REL * max(nb, 1.0):
                pops.append((st[i], i + 1, n, 'snap'))
    pops.sort(reverse=True)
    pelv = np.array([f['pos']['pelvis'] for f in F])
    st30 = max(int(round(fps / 30.0)), 1)   # judged at 30 fps: a one-frame kink scales with fps, the threshold must not
    p30 = pelv[::st30]
    pacc = np.linalg.norm(np.diff(p30, 2, axis=0), axis=1) * 900.0 if len(p30) >= 3 else np.array([0.0])
    M['pelvis_acc_max_cms2'] = round(float(pacc.max()), 0)
    M['pelvis_acc_max_frame'] = (int(pacc.argmax()) + 1) * st30 + 1
    pmed = max(float(np.median(pacc)), 1.0)
    for i, a in enumerate(pacc):
        if a > PELVIS_JOLT and a > POP_REL * pmed:
            pops.append((a, (i + 1) * st30 + 1, 'pelvis(pos)', 'jolt cm/s2'))
    M['pops'] = [{'frame': p[1], 'bone': p[2], 'size': round(p[0], 1), 'type': p[3]} for p in pops[:16]]
    def group(sel):
        g = {}
        for p in sorted(sel, key=lambda p: p[1]):
            grp = 'fingers' if any(p[2].startswith(f) for f in FINGERS) else p[2]
            if p[1] not in g.get(grp, []):
                g.setdefault(grp, []).append(p[1])
        return g

    def text(g):
        return '; '.join(f'{b} f' + ','.join(map(str, fr)) for b, fr in list(g.items())[:4]) + \
            (f' (+{len(g) - 4} bones)' if len(g) > 4 else '')
    hard = [p for p in pops if p[3] != 'spike' or p[0] > FLIP_DEG]
    soft = [p for p in pops if p not in hard]
    M['pops_hard'], M['pops_soft'] = group(hard), group(soft)
    pop_frames = sorted({p[1] for p in hard})
    if hard:
        fails.append('pops: ' + text(M['pops_hard']))
    if soft:
        warns.append('one-frame extremes to look at: ' + text(M['pops_soft']))

    # loop seam
    if cfg.get('loop'):
        gap = max(qangle(F[0]['q'][n], F[-1]['q'][n]) for n in KEY_BONES if n in F[0]['q'])
        M['seam_max_deg'] = round(gap, 2)
        if gap > SEAM_FAIL_DEG:
            fails.append(f'loop seam: last vs first pose {gap:.1f} deg')

    # handoff
    for key in ('start_ref', 'end_ref'):
        if key not in cfg:
            continue
        RF = ref_frames(cfg[key], base)
        fr = F[0] if key == 'start_ref' else F[-1]
        sel = cfg[key].get('frame', 'first')
        if sel == 'nearest':   # a loop can be entered / left at any phase: take its closest frame
            gaps = [pose_gap(R, fr) for R in RF]
            ri = int(np.argmin([g[0] + g[3] for g in gaps]))
        else:
            ri = {'first': 0, 'last': len(RF) - 1}.get(sel, sel if isinstance(sel, str) else int(sel) - 1)
        mx, bone, top3, fg = pose_gap(RF[ri], fr)
        M[key] = {'ref': cfg[key]['fbx'], 'ref_frame': ri + 1, 'max_deg': round(mx, 1), 'bone': bone, 'top3': top3,
                  'feet_gap_cm': round(fg, 1)}
        if mx > HANDOFF_FAIL_DEG or fg > HANDOFF_FAIL_CM:
            fails.append(f'{key}: {bone} off by {mx:.0f} deg, feet {fg:.1f} cm (ref f{ri + 1})')
        elif mx > HANDOFF_WARN_DEG or fg > HANDOFF_WARN_CM:
            warns.append(f'{key}: {bone} off by {mx:.0f} deg, feet {fg:.1f} cm (ref f{ri + 1}), the blend must hide it')

    # curves / notifies
    meta = cfg.get('meta')
    req = REQUIRED.get(cfg.get('kind'), [])
    if meta:
        mp = meta if os.path.isabs(meta) else os.path.join(base, meta)
        md = json.load(open(mp)) if os.path.exists(mp) else None
        if md is None:
            warns.append(f'sidecar {meta} missing: curves not checked')
        else:
            have = {c.lower() for c in md.get('curves', {})}
            miss = [c for c in req if c.lower() not in have]
            M['curves_missing'] = miss
            M['notifies'] = md.get('notifies', [])
            if md.get('bone_tracks') == 0:
                fails.append('no bone tracks in the asset: the pose comes from elsewhere')
            # Enable Root Motion = the root track is extracted (locked in the pose). Outside montages the ABP
            # (RootMotionFromMontagesOnly) throws the extracted motion away, so "on" is harmless for an in-place clip (every
            # GASP clip has it on). Only "off" on a clip whose root travels is wrong: the mesh walks away from the capsule.
            if 'enable_root_motion' in md and not md['enable_root_motion'] and np.linalg.norm(travel[:2]) > 5:
                fails.append(f'root travels {np.linalg.norm(travel[:2]):.0f} cm but Enable Root Motion is off: the mesh leaves the capsule')
            if miss:
                fails.append('missing curves: ' + ', '.join(miss))
            for s in 'lr':  # contact curve vs geometric plant
                c = md.get('curves', {}).get(f'contact_{s}')
                if c and len(c) == N:
                    agree = float(np.mean((np.array(c) > 0.5) == feet[s]['plant']))
                    M[f'contact_{s}_agree'] = round(agree, 3)
                    if agree < 0.85:
                        warns.append(f'contact_{s} curve agrees with the foot only {agree:.0%}')
    elif req:
        warns.append('no sidecar: curves/notifies not checked (export_clips_ue.py)')

    M['fails'], M['warns'] = fails, warns
    M['verdict'] = 'FAIL' if fails else ('PASS*' if warns else 'PASS')
    od = os.path.join(outdir, name); os.makedirs(od, exist_ok=True)
    json.dump(M, open(os.path.join(od, 'metrics.json'), 'w'), indent=1, default=float)
    draw(name, S, feet, jerk, pop_frames, fwd, lat, M, os.path.join(od, 'grid.png'))
    return M


# ---------------------------------------------------------------- one image per clip
def draw(name, S, feet, jerk, pop_frames, fwd, lat, M, path):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    F, N, fps = S['frames'], len(S['frames']), S['fps']
    picks = sorted(set(np.linspace(0, N - 1, min(N, 10)).round().astype(int).tolist() + [p - 1 for p in pop_frames[:2]]))[:12]
    fig = plt.figure(figsize=(2.0 * len(picks), 7.4))
    gs = fig.add_gridspec(4, len(picks), height_ratios=[2.2, 1, 1, 1])
    x0 = F[0]['root'] @ fwd
    for k, i in enumerate(picks):
        ax = fig.add_subplot(gs[0, k]); f = F[i]
        cam = f['root'] @ fwd
        for t in range(-8, 40):  # world ticks every 25 cm: a planted foot stays on its tick
            ax.plot([x0 + t * 25 - cam] * 2, [0, -4], color='0.6', lw=1)
        ax.axhline(0, color='0.4', lw=1)
        for a, b in SEG:
            pa, pb = f['pos'][a], f['pos'][b]
            c = '#2f6db5' if b.endswith('_l') else '#c0452f' if b.endswith('_r') else '#555'
            ax.plot([pa @ fwd - cam, pb @ fwd - cam], [pa[2], pb[2]], color=c, lw=3 if b.endswith('_l') else 2.2,
                    solid_capstyle='round', zorder=3 if b.endswith('_l') else 2)
        h = f['pos']['head']; ax.add_patch(plt.Circle((h @ fwd - cam, h[2] + 9), 10, color='#555'))
        for s in 'lr':
            if feet[s]['plant'][i]:
                p = f['pos'][f'ball_{s}']; ax.plot(p @ fwd - cam, 0, 'v', color='#2f6db5' if s == 'l' else '#c0452f', ms=7)
        ax.set_xlim(-100, 100); ax.set_ylim(-8, 185); ax.set_aspect('equal'); ax.set_xticks([]); ax.set_yticks([])
        ax.set_title(f'f{i + 1}' + (' POP' if i + 1 in pop_frames else ''), fontsize=9, color='red' if i + 1 in pop_frames else 'k')
    fr = np.arange(1, N + 1)
    ax = fig.add_subplot(gs[1, :])
    for s, c in (('l', '#2f6db5'), ('r', '#c0452f')):
        ax.plot(fr, feet[s]['h'], color=c, label=f'ball {s} height vs standing (cm)')
        for a, b in runs(feet[s]['plant']):
            ax.axvspan(a + 0.5, b + 1.5, color=c, alpha=0.12)
    ax.axhline(0, color='0.5', lw=0.8); ax.axhline(PEN_FAIL_CM, color='red', lw=0.8, ls=':')
    ax.legend(fontsize=7, loc='upper right'); ax.set_xlim(0.5, N + 0.5); ax.tick_params(labelsize=7)
    ax = fig.add_subplot(gs[2, :])
    for s, c in (('l', '#2f6db5'), ('r', '#c0452f')):
        b = feet[s]['ball']; v = np.r_[0, np.linalg.norm(np.diff(b[:, :2], axis=0), axis=1) * fps]
        ax.plot(fr, np.where(feet[s]['plant'], v, np.nan), 'o-', color=c, ms=3, label=f'planted ball {s} ground speed (cm/s)')
    ax.legend(fontsize=7, loc='upper right'); ax.set_xlim(0.5, N + 0.5); ax.tick_params(labelsize=7)
    ax = fig.add_subplot(gs[3, :])
    body = [j for j, n in enumerate(S['names']) if not any(n.startswith(f) for f in FINGERS)]
    ax.plot(fr, jerk[:, body].max(axis=1), color='k', label='max body bone angular accel, no fingers (deg/s^2)')
    for p in pop_frames:
        ax.axvline(p, color='red', lw=1)
    ax.axhline(POP_ABS, color='red', lw=0.8, ls=':'); ax.legend(fontsize=7, loc='upper right')
    ax.set_xlim(0.5, N + 0.5); ax.set_xlabel(f'frame (from 1, {fps:g} fps)', fontsize=8); ax.tick_params(labelsize=7)
    head = f'{name}: {M["verdict"]}  |  ' + ('; '.join(M['fails'][:3]) or 'no fails') + ('' if not M['warns'] else '  |  ' + M['warns'][0])
    fig.suptitle(head, fontsize=9, color='red' if M['fails'] else 'green')
    fig.tight_layout(); fig.savefig(path, dpi=100); plt.close(fig)


if __name__ == '__main__':
    cj, outdir = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3:])
    C = json.load(open(cj)); base = os.path.dirname(os.path.abspath(cj))
    os.makedirs(outdir, exist_ok=True)
    rows = []
    for name, cfg in C['clips'].items():
        if name.startswith('_') or (only and name not in only):
            continue
        if not os.path.exists(cfg['fbx'] if os.path.isabs(cfg['fbx']) else os.path.join(base, cfg['fbx'])):
            why = 'run export_clips_ue.py first' if 'asset' in cfg else f'fbx not found: {cfg["fbx"]}'
            rows.append({'clip': name, 'verdict': 'NO SOURCE', 'fails': [why], 'warns': []}); continue
        rows.append(check(name, cfg, base, outdir))
    json.dump([{k: r.get(k) for k in ('clip', 'verdict', 'fails', 'warns')} for r in rows], open(os.path.join(outdir, 'summary.json'), 'w'),
              indent=1)
    for r in rows[:4]:
        print(f"{r['clip']}: {r['verdict']} {('- ' + r['fails'][0]) if r['fails'] else ''}")
    print(f'{len(rows)} clips, summary in {os.path.join(outdir, "summary.json")}')
