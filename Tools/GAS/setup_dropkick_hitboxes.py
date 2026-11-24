# Dropkick hitboxes from the clips (owner 2026-11-23, provisional: "basta che funzionino nelle catture", the owner retunes them later).
#
# Rebuilds ONLY the attack hitbox tracks of one dropkick timeline (env DROPKICK_TL, default TL_Overlord_DropkickTBK6) from the montage it
# plays, so the boxes follow the legs of the current round's clips. Every other track (hold until landed, body capsule, landing, tags,
# hurtboxes) is kept: UGSTN_TimelineUtils::RemoveAttackHitboxTracks drops only attack hitbox tracks.
#
# Shapes and timing. References chosen by the owner: placement as Ganondorf's down air in Smash Ultimate (sweetspot on the feet, sourspot on
# the legs, short strong window: ssbwiki lists IDs 0 r7.0 19% and 1 r6.0 17%, active 16-18), hitstop as Big Band's j.HK in Skullgirls (a
# heavy: no per-move number was readable, the usual heavy freeze of ~13 frames, both fighters frozen); "when it falls, shrink the hitbox".
#   STRONG [start, start + STRONG_FRAMES): Sweet = sphere on the soles (midpoint of both feet pushed toward the balls; the legs are together),
#            Sour = capsule from the hips to the ankles covering both legs. Same HitGroup: the Sweet's higher Priority wins when both overlap.
#   LATE   [start + STRONG_FRAMES, land frame): smaller capsule knees -> ankles, weaker hit. With Hold Until Landed the timeline stands on the
#            loop's last frame in the air, so the late key of that frame is the box while it falls.
# Space ROOT, one key per timeline frame sampled from the montage pose (frame N of the timeline shows anim frame N, montage time N / 60):
# no socket bake to go stale. Rerun this script after any clip of the montage changes; it writes Saved/AnimR2/setup_dropkick_hitboxes.json
# with the sha256 of every clip it sampled, and the anim MCP refuses a capture when a clip no longer matches (pre-capture gate).
#
# Run in the editor (bridge or -ExecCmds), after setup_dropkick_kick_variant.py built the round's TL/montage. The timeline was made by
# duplicate_asset, so setup_guard calls it "unknown" and KEEPS it: run with SETUP_FORCE=TL_Overlord_DropkickTB<round> (old file backed up
# to Saved/Backups/SetupGuard/). DROPKICK_HITBOX_DRY=1 = sample and log only. Prints at most 5 lines, details in Saved/AnimR2/.
import csv
import hashlib
import json
import math
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_guard  # noqa: E402

DIR = '/Game/GSTN/Characters/Overlord/Moves/Dropkick'
TL_NAME = os.environ.get('DROPKICK_TL', 'TL_Overlord_DropkickTBK6')
DRY = os.environ.get('DROPKICK_HITBOX_DRY') == '1'
ANIM_FPS = 60.0
CAPSULE_HALF_HEIGHT = 90.0          # cm, the mesh sits this far below the root (as setup_overlord_dropkick_kit.py)
TIMING_CSV = 'Tools/AnimPipeline/cards/overlord/dropkick/timing/OverlordDropkick_timing.csv'
FALLBACK_ACTIVE_START = 15
STRONG_FRAMES = int(os.environ.get('DROPKICK_STRONG_FRAMES', '4'))   # Ganon's dair is strong for 3 frames; the snap a13-a15 + impact a15-a17

# Sizes in cm (Overlord ~190 cm; the leg is ~90 cm). Sweet: big enough to read as "the feet", ~10 cm past the soles at most
SWEET_RADIUS = 24.0
SWEET_TOWARD_BALL = 0.6             # 0 = ankle midpoint, 1 = ball midpoint
SOUR_RADIUS = 17.0                  # both legs together are ~30 cm wide
LATE_RADIUS = 13.0

# (damage, base knockback, angle, victim hitstop, attacker hitstop, priority). Angle 40 = the kit's. Hitstop in simulation frames
HITS = {
    'Sweet': (14.0, 450.0, 40.0, 13, 13, 2),
    'Sour': (10.0, 380.0, 40.0, 11, 11, 1),
    'Late': (7.0, 300.0, 40.0, 8, 8, 0),
}

OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'AnimR2')
OUT_TXT = os.path.join(OUT_DIR, 'setup_dropkick_hitboxes.txt')
OUT_JSON = os.path.join(OUT_DIR, 'setup_dropkick_hitboxes.json')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[dropkick_hitboxes] {msg}')


def _active_start():
    path = os.path.join(unreal.Paths.project_dir(), TIMING_CSV)
    try:
        with open(path, newline='') as f:
            rows = [r for r in csv.DictReader(f) if r['active'].strip().lower() == 'true']
        return int(rows[0]['frame']) - 1
    except Exception as e:  # noqa: BLE001
        log(f'WARNING: timing csv not read ({e!r}): active start {FALLBACK_ACTIVE_START}')
        return FALLBACK_ACTIVE_START


