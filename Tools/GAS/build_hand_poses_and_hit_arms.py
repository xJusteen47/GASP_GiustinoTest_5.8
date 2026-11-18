"""Thread hit-arms-1 (2026-11-18). Headless, idempotent:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<project>/Tools/GAS/build_hand_poses_and_hit_arms.py" -unattended -nullrhi
1. Hand-pose library (hand_poses.py): /Game/GSTN/Animation/Hands/AS_GSTN_HandPoses, one frame per pose (frame 0 = reference pose), both
   hands, every other bone at the reference pose; and PA_GSTN_HandPoses, a pose asset made from it (poses named as in hand_poses.ORDER).
2. Hit stand arms variant: /Game/GSTN/Animation/Reaction/AS_GSTN_React_HitStand_Arms<V>, a copy of AS_GSTN_React_HitStand_ArmLow (owner
   decision 3A, not touched) with bent elbows, wrists bent back, splayed hands and a slight shoulder shrug, asymmetric (the arm that took
   the attacker's side leads), with a decaying wobble and a slow drift so the held frames are not frozen. FK on the local tracks only.
3. Variant set /Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitArms: a copy of DA_ReactionAnims_HitAdd with HitStand on the new clip.
   Nothing else in the game changes: captures use PIE_CAPTURE_REACTION_SET. DA_ReactionAnims and DA_ReactionAnims_HitAdd are not touched.
Env: HITARMS_VERSION (default V1), HITARMS_LEAD (l/r, default l = the arm lowered by 3A). Log Saved/Reaction/hit_arms.log (DONE_MARK).
"""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hand_poses as hp  # noqa: E402

VER = os.environ.get('HITARMS_VERSION') or 'V1'
LEAD = os.environ.get('HITARMS_LEAD') or 'l'
SRC = '/Game/GSTN/Animation/Reaction/AS_GSTN_React_HitStand_ArmLow'
DST = f'/Game/GSTN/Animation/Reaction/AS_GSTN_React_HitStand_Arms{VER}'
VSET_SRC = '/Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitAdd'
VSET = '/Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitArms'
HANDS_DIR = '/Game/GSTN/Animation/Hands'
LOG = os.path.join(unreal.Paths.project_saved_dir(), 'Reaction', 'hit_arms.log')
L = []

# Arm tuning, degrees. lead = the arm on the attacker's side (it was thrown out by the hit), trail = the other one.
ARMS = {
    'lead': {'elbow': 60.0, 'wrist': 28.0, 'shrug': 12.0, 'hand': 'SplayedTense', 'hand_hold': 'Splayed', 'delay': 0.0},
    'trail': {'elbow': 45.0, 'wrist': 18.0, 'shrug': 7.0, 'hand': 'Splayed', 'hand_hold': 'Splayed', 'hand_mix': 0.85, 'delay': 0.05},
}
WOBBLE_DEG, WOBBLE_DECAY, WOBBLE_PERIOD = 9.0, 0.30, 0.42     # settle after the snap (follow-through)
DRIFT_DEG, DRIFT_PERIOD = 3.0, 1.1                            # slow drift through the held frames
TENSE_SECONDS, RELAX_FROM = 0.12, 0.78                        # tense hand at the hit, relaxed over the last 22 % (no pop into locomotion)


def log(s):
    L.append(str(s))


def ref_data(skel):
    ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(ref)]
    cs, loc, lpos = {}, {}, {}
    for b in names:
        w = unreal.AnimPoseExtensions.get_bone_pose(ref, b, unreal.AnimPoseSpaces.WORLD)
        lt = unreal.AnimPoseExtensions.get_bone_pose(ref, b, unreal.AnimPoseSpaces.LOCAL)
        cs[b] = ((w.rotation.x, w.rotation.y, w.rotation.z, w.rotation.w),
                 (w.translation.x, w.translation.y, w.translation.z))
        loc[b] = (lt.rotation.x, lt.rotation.y, lt.rotation.z, lt.rotation.w)
        lpos[b] = lt.translation
    return names, cs, loc, lpos


