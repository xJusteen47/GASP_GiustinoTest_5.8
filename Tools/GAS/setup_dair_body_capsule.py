# CAPSULE-MOVES (2026-11-15): the Overlord down-air v9's movement capsule small and centred on the torso instead of the standing capsule (owner:
# "dair v9: the attacker's body enters the victim on the landing hit"; same per-move Body Capsule mechanism as the dropkick,
# setup_dropkick_body_capsule.py). Data only: Body Capsule tracks in /Game/GSTN/Characters/Overlord/Moves/DAir/TL_Overlord_DAirV9. No pose, no key, no
# Blueprint graph, no input, no project setting. Run AFTER setup_overlord_dair_kit.py (the kit script rewrites the timeline).
#
# The clip is measured here (nothing hand typed): torso = midpoint of pelvis and spine_03 per animation frame (timeline frame N shows animation
# frame N, the kit's montage play rate). Per frame the capsule spans the lowest foot (minus the sole) to the head (plus the skull), centred on the
# torso horizontally. The frames are cut into runs where those four numbers stay within DAIR_CAPSULE_TOL cm; each run is one track (the run's
# envelope), Body anchor (the actor moves onto the body, the mesh is offset back: nothing moves on screen), MaxShiftPerFrame DAIR_CAPSULE_SHIFT cm.
# After the timeline the pawn's own capsule comes back (grows from the ground).
# Mesh axes as the dropkick: capsule X forward = mesh +Y, capsule Y right = mesh -X.
#
# Env: DAIR_CAPSULE_HH (smallest half height, cm, default 30), DAIR_CAPSULE_SOLE (8), DAIR_CAPSULE_SKULL (15), DAIR_CAPSULE_TOL (cm, default 8),
# DAIR_CAPSULE_SHIFT (cm/frame, default 20), DAIR_CAPSULE_LABEL (V9).
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_dair_body_capsule as s; s.run()      (s.measure(): log only)
# Output:    Saved/AnimR2/setup_dair_body_capsule.txt (per-frame torso / head / feet table + the tracks), at most 5 printed lines.
import os

import unreal

DIR = '/Game/GSTN/Characters/Overlord/Moves/DAir'
ANIM_FPS = 60.0
CHAIN = ('pelvis', 'spine_01', 'spine_02', 'spine_03', 'neck_01', 'head')
LEGS = {'foot_l': ('thigh_l', 'calf_l', 'foot_l'), 'foot_r': ('thigh_r', 'calf_r', 'foot_r')}
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'AnimR2', 'setup_dair_body_capsule.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[dair_capsule] {msg}')


def _chain_points(anim, frame, chain, base=None):
    comp, points = base, {}
    for bone in chain:
        local = unreal.AnimationLibrary.get_bone_pose_for_frame(anim, bone, frame, False)
        comp = local if comp is None else unreal.MathLibrary.compose_transforms(local, comp)
        points[bone] = comp
    return points


def _capsule_local(v):
    """Mesh space (Mannequin faces +Y, mesh turned -90 yaw) -> capsule frame (X forward, Y right, Z up)."""
    return unreal.Vector(v.y, -v.x, v.z)


def _sample(anim, frame):
    body = _chain_points(anim, frame, CHAIN)
    pelvis = body['pelvis']
    p, s = pelvis.translation, body['spine_03'].translation
    out = {'torso': _capsule_local(unreal.Vector((p.x + s.x) * 0.5, (p.y + s.y) * 0.5, (p.z + s.z) * 0.5)),
           'head': _capsule_local(body['head'].translation)}
    for name, chain in LEGS.items():
        out[name] = _capsule_local(_chain_points(anim, frame, chain, pelvis)[chain[-1]].translation)
    return out


def _load(label):
    lib = unreal.EditorAssetLibrary
    anim_path, tl_path = f'{DIR}/AS_OverlordDAir{label}', f'{DIR}/TL_Overlord_DAir{label}'
    for path in (anim_path, tl_path):
        if not lib.does_asset_exist(path):
            raise RuntimeError(f'{path} missing: run Tools/GAS/setup_overlord_dair_kit.py first')
    return unreal.load_asset(anim_path), unreal.load_asset(tl_path)


def _table(anim, total):
    samples = []
    log('frame | torso x,y,z | head z | foot_l z | foot_r z   (capsule frame, cm, mesh root = 0)')
    for f in range(total):
        frame = min(f, int(round(anim.get_play_length() * ANIM_FPS)))
        s = _sample(anim, frame)
        samples.append(s)
        t = s['torso']
        log(f'{f:3d} | {t.x:6.1f},{t.y:6.1f},{t.z:6.1f} | {s["head"].z:6.1f} | {s["foot_l"].z:6.1f} | {s["foot_r"].z:6.1f}')
    return samples