def _segments(montage):
    """[(anim, start_s, anim_start_s, anim_end_s, rate, loops)] of the montage's first slot. StartPos is read-only from Python: parsed from
    the export text, as the setup scripts write it"""
    out = []
    track = montage.get_editor_property('slot_anim_tracks')[0].get_editor_property('anim_track')
    for seg in track.get_editor_property('anim_segments'):
        m = re.search(r'StartPos=([-0-9.e]+)', seg.export_text())
        out.append((seg.get_editor_property('anim_reference'), float(m.group(1)) if m else 0.0,
                    seg.get_editor_property('anim_start_time'), seg.get_editor_property('anim_end_time'),
                    seg.get_editor_property('anim_play_rate') or 1.0, max(1, seg.get_editor_property('looping_count'))))
    return out


def _sample(segs, t):
    """(anim, local time) shown at montage time t"""
    for anim, start, a0, a1, rate, loops in segs:
        length = (a1 - a0) / rate
        if start - 1e-6 <= t < start + length * loops - 1e-6:
            local = (t - start) % length if length > 0 else 0.0
            return anim, a0 + local * rate
    anim, start, a0, a1, rate, loops = segs[-1]
    return anim, a1


def _root(pose, bone):
    """Root-space position of a bone (component space looks along +Y, the root along +X: (x, y) -> (y, -x)), as the kit script"""
    p = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
    return (p.y, -p.x, p.z - CAPSULE_HALF_HEIGHT)


def _mid(a, b, t=0.5):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def _pose_points(segs, frame, opts):
    anim, local = _sample(segs, frame / ANIM_FPS)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, local, opts)
    j = {b: _root(pose, b) for b in ('thigh_l', 'thigh_r', 'calf_l', 'calf_r', 'foot_l', 'foot_r', 'ball_l', 'ball_r')}
    ankle = _mid(j['foot_l'], j['foot_r'])
    ball = _mid(j['ball_l'], j['ball_r'])
    return {'hip': _mid(j['thigh_l'], j['thigh_r']), 'knee': _mid(j['calf_l'], j['calf_r']), 'ankle': ankle,
            'sole': _mid(ankle, ball, SWEET_TOWARD_BALL)}, anim


def _key(frame, centre, rot=None, scale=1.0):
    key = unreal.GSTN_HitboxKey()
    key.set_editor_property('frame', frame)
    key.set_editor_property('offset', unreal.Vector(*centre))
    if rot is not None:
        key.set_editor_property('rotation', rot)
    if scale != 1.0:
        key.set_editor_property('scale', unreal.Vector(scale, scale, scale))
    return key


def _capsule_key(frame, a, b, radius, ref_half):
    """Key of a capsule from a to b: centre, local Z along a->b, scale Z so the half height (hemispheres included) spans the segment"""
    axis = unreal.Vector(b[0] - a[0], b[1] - a[1], b[2] - a[2])
    length = math.sqrt(axis.x ** 2 + axis.y ** 2 + axis.z ** 2)
    rot = unreal.MathLibrary.make_rot_from_z(axis) if length > 1e-3 else unreal.Rotator(0.0, 0.0, 0.0)
    key = _key(frame, _mid(a, b), rot)
    key.set_editor_property('scale', unreal.Vector(1.0, 1.0, max(length / 2.0 + radius, radius) / ref_half))
    return key


def _hit(kind):
    dmg, bkb, angle, victim, attacker, prio = HITS[kind]
    hit = unreal.GSTN_HitData()
    for prop, value in (('damage', dmg), ('base_knockback', bkb), ('angle_deg', angle),
                        ('hitstop_mode', unreal.GSTN_HitstopMode.OVERRIDE), ('hitstop_frames_override', victim),
                        ('attacker_hitstop_frames_override', attacker), ('priority', prio)):
        hit.set_editor_property(prop, value)
    return hit


def _box(name, kind, zone, shape_type, extent, keys):
    shape = unreal.GSTN_HitShape()
    shape.set_editor_property('type', shape_type)
    shape.set_editor_property('extent', extent)
    box = unreal.GSTN_HitboxSpec()
    for prop, value in (('name', name), ('shape', shape), ('space', unreal.GSTN_HitboxSpace.ROOT), ('hit_group', 0), ('data', _hit(kind)),
                        ('keys', keys), ('sweep_between_frames', True), ('zone', zone)):
        box.set_editor_property(prop, value)
    return box


def _asset_sha(asset_path):
    path = setup_guard.asset_file(os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())), asset_path)
    return setup_guard.file_sha256(path) if os.path.exists(path) else ''