def set_rate60():
    aset = unreal.get_default_object(unreal.AnimationSettings)
    try:
        aset.set_editor_property('default_frame_rate', unreal.FrameRate(60, 1))
    except Exception as ex:
        log(f'anim settings rate: {ex}')


def build_library(skel, names, cs, loc, lpos):
    set_rate60()
    path = f'{HANDS_DIR}/AS_GSTN_HandPoses'
    EAL = unreal.EditorAssetLibrary
    for p in (path, f'{HANDS_DIR}/PA_GSTN_HandPoses'):
        if EAL.does_asset_exist(p):
            EAL.delete_asset(p)
    fac = unreal.AnimSequenceFactory()
    fac.set_editor_property('target_skeleton', skel)
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset('AS_GSTN_HandPoses', HANDS_DIR, unreal.AnimSequence, fac)
    frames = [None] + hp.ORDER                 # 7 frames = 6 intervals (even, see ue-anim-python-gotchas)
    per = [{} for _ in frames]
    for i, pose in enumerate(frames):
        if pose:
            for s in ('l', 'r'):
                per[i].update(hp.finger_locals(cs, loc, s, pose, log if i == 1 else None))
    ctrl = seq.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('hand poses'), False)
    ctrl.set_frame_rate(unreal.FrameRate(60, 1), False)
    ctrl.set_number_of_frames(unreal.FrameNumber(len(frames) - 1), False)
    touched = set()
    for b in names:
        rots = [unreal.Quat(*per[i].get(b, loc[b])) for i in range(len(frames))]
        if any(b in per[i] for i in range(len(frames))):
            touched.add(b)
        ctrl.set_bone_track_keys(b, [lpos[b]] * len(frames), rots, [unreal.Vector(1, 1, 1)] * len(frames), False)
    ctrl.close_bracket(False)
    EAL.save_asset(path)
    log(f'{path}: {len(frames)} frames (0 = reference, then {hp.ORDER}); finger bones posed: {len(touched)}')
    missing = [b for f in hp.FINGERS + ('thumb',) for s in ('l', 'r') for b in [f'{x}_{s}' for x in hp.chain(f)] if b not in cs]
    if missing:
        log(f'MISSING finger bones: {missing}')
    # pose asset from the sequence (frame i -> pose i)
    try:
        pf = unreal.PoseAssetFactory()
        pf.set_editor_property('source_animation', unreal.load_asset(path))
        try:
            pf.set_editor_property('pose_names', ['Reference'] + hp.ORDER)
        except Exception as ex:
            log(f'pose_names not settable: {ex}')
        pa = unreal.AssetToolsHelpers.get_asset_tools().create_asset('PA_GSTN_HandPoses', HANDS_DIR, unreal.PoseAsset, pf)
        if pa:
            got = [str(n) for n in pa.get_pose_names()]
            want = ['Reference'] + hp.ORDER
            for old, new in zip(got, want):
                if old != new:
                    pa.rename_pose(old, new)
            EAL.save_asset(f'{HANDS_DIR}/PA_GSTN_HandPoses')
            log(f'PA_GSTN_HandPoses: poses {[str(n) for n in pa.get_pose_names()]}')
        else:
            log('PA_GSTN_HandPoses: factory returned None')
    except Exception as ex:
        log(f'PoseAsset failed: {ex!r}')


def fk(seq, frame, bones, parent, cs_root):
    """Component-space (rot, pos) of `bones` (ordered parents first) from the stored local tracks; unlisted ancestors from cs_root."""
    out = {}
    for b in bones:
        lt = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, b, frame, False)
        pr, pp = out.get(parent[b]) or cs_root[parent[b]]
        tr = hp.qrot(pr, (lt.translation.x, lt.translation.y, lt.translation.z))
        out[b] = (hp.qmul(pr, (lt.rotation.x, lt.rotation.y, lt.rotation.z, lt.rotation.w)), tuple(pp[i] + tr[i] for i in range(3)))
    return out


