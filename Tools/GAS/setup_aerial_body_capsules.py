# AERIAL-CAPSULES (owner 2026-11-26, choice B, after the dropkick bounce fix): every aerial gets a movement capsule fitted to its body, the
# bottom on the lowest foot, as the dair v9 has had since 2026-11-15 (setup_dair_body_capsule.py). The pawn lands when its feet do, not when the
# standing capsule's bottom does (the "aerial must come out as close to the ground as possible, but it lands instead" feeling). Data only: Body
# Capsule tracks in combat timelines. No pose, no key, no Blueprint graph, no input, no project setting.
#
# Which timelines: every GSTN_CombatTimeline under AERIAL_CAPSULE_ROOT that ends at a landing (a Hold Until Landed or a Landing track) and has a
# montage, and has NO Body Capsule track yet: the dropkick's (setup_dropkick_body_capsule.py, its own pin rule) and the dair v9's stay as they are.
# AERIAL_CAPSULE_ONLY=TL_a,TL_b picks by name instead (and refits those even with tracks). Hand-edited timelines are kept (setup_guard.py).
# The clip: the montage's first slot segment's animation (else the AS_ named like the AM_ in the same folder), timeline frame N at montage
# time N / 60 x the timeline's play rate, as the kit scripts lay them out. The fit (bottom, top, runs) is body_capsule_fit.py.
# Body anchor (the actor moves onto the body, the mesh is offset back: nothing moves on screen), MaxShiftPerFrame AERIAL_CAPSULE_SHIFT cm.
# Mesh axes as the dropkick: capsule X forward = mesh +Y, capsule Y right = mesh -X.
#
# Env: AERIAL_CAPSULE_ROOT (/Game), AERIAL_CAPSULE_ONLY, AERIAL_CAPSULE_DRY=1 (log the fit, save nothing), AERIAL_CAPSULE_HH (smallest half
# height, 30), _SOLE (8), _SKULL (15), _TOL (8), _SHIFT (20 cm/frame), _LATERAL=1 (keep the torso's sideways offset), SETUP_FORCE (setup_guard).
# Editor:  import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_aerial_body_capsules as s; s.run()   (dry first: AERIAL_CAPSULE_DRY=1)
# Output:  Saved/AnimR2/setup_aerial_body_capsules.txt (per timeline: clip, frames, tracks), at most 5 printed lines.
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import body_capsule_fit  # noqa: E402
import setup_guard  # noqa: E402

ROOT = os.environ.get('AERIAL_CAPSULE_ROOT', '/Game')
ONLY = {n.strip() for n in os.environ.get('AERIAL_CAPSULE_ONLY', '').split(',') if n.strip()}
DRY = os.environ.get('AERIAL_CAPSULE_DRY') == '1'
ANIM_FPS = 60.0
CHAIN = ('pelvis', 'spine_01', 'spine_02', 'spine_03', 'neck_01', 'head')
LEGS = (('thigh_l', 'calf_l', 'foot_l'), ('thigh_r', 'calf_r', 'foot_r'))
LANDS = ('GSTN_TimelineTrack_HoldUntilLanded', 'GSTN_TimelineTrack_Landing')
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'AnimR2', 'setup_aerial_body_capsules.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[aerial_capsule] {msg}')


def _env(name, default):
    return float(os.environ.get(f'AERIAL_CAPSULE_{name}', str(default)))


def _timelines():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    cls = unreal.GSTN_CombatTimeline.static_class()
    path = unreal.TopLevelAssetPath(cls.get_outer().get_path_name(), cls.get_name())
    return sorted((d for d in reg.get_assets_by_class(path, True) if str(d.package_name).startswith(ROOT)), key=lambda d: str(d.package_name))


def _montage(data, text):
    m = None
    try:
        m = data.get_editor_property('montage')
    except Exception:
        pass
    if m is None:
        # A soft pointer that Python hands back unresolved: its path from the exported text
        found = re.search(r'(?<![A-Za-z])Montage=[^,)]*?(/Game/[^"\' ,)]+)', text)
        m = unreal.load_asset(found.group(1)) if found else None
    return m


def _clip(montage):
    """(animation, its start time in the clip) of the montage's first slot segment; else the AS_ named like the AM_ beside it"""
    try:
        track = montage.get_editor_property('slot_anim_tracks')[0].get_editor_property('anim_track')
        seg = track.get_editor_property('anim_segments')[0]
        anim = seg.get_editor_property('anim_reference')
        if anim:
            return anim, float(seg.get_editor_property('anim_start_time')), 'slot segment'
    except Exception:
        pass
    path = montage.get_path_name().split('.')[0]
    folder, name = path.rsplit('/', 1)
    guess = f'{folder}/AS_{name[3:]}' if name.startswith('AM_') else None
    if guess and unreal.EditorAssetLibrary.does_asset_exist(guess):
        return unreal.load_asset(guess), 0.0, 'AS_ beside the AM_'
    raise RuntimeError(f'{name}: no animation found (slot segment unreadable, no AS_ beside it)')