def _finish(ok):
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in _lines[-4:]:
        print(line[:300])
    return ok


def measure(label=None):
    del _lines[:]
    try:
        label = label or os.environ.get('DAIR_CAPSULE_LABEL', 'V9')
        anim, asset = _load(label)
        _table(anim, asset.get_editor_property('data').get_editor_property('total_frames'))
        return _finish(True)
    except Exception as e:
        log(f'ERROR: {e!r}')
        return _finish(False)


def run(label=None):
    del _lines[:]
    ok = False
    try:
        label = label or os.environ.get('DAIR_CAPSULE_LABEL', 'V9')
        hh = float(os.environ.get('DAIR_CAPSULE_HH', '30'))   # the smallest half height (= the pawn's radius, a sphere)
        tol = float(os.environ.get('DAIR_CAPSULE_TOL', '8'))
        shift = float(os.environ.get('DAIR_CAPSULE_SHIFT', '20'))
        anim, asset = _load(label)
        data = asset.get_editor_property('data')
        total = data.get_editor_property('total_frames')
        samples = _table(anim, total)
        # The dair is upright with the feet down (measured, base capture 2026-11-15): a capsule of a fixed small height on the torso would put
        # the feet through the floor at the landing. So the capsule is fitted to the body: from the lowest foot (minus the sole) to the head
        # (plus the skull), centred on the torso horizontally; small in the tucked startup, full length when the legs strike down.
        sole, skull, min_hh = float(os.environ.get('DAIR_CAPSULE_SOLE', '8')), float(os.environ.get('DAIR_CAPSULE_SKULL', '15')), hh
        # Once the feet have reached their lowest frame the bottom stays there: the recovery tucks the legs up again, and a rising bottom on the
        # ground would drop the whole body by that much (the capsule falls back to the floor)
        # Never below the mesh root: the standing capsule that comes back after the timeline has its bottom there, so a lower bottom on the
        # ground drops the body by the difference on the last frame (first variant capture: -17 cm pop at v122). The clip's feet that reach
        # under the root in the strike stay as the base shows them.
        lows = [max(min(s['foot_l'].z, s['foot_r'].z) - sole, 0.0) for s in samples]
        deepest = lows.index(min(lows))
        spans = []
        for i, s in enumerate(samples):
            bottom = lows[i] if i < deepest else lows[deepest]
            top = max(s['head'].z + skull, bottom + 2 * min_hh)
            spans.append((s['torso'].x, s['torso'].y, bottom, top))
        runs, start = [], 0
        for i in range(1, len(spans) + 1):
            if i < len(spans):
                seg = spans[start:i + 1]
                if (max(v[2] for v in seg) - min(v[2] for v in seg) <= tol and max(v[3] for v in seg) - min(v[3] for v in seg) <= tol
                        and max(v[0] for v in seg) - min(v[0] for v in seg) <= tol and max(v[1] for v in seg) - min(v[1] for v in seg) <= tol):
                    continue
            seg = spans[start:i]
            runs.append((start, i, sum(v[0] for v in seg) / len(seg), sum(v[1] for v in seg) / len(seg), min(v[2] for v in seg), max(v[3] for v in seg)))
            start = i
        utils = unreal.GSTN_TimelineUtils
        data = utils.remove_body_capsule_tracks(data)
        # Lateral offset off by default (DAIR_CAPSULE_LATERAL=1 keeps it): with the torso's -10 cm sideways the capsule met the victim off its
        # centre line on the landing hit and the attacker slid ~50 cm sideways along it (second capture, frames.jsonl loc). Forward stays.
        lateral = os.environ.get('DAIR_CAPSULE_LATERAL', '0') == '1'
        runs = [(a, b, x, y if lateral else 0.0, lo, hi) for a, b, x, y, lo, hi in runs]
        for start, end, x, y, bottom, top in runs:
            run_hh = (top - bottom) * 0.5
            data = utils.add_body_capsule_track(data, start, end, run_hh, 0.0, unreal.Vector(x, y, (top + bottom) * 0.5),
                                                unreal.GSTN_BodyCapsuleAnchor.BODY, shift)
            log(f'track [{start}, {end}) hh {run_hh:.1f} Body centre {x:.1f},{y:.1f},{(top + bottom) * 0.5:.1f} (bottom {bottom:.1f}, top {top:.1f})')
        asset.set_editor_property('data', data)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        log(f'TL_Overlord_DAir{label}: {len(runs)} Body Capsule tracks over [0, {total}), min hh {min_hh:.0f}, tol {tol:.0f}, shift {shift:.0f}; saved')
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    return _finish(ok)


if __name__ == '__main__':
    run()
