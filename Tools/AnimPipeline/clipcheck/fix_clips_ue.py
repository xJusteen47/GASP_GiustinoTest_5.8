"""Apply the generic clip fixes to the editor assets of clips.json rows that list "fix" (headless UE, editor CLOSED on that checkout).

    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -nullrhi      (no args: paths with spaces)

Per row with "asset" and "fix" (a list):
  "filters"  every bone track through clip_filters.clean_track (deflick + ease_snaps = exactly the pops clip_check fails)
  "ik_feet"  ik_foot_l/r re-baked from the feet on every frame (GASP leg IK puts the feet on these bones; a stale or popping
             ik bone shows in game even when the FK leg is clean). Same code as the animpipe export (ue_rig._bake_ik_feet).
Only the changed bone tracks are rewritten (translation and scale keys kept); curves, notifies, sync markers and settings are
untouched. Saves the asset. Report: Saved/ClipCheck/fix_report.json; prints max 5 lines. Re-export + clip_check to verify.
  "splice"   first/last ~0.1 s eased to the neighbour clip's pose (row "splice": {"start": {"asset", "frame"}, "end": {...}, "seconds"})
  "floor_clamp" feet lifted out of the floor by leg IK, vertical only (row "floor_clamp": {"stand_ball_cm", "stand_foot_cm"})
Default = DRY RUN (report only, nothing saved). CLIPFIX_WRITE=1 writes and saves. CLIPFIX_ONLY="RowA,RowB" limits the rows,
CLIPFIX_STEPS="splice,ik_feet" limits the fixes. Agents save through Tools/CI/Save-Assets.ps1 (sets CLIPFIX_WRITE=1).
"""
import importlib.util, json, math, os, sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import clip_filters  # noqa: E402

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary


def _ue_rig():
    p = os.path.join(HERE, '..', 'animpipe', 'ue_rig.py')
    spec = importlib.util.spec_from_file_location('ue_rig_clipfix', p)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m


def resolve(asset):
    if asset.startswith('/'):
        return unreal.load_asset(asset)
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence')], package_paths=['/Game'],
                          recursive_paths=True)
    hits = [a for a in ar.get_assets(flt) if str(a.asset_name) == asset]
    hits.sort(key=lambda a: (not str(a.package_path).startswith('/Game/GSTN'), str(a.package_path)))
    return hits[0].get_asset() if hits else None


def q2t(q):
    return (q.x, q.y, q.z, q.w)


def filters(anim, dry):
    n = int(anim.get_editor_property('number_of_sampled_keys'))
    fps = (n - 1) / max(anim.get_play_length(), 1e-6)
    ctrl = anim.get_editor_property('controller')
    rep = {}
    if not dry:
        ctrl.open_bracket(unreal.Text('clipfix filters'), False)
    for b in [str(x) for x in AL.get_animation_track_names(anim)]:
        tfs = [AL.get_bone_pose_for_frame(anim, b, f, False) for f in range(n)]
        new, r = clip_filters.clean_track([q2t(t.rotation) for t in tfs], fps)
        if r is None:
            continue
        rep[b] = r
        if not dry:
            ctrl.set_bone_track_keys(b, [t.translation for t in tfs], [unreal.Quat(*q) for q in new],
                                     [t.scale3d for t in tfs], False)
    if not dry:
        ctrl.close_bracket(False)
    return {'fps': round(fps, 2), 'frames': n, 'bones': rep}


APE = unreal.AnimPoseExtensions
SKIP_SPLICE = ('root', 'ik_foot_root', 'ik_foot_l', 'ik_foot_r', 'ik_hand_root', 'ik_hand_gun', 'ik_hand_l', 'ik_hand_r')
MATCH_BONES = ('pelvis', 'spine_03', 'clavicle_l', 'upperarm_l', 'lowerarm_l', 'clavicle_r', 'upperarm_r', 'lowerarm_r',
               'thigh_l', 'calf_l', 'foot_l', 'thigh_r', 'calf_r', 'foot_r', 'neck_01', 'head')