def _chain(anim, frame, chain, base=None):
    comp, points = base, {}
    for bone in chain:
        local = unreal.AnimationLibrary.get_bone_pose_for_frame(anim, bone, frame, False)
        comp = local if comp is None else unreal.MathLibrary.compose_transforms(local, comp)
        points[bone] = comp
    return points


def _cap(v):
    """Mesh space (Mannequin faces +Y, mesh turned -90 yaw) -> capsule frame (X forward, Y right, Z up)"""
    return (v.y, -v.x, v.z)


def _samples(anim, start_time, rate, total):
    last = int(round(anim.get_play_length() * ANIM_FPS))
    out = []
    for f in range(total):
        frame = min(int(round((start_time + f / ANIM_FPS * rate) * ANIM_FPS)), last)
        body = _chain(anim, frame, CHAIN)
        p, s = body['pelvis'].translation, body['spine_03'].translation
        torso = _cap(unreal.Vector((p.x + s.x) * 0.5, (p.y + s.y) * 0.5, (p.z + s.z) * 0.5))
        feet = tuple(_cap(_chain(anim, frame, leg, body['pelvis'])[leg[-1]].translation)[2] for leg in LEGS)
        out.append({'torso': torso, 'head_z': _cap(body['head'].translation)[2], 'foot_z': feet})
    return out


def run(force=False):
    del _lines[:]
    ok, done, kept, skipped = False, 0, 0, 0
    try:
        hh, sole, skull, tol, shift = _env('HH', 30), _env('SOLE', 8), _env('SKULL', 15), _env('TOL', 8), _env('SHIFT', 20)
        lateral = os.environ.get('AERIAL_CAPSULE_LATERAL', '0') == '1'
        guard = setup_guard.Guard.for_unreal('setup_aerial_body_capsules', force=force, log=log)
        picked = []
        for d in _timelines():
            name, path = str(d.asset_name), str(d.package_name)
            if ONLY and name not in ONLY:
                continue
            asset = d.get_asset()
            data = asset.get_editor_property('data') if asset else None
            text = data.export_text() if data else ''
            if not ONLY and (not any(t in text for t in LANDS) or 'GSTN_TimelineTrack_BodyCapsule' in text):
                continue
            picked.append((name, path, asset, data, text))
        log(f'{len(picked)} aerial timeline(s) under {ROOT}' + (f' (only {sorted(ONLY)})' if ONLY else ' without a Body Capsule yet')
            + (' [dry run]' if DRY else ''))
        guard.precheck([p for _, p, *_ in picked])
        utils = unreal.GSTN_TimelineUtils
        for name, path, asset, data, text in picked:
            montage = _montage(data, text)
            if not montage:
                log(f'{name}: no montage, skipped')
                skipped += 1
                continue
            if str(data.get_editor_property('montage_section')) not in ('None', ''):
                log(f'{name}: montage section {data.get_editor_property("montage_section")} set; the fit reads the montage from its start')
            anim, start_time, how = _clip(montage)
            total = data.get_editor_property('total_frames')
            rate = float(data.get_editor_property('montage_play_rate'))
            tracks = body_capsule_fit.fit(_samples(anim, start_time, rate, total), sole, skull, hh, tol, lateral)
            log(f'{name}: {anim.get_name()} ({how}, rate {rate:g}), {total} frames, {len(tracks)} track(s)')
            for a, b, run_hh, x, y, z in tracks:
                log(f'  [{a}, {b}) hh {run_hh:.1f} Body centre {x:.1f},{y:.1f},{z:.1f} (bottom {z - run_hh:.1f}, top {z + run_hh:.1f})')
            if DRY:
                continue
            if not guard.may_write(path):
                kept += 1
                continue
            data = utils.remove_body_capsule_tracks(data)
            for a, b, run_hh, x, y, z in tracks:
                data = utils.add_body_capsule_track(data, a, b, run_hh, 0.0, unreal.Vector(x, y, z), unreal.GSTN_BodyCapsuleAnchor.BODY, shift)
            asset.set_editor_property('data', data)
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
            guard.record([path])
            done += 1
        log(f'done: {done} saved, {kept} kept (edited by hand), {skipped} skipped; {guard.summary()}')
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'ERROR' in l][:4] + [_lines[-1][:300] if _lines else '']:
        print(line)
    return ok


if __name__ == '__main__':
    run()
