# Skid root fix, 180 deg exact (thread "Fix skid clip root rotation for GASP", 2026-11-18).
# Owner ask: take the FIRST, unmodified skid clip (SS_180_Turn, root never turns) and fix the root so it turns 180 deg exactly once,
# when the body turns, so GASP motion matching / orientation warping can drive it. The owner's RTG_RootCorrection (IK Retargeter,
# GenerateFromTargetPelvis) is NOT touched: an RTG copies the pelvis yaw as is, so it cannot land on exactly 180 (SS_180_Turn2
# re-retargeted ended at -188). This script is the "copy" of that correction, done on the bone tracks of a COPY of the clip:
#   root.xy  = pelvis.xy (component space) minus the start offset, root.z unchanged   (same as the RTG: root follows the pelvis)
#   root.yaw = start yaw + 180 * s(t), s = pelvis heading progress (0..1, monotonic, clamped) -> exactly 180 at the end, no overshoot
#   every direct child of root (pelvis, ik_*_root, ...) is re-expressed so the world pose of every bone is unchanged.
# Env: SRC (default Ported57/Skid/SS_180_Turn), DST (default Ported57/SkidOwner/SS_180_Turn_Root180).
# Writes Saved/SkidOwner/root_fix_skid_180.log; prints one ESITO line.
import math
import os
import unreal

ANIM = '/Game/GSTN/Animation'
SRC = os.environ.get('SRC', f'{ANIM}/Ported57/Skid/SS_180_Turn')
DST = os.environ.get('DST', f'{ANIM}/Ported57/SkidOwner/SS_180_Turn_Root180')
ROOT_CHILDREN = ['pelvis', 'ik_foot_root', 'ik_hand_root', 'interaction', 'center_of_mass']
LOG = os.path.join(unreal.Paths.project_saved_dir(), 'SkidOwner', 'root_fix_skid_180.log')
os.makedirs(os.path.dirname(LOG), exist_ok=True)
_f = open(LOG, 'w', encoding='utf-8')
AL = unreal.AnimationLibrary
eal = unreal.EditorAssetLibrary
errors = []


def log(s):
    _f.write(str(s) + '\n')
    _f.flush()


def yaw_quat(yaw):
    return unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw).quaternion()


def heading(q, local_fwd):
    v = q.rotate_vector(local_fwd)
    return math.degrees(math.atan2(v.y, v.x))


def run():
    if eal.does_asset_exist(DST):
        eal.delete_asset(DST)
    if not eal.duplicate_asset(SRC, DST):
        errors.append(f'duplicate {SRC} -> {DST} failed')
        return
    anim = eal.load_asset(DST)
    n = AL.get_num_keys(anim)
    tracks = [str(t) for t in AL.get_animation_track_names(anim)]
    kids = [b for b in ROOT_CHILDREN if b in tracks]
    log(f'src={SRC} dst={DST} keys={n} root children={kids} root motion={anim.get_editor_property("enable_root_motion")}')
    root = [AL.get_bone_pose_for_frame(anim, 'root', i, False) for i in range(n)]
    cs = {b: [AL.get_bone_pose_for_frame(anim, b, i, False).multiply(root[i]) for i in range(n)] for b in kids}
    pel = cs['pelvis']
    # pelvis heading: a horizontal component-space axis carried by the pelvis rotation (robust to the pelvis bone's own axes)
    local_fwd = pel[0].rotation.unrotate_vector(unreal.Vector(0.0, 1.0, 0.0))
    h, prev, acc = [], None, 0.0
    for t in pel:
        a = heading(t.rotation, local_fwd)
        if prev is not None:
            d = (a - prev + 180.0) % 360.0 - 180.0
            acc += d
        prev = a
        h.append(acc)
    total = h[-1]
    sign = 1.0 if total >= 0 else -1.0
    log(f'pelvis heading change start->end {total:.1f} deg (max |h| {max(abs(x) for x in h):.1f})')
    s, run_max = [], 0.0
    for x in h:
        run_max = max(run_max, min(1.0, max(0.0, x / total if abs(total) > 1e-3 else 0.0)))
        s.append(run_max)
    yaw0 = root[0].rotation.rotator().yaw
    off = pel[0].translation - root[0].translation
    new_root = []
    for i in range(n):
        p = pel[i].translation
        loc = unreal.Vector(p.x - off.x, p.y - off.y, root[i].translation.z)
        new_root.append(unreal.Transform(location=loc, rotation=yaw_quat(yaw0 + sign * 180.0 * s[i]).rotator(), scale=unreal.Vector(1, 1, 1)))
    ctrl = anim.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('root fix 180'))
    ctrl.set_bone_track_keys('root', [t.translation for t in new_root], [t.rotation for t in new_root], [unreal.Vector(1, 1, 1)] * n)
    for b in kids:
        locs = [cs[b][i].multiply(new_root[i].inverse()) for i in range(n)]
        ctrl.set_bone_track_keys(b, [t.translation for t in locs], [t.rotation for t in locs], [t.scale3d for t in locs])
    ctrl.close_bracket()
    # verify: root yaw end, world pelvis unchanged, root travel
    worst = 0.0
    for i in range(n):
        r = AL.get_bone_pose_for_frame(anim, 'root', i, False)
        pw = AL.get_bone_pose_for_frame(anim, 'pelvis', i, False).multiply(r).translation
        worst = max(worst, (pw - pel[i].translation).length())
    r_end = AL.get_bone_pose_for_frame(anim, 'root', n - 1, False)
    dyaw = (r_end.rotation.rotator().yaw - yaw0 + 180.0) % 360.0 - 180.0
    half = next((i for i in range(n) if s[i] >= 0.5), -1)
    log(f'root yaw start->end {dyaw:.1f} deg (sign {sign:+.0f}); turn 10%/50%/90% at keys '
        f'{next((i for i in range(n) if s[i] >= 0.1), -1)}/{half}/{next((i for i in range(n) if s[i] >= 0.9), -1)} of {n}')
    log(f'root travel {(r_end.translation - new_root[0].translation).length():.1f} cm; pelvis world drift max {worst:.3f} cm')
    if abs(abs(dyaw) - 180.0) > 0.5 and abs(abs(dyaw) - 180.0) < 179.5:
        errors.append(f'root yaw {dyaw:.1f} not 180')
    if worst > 0.5:
        errors.append(f'pelvis drift {worst:.2f} cm')
    eal.save_asset(DST, only_if_is_dirty=False)


try:
    run()
except Exception as e:
    errors.append(repr(e))
for e in errors:
    log('ERROR ' + e)
_f.close()
print(f'{"ESITO" if __name__ == "__main__" else "ROOTFIX"}: {"OK" if not errors else "FAIL"} root fix skid 180 ({len(errors)} errors) log {LOG}')