def wob(t):
    return WOBBLE_DEG * math.exp(-t / WOBBLE_DECAY) * math.sin(2 * math.pi * t / WOBBLE_PERIOD) + DRIFT_DEG * math.sin(2 * math.pi * t / DRIFT_PERIOD)


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def build_arms(names, cs, loc):
    EAL = unreal.EditorAssetLibrary
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    EAL.duplicate_asset(SRC, DST)
    seq = unreal.load_asset(DST)
    n = seq.get_editor_property('number_of_sampled_keys')
    dt = seq.get_play_length() / max(1, n - 1)
    up = (0.0, 0.0, 1.0)
    axes = {}
    for s in ('l', 'r'):
        ua_r, fa_r = cs[f'upperarm_{s}'], cs[f'lowerarm_{s}']
        ua = hp.norm(hp.sub(fa_r[1], ua_r[1]))
        fa = hp.norm(hp.sub(cs[f'hand_{s}'][1], fa_r[1]))
        bend = math.degrees(math.acos(max(-1.0, min(1.0, hp.dot(ua, fa)))))
        if bend < 3.0:
            raise RuntimeError(f'reference elbow {s} too straight ({bend:.1f} deg) for a hinge axis')
        # elbow hinge, fixed in the upper arm's frame: + about cross(ua, fa) increases the bend
        el = hp.qrot(hp.qinv(ua_r[0]), hp.norm(hp.cross(ua, fa)))
        # wrist: bend back = away from the palm, about cross(hand dir, palm normal), fixed in the forearm's frame
        npalm, _, _ = hp.palm_frame(cs, s)
        hd = hp.norm(hp.sub(cs[f'middle_01_{s}'][1], cs[f'hand_{s}'][1]))
        wr = hp.qrot(hp.qinv(fa_r[0]), hp.norm(hp.cross(hd, npalm)))
        # shrug: clavicle tip up, about cross(clavicle dir, up), fixed in the chest's frame
        cd = hp.norm(hp.sub(ua_r[1], cs[f'clavicle_{s}'][1]))
        sh_axis = hp.norm(hp.cross(cd, up))
        if hp.dot(hp.qrot(hp.qaxis(sh_axis, 10), cd), up) < hp.dot(cd, up):
            sh_axis = tuple(-x for x in sh_axis)
        sh = hp.qrot(hp.qinv(cs['spine_05'][0]), sh_axis)
        axes[s] = (el, wr, sh)
        log(f'arm {s}: reference elbow bend {bend:.1f} deg')
    role = {LEAD: 'lead', ('r' if LEAD == 'l' else 'l'): 'trail'}
    hands = {s: {k: hp.finger_locals(cs, loc, s, k) for k in ('Splayed', 'SplayedTense', 'Relaxed')} for s in ('l', 'r')}
    parent = {}
    for s in ('l', 'r'):
        parent.update({f'clavicle_{s}': 'spine_05', f'upperarm_{s}': f'clavicle_{s}', f'lowerarm_{s}': f'upperarm_{s}', f'hand_{s}': f'lowerarm_{s}'})
    before = {s: [fk(seq, f, [f'clavicle_{s}', f'upperarm_{s}', f'lowerarm_{s}', f'hand_{s}'], parent, {'spine_05': cs['spine_05']}) for f in (0, n // 2)] for s in ('l', 'r')}
    tracks = {}
    for s in ('l', 'r'):
        a = ARMS[role[s]]
        el, wr, sh = axes[s]
        for f in range(n):
            t = f * dt
            tw = t - a['delay']
            w = wob(max(0.0, tw))
            ramp = 1.0                       # Sakurai: the first damage frame is in hitstop and must already read as pain; the system blends in
            for bone, ax, deg in ((f'clavicle_{s}', sh, a['shrug'] * ramp + 0.25 * w),
                                  (f'lowerarm_{s}', el, (a['elbow'] + w) * ramp),
                                  (f'hand_{s}', wr, -(a['wrist'] + 0.8 * w) * ramp)):
                lt = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, bone, f, False)
                q = hp.qmul(hp.qaxis(ax, deg), (lt.rotation.x, lt.rotation.y, lt.rotation.z, lt.rotation.w))
                tracks.setdefault(bone, ([], []))
                tracks[bone][0].append(lt.translation)
                tracks[bone][1].append(q)
            # fingers: tense at the hit -> held pose -> relaxed at the end
            u = f / max(1, n - 1)
            tense = 1.0 - smooth(t / TENSE_SECONDS) if t > 0 else 1.0
            relax = smooth((u - RELAX_FROM) / (1 - RELAX_FROM))
            for b, qrel in hands[s]['Relaxed'].items():
                q = hp.qslerp(hands[s][a['hand_hold']][b], hands[s][a['hand']][b], tense)
                if 'hand_mix' in a:
                    q = hp.qslerp(qrel, q, a['hand_mix'])
                q = hp.qslerp(q, qrel, relax)
                lt = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, b, f, False)
                tracks.setdefault(b, ([], []))
                tracks[b][0].append(lt.translation)
                tracks[b][1].append(q)
    ctrl = seq.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('hit stand arms'), False)
    for b, (pos, rots) in tracks.items():
        ctrl.set_bone_track_keys(b, pos, [unreal.Quat(*q) for q in rots], [unreal.Vector(1, 1, 1)] * len(pos), False)
    ctrl.close_bracket(False)
    EAL.save_asset(DST)
    seq = unreal.load_asset(DST)
    # self-check: elbow bend and hand height before/after, at frame 0 and mid clip
    for s in ('l', 'r'):
        for k, f in enumerate((0, n // 2)):
            p0 = before[s][k]
            p1 = fk(seq, f, [f'clavicle_{s}', f'upperarm_{s}', f'lowerarm_{s}', f'hand_{s}'], parent, {'spine_05': cs['spine_05']})

            def bend(p):
                ua = hp.norm(hp.sub(p[f'lowerarm_{s}'][1], p[f'upperarm_{s}'][1]))
                fa = hp.norm(hp.sub(p[f'hand_{s}'][1], p[f'lowerarm_{s}'][1]))
                return math.degrees(math.acos(max(-1.0, min(1.0, hp.dot(ua, fa)))))
            log(f'{role[s]} arm {s} frame {f}: elbow bend {bend(p0):.1f} -> {bend(p1):.1f} deg, '
                f'hand moved {tuple(round(p1[f"hand_{s}"][1][i] - p0[f"hand_{s}"][1][i], 1) for i in range(3))} cm (chest frame)')
    log(f'{DST}: {n} keys, {dt * 60:.2f} frames@60 per key, {len(tracks)} tracks rewritten')
    return seq


def variant_set(clip):
    EAL = unreal.EditorAssetLibrary
    if EAL.does_asset_exist(VSET):
        EAL.delete_asset(VSET)
    EAL.duplicate_asset(VSET_SRC, VSET)
    rset = unreal.load_asset(VSET)
    rules = list(rset.get_editor_property('rules'))
    hit = 0
    for r in rules:
        if str(r.get_editor_property('name')) == 'HitStand':
            r.set_editor_property('animation', clip)
            hit += 1
    rset.set_editor_property('rules', rules)
    EAL.save_loaded_asset(rset, False)
    log(f'{VSET}: copy of {VSET_SRC}, HitStand rules set {hit}')


try:
    src = unreal.load_asset(SRC)
    skel = src.get_editor_property('skeleton')
    names, cs, loc, lpos = ref_data(skel)
    log(f'finger-ish bones: {[b for b in names if any(k in b for k in ("thumb", "index", "pinky"))][:12]}')
    if os.environ.get('HITARMS_SKIP_LIBRARY') != '1':
        build_library(skel, names, cs, loc, lpos)
    clip = build_arms(names, cs, loc)
    variant_set(clip)
except Exception as ex:
    import traceback
    log('ERROR ' + repr(ex))
    log(traceback.format_exc())
os.makedirs(os.path.dirname(LOG), exist_ok=True)
open(LOG, 'w').write('\n'.join(L) + '\nDONE_MARK\n')
