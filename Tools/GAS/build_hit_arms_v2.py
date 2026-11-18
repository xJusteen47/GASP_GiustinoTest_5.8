"""Thread hit-arms-1, pose round 2 (Fable FAIL r1, docs/notes/reviews/hitarms-1/REVIEW-fable.md). Headless, idempotent:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<project>/Tools/GAS/build_hit_arms_v2.py" -unattended -nullrhi
/Game/GSTN/Animation/Reaction/AS_GSTN_React_HitStand_Arms<V> (default V2) from AS_GSTN_React_HitStand_ArmLow (owner 3A, not touched):
  the 4-key pose clip is stretched to LEN_S (body tracks hold their last key, the reaction rule holds the last frame anyway) so the arms can
  peak, settle and drift instead of being frozen. Shoulder swings are authored about the character's axes and calibrated by where the hand
  must move, so they do not depend on the bones' local axes. FK on local tracks only; the root and the body are not touched.
Then DA_ReactionAnims_HitArms (copy of DA_ReactionAnims_HitAdd) gets HitStand on the new clip. Log Saved/Reaction/hit_arms_v2.log.
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hand_poses as hp  # noqa: E402

VER = os.environ.get('HITARMS_VERSION') or 'V2'
LEAD = os.environ.get('HITARMS_LEAD') or 'l'
SRC = '/Game/GSTN/Animation/Reaction/AS_GSTN_React_HitStand_ArmLow'
DST = f'/Game/GSTN/Animation/Reaction/AS_GSTN_React_HitStand_Arms{VER}'
VSET_SRC = '/Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitAdd'
VSET = '/Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitArms'
LOG = os.path.join(unreal.Paths.project_saved_dir(), 'Reaction', 'hit_arms_v2.log')
FPS, FRAMES = 60, 55            # 54 intervals = 0.9 s: one full drift cycle (even interval count, 30 fps resample safe)
HOLD_S = 4 / 60.0               # the hitstop holds reaction frame 4 (thread 8oy91z): the peak lasts to there

# Hold values (degrees). Swings: flex = arm forward/up (+) or back (-) about the chest's side axis; hab = horizontal swing back (+);
# abd = arm away from the body sideways (+); twist = internal rotation of the upper arm (+ turns the forearm toward the belly).
ARMS = {
    'lead': {'flex': -20.0, 'hab': 28.0, 'abd': 15.0, 'twist': 20.0, 'elbow': 80.0, 'wrist': 33.0, 'shrug': 20.0, 'protract': 10.0,
             'hand': 'SplayedTense', 'hand_hold': 'Splayed', 'phase': 0.0},
    'trail': {'flex': -20.0, 'hab': 0.0, 'abd': 28.0, 'twist': 0.0, 'elbow': 60.0, 'wrist': 30.0, 'shrug': 12.0, 'protract': 10.0,
              'hand': 'SplayedTense', 'hand_hold': 'SplayedTense', 'phase': 0.22},
}
PEAK_ELBOW, PEAK_SWING = 15.0, 0.25      # at the hit: elbows 15 deg past the hold, shoulder swings 25 % past
SETTLE_TAU, SETTLE_PERIOD = 0.05, 0.22   # after the hitstop: damped settle, done by ~0.2 s
DRIFT_ELBOW, DRIFT_SWING, DRIFT_PERIOD = 5.5, 3.0, 0.9   # r3 (Fable): 2.5 was invisible
TENSE_S, RELAX_FROM = 0.15, 0.80
L = []


def log(s):
    L.append(str(s))


def settle(t):
    """1 through the hitstop, then a damped spring to 0 (one small undershoot)."""
    if t <= HOLD_S:
        return 1.0
    u = t - HOLD_S
    return math.exp(-u / SETTLE_TAU) * math.cos(2 * math.pi * u / SETTLE_PERIOD)


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def lq(lt):
    return (lt.rotation.x, lt.rotation.y, lt.rotation.z, lt.rotation.w)


def main():
    EAL = unreal.EditorAssetLibrary
    src = unreal.load_asset(SRC)
    skel = src.get_editor_property('skeleton')
    ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(ref)]
    cs, loc = {}, {}
    for b in names:
        w = unreal.AnimPoseExtensions.get_bone_pose(ref, b, unreal.AnimPoseSpaces.WORLD)
        cs[b] = (lq(w), (w.translation.x, w.translation.y, w.translation.z))
        loc[b] = lq(unreal.AnimPoseExtensions.get_bone_pose(ref, b, unreal.AnimPoseSpaces.LOCAL))
    up = (0.0, 0.0, 1.0)
    fwd = hp.norm(tuple(cs['ball_l'][1][i] - cs['foot_l'][1][i] + cs['ball_r'][1][i] - cs['foot_r'][1][i] for i in range(2)) + (0.0,))
    side = hp.norm(hp.cross(up, fwd))
    if hp.dot(hp.sub(cs['thigh_l'][1], cs['thigh_r'][1]), side) < 0:
        side = tuple(-x for x in side)        # side points to the character's left
    log(f'axes fwd {tuple(round(x, 2) for x in fwd)} left {tuple(round(x, 2) for x in side)}')

    # source keys of every track (4 keys), held after the last one
    n_src = src.get_editor_property('number_of_sampled_keys')
    tracks = [str(t) for t in unreal.AnimationLibrary.get_animation_track_names(src)]
    keys = {b: [unreal.AnimationLibrary.get_bone_pose_for_frame(src, b, min(f, n_src - 1), False) for f in range(n_src)] for b in tracks}

    parent = {}
    chainb = ['pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05']
    for i, b in enumerate(chainb[1:]):
        parent[b] = chainb[i]
    for s in ('l', 'r'):
        parent.update({f'clavicle_{s}': 'spine_05', f'upperarm_{s}': f'clavicle_{s}', f'lowerarm_{s}': f'upperarm_{s}', f'hand_{s}': f'lowerarm_{s}'})

    def fk(getl, bones):
        out = {'root': ((0, 0, 0, 1), (0.0, 0.0, 0.0))}
        parent['pelvis'] = 'root'
        for b in bones:
            lt = getl(b)
            pr, pp = out[parent[b]]
            tr = hp.qrot(pr, (lt.translation.x, lt.translation.y, lt.translation.z))
            out[b] = (hp.qmul(pr, lq(lt)), tuple(pp[i] + tr[i] for i in range(3)))
        return out

    arm_bones = chainb + [f'{x}_{s}' for s in ('l', 'r') for x in ('clavicle', 'upperarm', 'lowerarm', 'hand')]
    src_last = fk(lambda b: keys[b][-1], arm_bones)       # component pose of the held key (root at origin)

    # hinge axes fixed in the parent bone's frame (from the reference pose)
    hinge = {}
    for s in ('l', 'r'):
        ua = hp.norm(hp.sub(cs[f'lowerarm_{s}'][1], cs[f'upperarm_{s}'][1]))
        fa = hp.norm(hp.sub(cs[f'hand_{s}'][1], cs[f'lowerarm_{s}'][1]))
        npalm, _, _ = hp.palm_frame(cs, s)
        hd = hp.norm(hp.sub(cs[f'middle_01_{s}'][1], cs[f'hand_{s}'][1]))
        hinge[s] = (hp.qrot(hp.qinv(cs[f'upperarm_{s}'][0]), hp.norm(hp.cross(ua, fa))),
                    hp.qrot(hp.qinv(cs[f'lowerarm_{s}'][0]), hp.norm(hp.cross(hd, npalm))))

    role = {LEAD: 'lead', ('r' if LEAD == 'l' else 'l'): 'trail'}
    out = {s: s and 1.0 for s in ('l', 'r')}
    hands = {s: {k: hp.finger_locals(cs, loc, s, k) for k in ('Splayed', 'SplayedTense', 'Relaxed')} for s in ('l', 'r')}

    def swing_world(s, a, k, sp):
        """World rotation of the upper arm about the shoulder for the swings scaled by k, calibrated on the held pose."""
        sh = src_last[f'upperarm_{s}'][1]
        d = hp.norm(hp.sub(src_last[f'hand_{s}'][1], sh))
        outv = side if s == 'l' else tuple(-x for x in side)
        if hp.dot(hp.sub(sh, src_last['spine_05'][1]), outv) < 0:
            outv = tuple(-x for x in outv)

        def cal(axis, deg, want, probe=d):
            if not deg:
                return (0.0, 0.0, 0.0, 1.0)
            q = hp.qaxis(axis, abs(deg))
            moved = hp.qrot(q, probe)
            good = hp.dot(hp.sub(moved, probe), want) > 0
            return hp.qaxis(axis, abs(deg) * (1 if good else -1))
        back = tuple(-x for x in fwd)
        qs = [cal(side, a['flex'] * k, fwd if a['flex'] > 0 else back),
              cal(up, a['hab'] * k, back),
              cal(fwd, a['abd'] * k, hp.norm(tuple(outv[i] + 0.3 * up[i] for i in range(3))))]
        w = (0.0, 0.0, 0.0, 1.0)
        for q in qs:
            w = hp.qmul(q, w)
        if a['twist']:
            fa = hp.norm(hp.sub(src_last[f'hand_{s}'][1], src_last[f'lowerarm_{s}'][1]))
            axis = hp.qrot(w, d)
            tq = cal(axis, a['twist'] * k, tuple(-x for x in outv), probe=hp.qrot(w, fa))
            w = hp.qmul(tq, w)
        if sp:
            log(f'{s}: hand dir {tuple(round(x, 2) for x in d)} -> {tuple(round(x, 2) for x in hp.qrot(w, d))}')
        return w

    def clav_world(s, a, k):
        cl = hp.norm(hp.sub(src_last[f'upperarm_{s}'][1], src_last[f'clavicle_{s}'][1]))

        def cal(axis, deg, want):
            q = hp.qaxis(axis, deg)
            return q if hp.dot(hp.sub(hp.qrot(q, cl), cl), want) > 0 else hp.qaxis(axis, -deg)
        return hp.qmul(cal(up, a['protract'] * k, fwd), cal(hp.norm(hp.cross(cl, up)), a['shrug'] * k, up))

    new = {}
    for s in ('l', 'r'):
        a = ARMS[role[s]]
        el_ax, wr_ax = hinge[s]
        pclav = src_last[f'clavicle_{s}'][0]
        pspine = src_last['spine_05'][0]
        for f in range(FRAMES):
            t = f / FPS
            st = settle(t)
            dr = math.sin(2 * math.pi * (t / DRIFT_PERIOD + a['phase']))
            ks = 1.0 + PEAK_SWING * st + DRIFT_SWING * dr / 30.0
            kc = 1.0 + 0.3 * st
            lk = lambda b: keys[b][min(f, n_src - 1)]  # noqa: E731
            # clavicle: world rotation about its own pivot -> parent-local pre-multiply
            cw = clav_world(s, a, kc)
            new.setdefault(f'clavicle_{s}', []).append(hp.qmul(hp.qmul(hp.qinv(pspine), hp.qmul(cw, pspine)), lq(lk(f'clavicle_{s}'))))
            sw = swing_world(s, a, ks, f == 0)
            new.setdefault(f'upperarm_{s}', []).append(hp.qmul(hp.qmul(hp.qinv(pclav), hp.qmul(sw, pclav)), lq(lk(f'upperarm_{s}'))))
            el = a['elbow'] + PEAK_ELBOW * st + DRIFT_ELBOW * dr
            new.setdefault(f'lowerarm_{s}', []).append(hp.qmul(hp.qaxis(el_ax, el), lq(lk(f'lowerarm_{s}'))))
            wr = a['wrist'] + 5.0 * st + 0.6 * DRIFT_ELBOW * dr
            new.setdefault(f'hand_{s}', []).append(hp.qmul(hp.qaxis(wr_ax, -wr), lq(lk(f'hand_{s}'))))
            tense = 1.0 - smooth((t - HOLD_S) / (TENSE_S - HOLD_S)) if t > HOLD_S else 1.0
            relax = smooth((f / (FRAMES - 1) - RELAX_FROM) / (1 - RELAX_FROM))
            for b, qrel in hands[s]['Relaxed'].items():
                q = hp.qslerp(hands[s][a['hand_hold']][b], hands[s][a['hand']][b], tense)
                new.setdefault(b, []).append(hp.qslerp(q, qrel, relax))
    # write: a fresh copy stretched to FRAMES, every track rewritten
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    EAL.duplicate_asset(SRC, DST)
    seq = unreal.load_asset(DST)
    ctrl = seq.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('hit stand arms v2'), False)
    ctrl.set_frame_rate(unreal.FrameRate(FPS, 1), False)
    ctrl.set_number_of_frames(unreal.FrameNumber(FRAMES - 1), False)
    for b in tracks:
        pos = [keys[b][min(f, n_src - 1)].translation for f in range(FRAMES)]
        rots = [unreal.Quat(*new[b][f]) if b in new else keys[b][min(f, n_src - 1)].rotation for f in range(FRAMES)]
        sc = [unreal.Vector(1, 1, 1)] * FRAMES
        ctrl.set_bone_track_keys(b, pos, rots, sc, False)
    ctrl.close_bracket(False)
    EAL.save_asset(DST)
    seq = unreal.load_asset(DST)
    log(f'{DST}: {seq.get_editor_property("number_of_sampled_keys")} keys, {seq.get_play_length():.3f} s; tracks {len(tracks)}, '
        f'arm/finger tracks rewritten {len([b for b in tracks if b in new])}')
    # self-check: hand position vs shoulder, elbow bend, at the peak, after the settle and at the end
    for f in (0, 4, 15, FRAMES - 1):
        p = fk(lambda b: unreal.AnimationLibrary.get_bone_pose_for_frame(seq, b, f, False), arm_bones)
        msg = []
        for s in ('l', 'r'):
            ua = hp.norm(hp.sub(p[f'lowerarm_{s}'][1], p[f'upperarm_{s}'][1]))
            fa = hp.norm(hp.sub(p[f'hand_{s}'][1], p[f'lowerarm_{s}'][1]))
            rel = hp.sub(p[f'hand_{s}'][1], p[f'upperarm_{s}'][1])
            outv = side if s == 'l' else tuple(-x for x in side)
            msg.append(f'{role[s]} {s}: elbow {math.degrees(math.acos(max(-1, min(1, hp.dot(ua, fa))))):.0f} deg, hand from shoulder '
                       f'fwd {hp.dot(rel, fwd):+.0f} out {hp.dot(rel, outv):+.0f} up {rel[2]:+.0f} cm')
        log(f'frame {f}: ' + '; '.join(msg))
    p0 = src_last
    for s in ('l', 'r'):
        rel = hp.sub(p0[f'hand_{s}'][1], p0[f'upperarm_{s}'][1])
        outv = side if s == 'l' else tuple(-x for x in side)
        log(f'source held key {s}: hand from shoulder fwd {hp.dot(rel, fwd):+.0f} out {hp.dot(rel, outv):+.0f} up {rel[2]:+.0f} cm')
    # variant set
    if not EAL.does_asset_exist(VSET):      # a loaded set cannot be deleted headless: reuse it (HitStand is reset below)
        EAL.duplicate_asset(VSET_SRC, VSET)
    rset = unreal.load_asset(VSET)
    rules = list(rset.get_editor_property('rules'))
    hit = 0
    for r in rules:
        if str(r.get_editor_property('name')) == 'HitStand':
            r.set_editor_property('animation', seq)
            hit += 1
            log(f'HitStand rule end behaviour {r.get_editor_property("end_behaviour")}')
    rset.set_editor_property('rules', rules)
    EAL.save_loaded_asset(rset, False)
    log(f'{VSET}: copy of {VSET_SRC}, HitStand rules set {hit}')


try:
    aset = unreal.get_default_object(unreal.AnimationSettings)
    aset.set_editor_property('default_frame_rate', unreal.FrameRate(FPS, 1))
    main()
except Exception as ex:
    import traceback
    log('ERROR ' + repr(ex))
    log(traceback.format_exc())
os.makedirs(os.path.dirname(LOG), exist_ok=True)
open(LOG, 'w').write('\n'.join(L) + '\nDONE_MARK\n')