def _local(anim, bone, f):
    return AL.get_bone_pose_for_frame(anim, bone, f, False)


def _nkeys(anim):
    return int(anim.get_editor_property('number_of_sampled_keys'))


def _ref_frame(ref, spec, anim, at):
    n = _nkeys(ref)
    fr = spec.get('frame', 'nearest')
    if fr == 'first':
        return 0
    if fr == 'last':
        return n - 1
    if isinstance(fr, int):
        return max(0, min(n - 1, fr))
    tgt = {b: q2t(_local(anim, b, at).rotation) for b in MATCH_BONES}
    best, bi = None, 0
    for f in range(n):   # the ref frame whose main-bone local rotations are closest to the clip's boundary pose
        d = sum(clip_filters.angle(clip_filters._norm(q2t(_local(ref, b, f).rotation)), clip_filters._norm(tgt[b])) for b in MATCH_BONES)
        if best is None or d < best:
            best, bi = d, f
    return bi


def splice(anim, cfg, dry):
    """Hand-off splice (owner rule: solve in code, not per clip; anim-setup-3 decision 1A). The first / last `seconds` of every bone
    track (root and ik bones excepted: root motion stays, ik feet are re-baked after) are eased toward the neighbour clip's pose:
    weight 1 on the boundary frame, smoothstep to 0 at the window's inner edge, so the clip starts exactly in the pose it takes over
    from and ends in the pose it hands to. Rotations slerp; the pelvis translation lerps too (height and sway). The middle is untouched.
    cfg: {"start": {"asset": ..., "frame": "nearest"|"first"|"last"|int}, "end": {...}, "seconds": 0.1}"""
    n = _nkeys(anim)
    fps = (n - 1) / max(anim.get_play_length(), 1e-6)
    k = max(2, int(round(cfg.get('seconds', 0.1) * fps)))
    sides = []
    for side, at in (('start', 0), ('end', n - 1)):
        spec = cfg.get(side)
        if not spec:
            continue
        ref = resolve(spec['asset'])
        if ref is None:
            return {'error': f'{side} ref missing: {spec["asset"]}'}
        sides.append((side, at, ref, _ref_frame(ref, spec, anim, at)))
    rep = {'frames_each_side': k, 'refs': {s: [r.get_name(), f] for s, _, r, f in sides}, 'max_gap_deg': {}}
    ctrl = anim.get_editor_property('controller')
    if not dry:
        ctrl.open_bracket(unreal.Text('clipfix hand-off splice'), False)
    ref_tracks = {id(r): {str(x) for x in AL.get_animation_track_names(r)} for _, _, r, _ in sides}
    for b in [str(x) for x in AL.get_animation_track_names(anim)]:
        if b in SKIP_SPLICE:
            continue
        tfs = [_local(anim, b, f) for f in range(n)]
        qs = clip_filters.continuous([q2t(t.rotation) for t in tfs])
        ts = [t.translation for t in tfs]
        changed = False
        for side, at, ref, rf in sides:
            if b not in ref_tracks[id(ref)]:
                continue
            rt = _local(ref, b, rf)
            rq = q2t(rt.rotation)
            gap = clip_filters.angle(clip_filters._norm(rq), clip_filters._norm(qs[at]))
            rep['max_gap_deg'][side] = max(rep['max_gap_deg'].get(side, 0.0), round(gap, 1))
            if gap < 0.5 and (b != 'pelvis' or rt.translation.distance(ts[at]) < 0.5):
                continue
            for j in range(k + 1):
                i = j if side == 'start' else n - 1 - j
                t = j / float(k); w = 1.0 - t * t * (3 - 2 * t)
                qs[i] = clip_filters.slerp(qs[i], rq, w)
                if b == 'pelvis':
                    ts[i] = ts[i] + (rt.translation - ts[i]) * w
            changed = True
        if changed and not dry:
            ctrl.set_bone_track_keys(b, ts, [unreal.Quat(*q) for q in qs], [t.scale3d for t in tfs], False)
    if not dry:
        ctrl.close_bracket(False)
    return rep


