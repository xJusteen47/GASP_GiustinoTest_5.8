# Make a converted clip usable as a GASP chooser row (skid-walkturn-1, 2026-11-20). Docs: docs/knowledge/gasp/gasp-structure-5.9.md
# section 10.3 (every clip: root motion, ik feet, length after the likely start > 0.75 s + blend, Pose Search notify states).
#  1. TAIL: GASP leaves a non-looping clip when <= 0.75 s remain (IsAnimationAlmostComplete). A 0.8 s turn would be cut at once,
#     so PREP_TAIL seconds are appended: body held on the last frame, root continuing at the clip's end velocity (the plan GASP
#     warps against). The tail is only seen under the outgoing blend; it is excluded from the pose search database.
#  2. ik_foot_l/r baked from the feet (GASP leg IK pins the feet to them), same code as the animpipe export.
#  3. enable_root_motion on, no root lock.
#  4. Pose Search notify states: ExcludeFromDatabase from PREP_ENTRY_MAX s to the end (entry only at the start of the turn).
# Env PREP_CLIP (asset path), PREP_TAIL (s, default 0.8), PREP_ENTRY_MAX (s, default 0.2). Idempotent only on a fresh copy:
# PREP_SRC, if given, is duplicated to PREP_CLIP first. Saves only with GSTN_SAVE=1. Log Saved/Mixamo/prep_<name>.log, ESITO line.
import importlib.util
import math
import os
import unreal

CLIP = os.environ.get('PREP_CLIP', '')
SRC = os.environ.get('PREP_SRC', '')
TAIL = float(os.environ.get('PREP_TAIL', '0.8'))
ENTRY_MAX = float(os.environ.get('PREP_ENTRY_MAX', '0.2'))
SAVE = os.environ.get('GSTN_SAVE') == '1'
AL = unreal.AnimationLibrary
eal = unreal.EditorAssetLibrary
NAME = CLIP.rsplit('/', 1)[-1] or 'none'
LOG = os.path.join(unreal.Paths.project_saved_dir(), 'Mixamo', f'prep_{NAME}.log')
os.makedirs(os.path.dirname(LOG), exist_ok=True)
_f = open(LOG, 'w', encoding='utf-8')
errors = []


def log(s):
    _f.write(str(s) + '\n')
    _f.flush()


def ue_rig():
    here = os.path.dirname(os.path.abspath(__file__))
    p = os.path.join(here, '..', '..', 'AnimPipeline', 'animpipe', 'ue_rig.py')
    spec = importlib.util.spec_from_file_location('ue_rig_prep', p)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def add_tail(anim):
    n = AL.get_num_keys(anim)
    fps = (n - 1) / anim.get_play_length()
    extra = int(round(TAIL * fps))
    if extra % 2:
        extra += 1  # even 60 fps interval count (ue-anim-python-gotchas)
    tracks = [str(t) for t in AL.get_animation_track_names(anim)]
    local = {b: [AL.get_bone_pose_for_frame(anim, b, i, False) for i in range(n)] for b in tracks}
    root = local['root']
    k = min(3, n - 1)
    vel = (root[n - 1].translation - root[n - 1 - k].translation) * (1.0 / k)  # cm per frame, component space
    vel.z = 0.0
    log(f'tail: keys {n} -> {n + extra} at {fps:.1f} fps, root end speed {vel.length() * fps:.1f} cm/s')
    ctrl = anim.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('prep tail'), False)
    ctrl.set_number_of_frames(unreal.FrameNumber(n - 1 + extra), False)
    for b in tracks:
        xs = list(local[b])
        last = xs[-1]
        for j in range(1, extra + 1):
            if b == 'root':
                xs.append(unreal.Transform(location=last.translation + vel * float(j), rotation=last.rotation.rotator(),
                                           scale=last.scale3d))
            else:
                xs.append(last)
        ctrl.set_bone_track_keys(b, [x.translation for x in xs], [x.rotation for x in xs], [x.scale3d for x in xs], False)
    ctrl.close_bracket(False)
    log(f'tail: length now {anim.get_play_length():.3f} s')


def add_notifies(anim):
    L = anim.get_play_length()
    track = 'GSTN_PoseSearch'
    if track not in [str(t) for t in AL.get_animation_notify_track_names(anim)]:
        AL.add_animation_notify_track(anim, track)
    else:
        AL.remove_animation_notify_events_by_track(anim, track)
    cls = getattr(unreal, 'AnimNotifyState_PoseSearchExcludeFromDatabase', None)
    if cls is None:
        errors.append('AnimNotifyState_PoseSearchExcludeFromDatabase missing')
        return
    AL.add_animation_notify_state_event(anim, track, ENTRY_MAX, L - ENTRY_MAX, cls)
    log(f'notify ExcludeFromDatabase {ENTRY_MAX:.2f}-{L:.2f} s')


def run():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar.scan_paths_synchronous(['/Game/GSTN'], True)
    if SRC:
        if eal.does_asset_exist(CLIP):
            eal.delete_asset(CLIP)
        if not eal.duplicate_asset(SRC, CLIP):
            errors.append(f'duplicate {SRC} failed')
            return
    anim = eal.load_asset(CLIP)
    if not anim:
        errors.append(f'missing {CLIP}')
        return
    if TAIL > 0:
        add_tail(anim)
    res = ue_rig()._bake_ik_feet(anim)
    log(f'ik feet {res}')
    if 'error' in res or res.get('ik_to_foot_max_cm', 99) > 0.5:
        errors.append(f'ik feet {res}')
    anim.set_editor_property('enable_root_motion', True)
    anim.set_editor_property('force_root_lock', False)
    add_notifies(anim)
    n = AL.get_num_keys(anim)
    for i in range(0, n, max(1, n // 8)):
        r = AL.get_bone_pose_for_frame(anim, 'root', i, False)
        log(f'root f{i} ({r.translation.x:.1f},{r.translation.y:.1f}) yaw {r.rotation.rotator().yaw:.1f}')
    if SAVE and not errors:
        eal.save_asset(CLIP, only_if_is_dirty=False)
        log('saved')


try:
    run()
except Exception as e:
    import traceback
    errors.append(repr(e)[:200])
    log(traceback.format_exc())
log(f'errors={errors}')
_f.close()
print(f'ESITO: {"OK" if not errors else "ERRORE " + "; ".join(errors)} prep {NAME} {"(salvato)" if SAVE else "(dry run)"}')