def run(force=False):
    del _lines[:]
    ok = False
    try:
        path = f'{DIR}/{TL_NAME}'
        guard = setup_guard.Guard.for_unreal('setup_dropkick_hitboxes', force=force, log=log)
        guard.precheck([path])
        tl = unreal.load_asset(path)
        if not tl:
            raise RuntimeError(f'{path} not found: run setup_dropkick_kick_variant.py for the round first')
        data = tl.get_editor_property('data')
        montage = data.get_editor_property('montage')
        if not montage:
            raise RuntimeError(f'{TL_NAME} has no montage')
        segs = _segments(montage)
        start = _active_start()
        land = int(round(segs[-1][1] * ANIM_FPS))          # first frame of the last segment (the land clip) = the kit's loop_end
        strong_end = min(start + STRONG_FRAMES, land)
        if land <= start + 1:
            raise RuntimeError(f'land frame {land} not after active start {start}')
        opts = unreal.AnimPoseEvaluationOptions()
        sweet, sour, late, used = [], [], [], {}
        ref_half = 50.0   # capsule half height in the shape; keys scale Z to the real span
        for f in range(start, land):
            pts, anim = _pose_points(segs, f, opts)
            used[anim.get_path_name().split('.', 1)[0]] = 1
            if f < strong_end:
                k = f - start
                sweet.append(_key(k, pts['sole']))
                sour.append(_capsule_key(k, pts['hip'], pts['ankle'], SOUR_RADIUS, ref_half))
            else:
                late.append(_capsule_key(f - strong_end, pts['knee'], pts['ankle'], LATE_RADIUS, ref_half))
            if f in (start, strong_end - 1, strong_end, land - 1):
                log(f'f{f}: sole {tuple(round(v, 1) for v in pts["sole"])} hip {tuple(round(v, 1) for v in pts["hip"])} '
                    f'knee {tuple(round(v, 1) for v in pts["knee"])} ankle {tuple(round(v, 1) for v in pts["ankle"])}')
        cap = unreal.GSTN_ShapeType.CAPSULE
        strong_boxes = [
            _box('Sweet piedi', 'Sweet', unreal.GSTN_HitboxZone.SWEET, unreal.GSTN_ShapeType.SPHERE,
                 unreal.Vector(SWEET_RADIUS, SWEET_RADIUS, SWEET_RADIUS), sweet),
            _box('Sour gambe', 'Sour', unreal.GSTN_HitboxZone.SOUR, cap, unreal.Vector(SOUR_RADIUS, SOUR_RADIUS, ref_half), sour)]
        late_boxes = [_box('Late gambe (caduta)', 'Late', unreal.GSTN_HitboxZone.SOUR, cap, unreal.Vector(LATE_RADIUS, LATE_RADIUS, ref_half), late)]
        log(f'{TL_NAME}: strong [{start}, {strong_end}) sweet r{SWEET_RADIUS} + sour capsule r{SOUR_RADIUS}; late [{strong_end}, {land}) '
            f'capsule r{LATE_RADIUS}; hitstop {HITS["Sweet"][3]}/{HITS["Sour"][3]}/{HITS["Late"][3]}; clips {", ".join(n.rsplit("/", 1)[-1] for n in used)}')
        if DRY:
            log('DRY: nothing written')
            ok = True
        elif not guard.may_write(path):
            raise RuntimeError(f'{TL_NAME} kept by setup_guard: rerun with SETUP_FORCE={TL_NAME}')
        else:
            data = unreal.GSTN_TimelineUtils.remove_attack_hitbox_tracks(data)
            data = unreal.GSTN_TimelineUtils.add_hitbox_track(data, start, strong_end, unreal.GSTN_PartSelector(), strong_boxes)
            if late:
                data = unreal.GSTN_TimelineUtils.add_hitbox_track(data, strong_end, land, unreal.GSTN_PartSelector(), late_boxes)
            tl.set_editor_property('data', data)
            guard.save_and_record([tl], unreal.EditorAssetLibrary.save_loaded_asset)
            os.makedirs(OUT_DIR, exist_ok=True)
            with open(OUT_JSON, 'w') as f:
                json.dump({'timeline': path, 'montage': montage.get_path_name().split('.', 1)[0],
                           'clips': {p: _asset_sha(p) for p in sorted(used)},
                           'start': start, 'strong_end': strong_end, 'land': land}, f, indent=1)
            log(f'saved {TL_NAME}; clip hashes in {OUT_JSON}; {guard.summary()}')
            ok = True
    except Exception as e:  # noqa: BLE001
        log(f'ERROR: {e!r}')
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(OUT_TXT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if any(w in l for w in ('WARNING', 'ERROR', 'KEPT', 'FORCE'))][:3] + _lines[-2:]:
        print(line[:300])
    return ok


if __name__ == '__main__':
    run()