def floor_clamp(anim, cfg, dry):
    """Feet never go under the floor (anim-setup-3 decision 2A: skid ball r -5.8 cm). Per frame, the lowest of ball/heel below its
    flat-foot height lifts the FOOT straight up by the missing height (two-bone IK on thigh-calf-foot, knee kept in its plane);
    only vertical, so an intended slide (GSTN_FootSlide) keeps its travel. cfg: {"stand_ball_cm": 3.2, "stand_foot_cm": 9.4}"""
    n = _nkeys(anim)
    step = anim.get_play_length() / max(n - 1, 1)
    sb, sf = cfg.get('stand_ball_cm', 3.2), cfg.get('stand_foot_cm', 9.4)
    opts = unreal.AnimPoseEvaluationOptions()
    W = unreal.AnimPoseSpaces.WORLD
    edits = {}   # bone -> {frame: local quat}
    rep = {'frames': {}}
    for f in range(n):
        pose = APE.get_anim_pose_at_time(anim, f * step, opts)
        for s in 'lr':
            ball = APE.get_bone_pose(pose, f'ball_{s}', W).translation
            foot = APE.get_bone_pose(pose, f'foot_{s}', W).translation
            lift = max(sb - ball.z, sf - foot.z, 0.0)
            if lift < 0.2:
                continue
            thigh = APE.get_bone_pose(pose, f'thigh_{s}', W)
            calf = APE.get_bone_pose(pose, f'calf_{s}', W)
            tgt = unreal.Vector(foot.x, foot.y, foot.z + lift)
            nt, nc = _two_bone(thigh, calf, foot, tgt)
            # back to local: thigh local = parent(pelvis)^-1 * new thigh; calf local = new thigh^-1 * new calf
            parent = APE.get_bone_pose(pose, 'pelvis', W)
            edits.setdefault(f'thigh_{s}', {})[f] = unreal.MathLibrary.make_relative_transform(nt, parent).rotation
            edits.setdefault(f'calf_{s}', {})[f] = unreal.MathLibrary.make_relative_transform(nc, nt).rotation
            footw = APE.get_bone_pose(pose, f'foot_{s}', W)   # the foot keeps its world rotation (flat stays flat)
            edits.setdefault(f'foot_{s}', {})[f] = unreal.MathLibrary.make_relative_transform(footw, nc).rotation
            rep['frames'].setdefault(s, []).append([f + 1, round(lift, 2)])
    if edits and not dry:
        ctrl = anim.get_editor_property('controller')
        ctrl.open_bracket(unreal.Text('clipfix floor clamp'), False)
        for b, fr in edits.items():
            tfs = [_local(anim, b, f) for f in range(n)]
            ctrl.set_bone_track_keys(b, [t.translation for t in tfs], [fr.get(f, tfs[f].rotation) for f in range(n)],
                                     [t.scale3d for t in tfs], False)
        ctrl.close_bracket(False)
    return rep


def _two_bone(thigh, calf, foot, tgt):
    """Analytic two-bone IK in world space. Returns new thigh and calf world transforms (foot rotation follows the calf, which is
    what keeps the foot's orientation in world when only the knee swings a little: the lift is a few cm)."""
    a, b_, c = thigh.translation, calf.translation, foot
    l1, l2 = (b_ - a).length(), (c - b_).length()
    d = tgt - a
    dl = max(min(d.length(), l1 + l2 - 1e-3), abs(l1 - l2) + 1e-3)
    pole = (b_ - a) - d.normal() * (b_ - a).dot(d.normal())    # knee direction off the hip->target line
    pole = pole.normal() if pole.length() > 1e-4 else unreal.Vector(1, 0, 0)
    ca = (l1 * l1 + dl * dl - l2 * l2) / (2 * l1 * dl)
    ang = math.acos(max(-1.0, min(1.0, ca)))
    knee = a + d.normal() * (math.cos(ang) * l1) + pole * (math.sin(ang) * l1)
    nt = _aim(thigh, b_ - a, knee - a)
    nc = _aim(calf, c - b_, tgt - knee)
    nc.translation = knee
    return nt, nc


def _aim(tf, old_dir, new_dir):
    u, v = old_dir.normal(), new_dir.normal()
    cr = u.cross(v)
    q = unreal.Quat(cr.x, cr.y, cr.z, 1.0 + u.dot(v)); q.normalize()   # shortest arc u -> v
    out = unreal.Transform(tf.translation, tf.rotation.rotator(), tf.scale3d)
    out.rotation = q * tf.rotation
    return out


def slide_marker(anim, cfg, dry):
    """Writes the intended-slide marker curve (docs/notes/foot-lock-and-slides.md) from the row's "slide_marker":
    {"side": "l"|"r"|"both", "frames": [first, last] (1-based), "ramp": 2}. 1 inside, linear ramp of `ramp` frames each side."""
    n = _nkeys(anim)
    step = anim.get_play_length() / max(n - 1, 1)
    side = cfg.get('side', 'both')
    name = 'GSTN_FootSlide' if side == 'both' else f'GSTN_FootSlide_{side}'
    a, b = cfg['frames'][0] - 1, cfg['frames'][1] - 1
    ramp = int(cfg.get('ramp', 2))
    vals = []
    for f in range(n):
        if a <= f <= b:
            v = 1.0
        elif a - ramp <= f < a:
            v = (f - (a - ramp)) / float(ramp + 1)
        elif b < f <= b + ramp:
            v = (b + ramp + 1 - f) / float(ramp + 1)
        else:
            v = 0.0
        vals.append(v)
    if not dry:
        T = unreal.RawCurveTrackTypes.RCT_FLOAT
        if AL.does_curve_exist(anim, name, T):
            AL.remove_curve(anim, name, False)
        AL.add_curve(anim, name, T, False)
        AL.add_float_curve_keys(anim, name, [f * step for f in range(n)], vals)
    return {'curve': name, 'frames': [a + 1, b + 1]}


def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game'], True)
    C = json.load(open(os.path.join(HERE, 'clips.json')))['clips']
    only = [x for x in os.environ.get('CLIPFIX_ONLY', '').split(',') if x]
    dry = os.environ.get('CLIPFIX_WRITE') != '1'
    steps = [x for x in os.environ.get('CLIPFIX_STEPS', '').split(',') if x]   # e.g. "splice,floor_clamp,ik_feet": only these
    rig = None
    report, lines = {}, []
    for clip, cfg in C.items():
        if clip.startswith('_') or 'asset' not in cfg or not cfg.get('fix') or (only and clip not in only):
            continue
        anim = resolve(cfg['asset'])
        if anim is None:
            lines.append(f'MISSING {clip}'); continue
        r = {'asset': anim.get_path_name(), 'dry': dry}
        fx = [x for x in cfg['fix'] if not steps or x in steps]
        if 'filters' in fx:
            r['filters'] = filters(anim, dry)
        if 'splice' in fx:
            r['splice'] = splice(anim, cfg.get('splice', {}), dry)
        if 'slide_marker' in fx:
            r['slide_marker'] = slide_marker(anim, cfg['slide_marker'], dry)
        if 'floor_clamp' in fx:
            r['floor_clamp'] = floor_clamp(anim, cfg.get('floor_clamp', {}), dry)
        if 'ik_feet' in fx and not dry:
            rig = rig or _ue_rig()
            r['ik_feet'] = rig._bake_ik_feet(anim)
        if not dry:
            r['saved'] = bool(EAL.save_asset(anim.get_path_name().split('.')[0], only_if_is_dirty=False))
        report[clip] = r
        nb = len(r.get('filters', {}).get('bones', {}))
        sp = r.get('splice', {}).get('max_gap_deg', {})
        fc = sum(len(v) for v in r.get('floor_clamp', {}).get('frames', {}).values())
        lines.append(f'{clip}: filters {nb}, splice gaps {sp}, floor frames {fc}, saved {r.get("saved")}')
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'ClipCheck', 'fix_report.json')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    json.dump(report, open(out, 'w'), indent=1, default=str)
    print(f'clipfix: {len(report)} clips, report {out}')
    for s in lines[:4]:
        print(s)


main()
