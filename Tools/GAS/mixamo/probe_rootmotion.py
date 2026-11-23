# Read-only probe (skid-walkturn-1): root motion op settings of RTG_RootCorrection and of RTG_Mixamo_to_UEFN,
# and the component-space pelvis/feet track of a clip (env PROBE_CLIP). Writes Saved/Mixamo/probe_root.txt.
import math
import os
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'Mixamo', 'probe_root.txt')
f = open(OUT, 'w', encoding='utf-8')
eal = unreal.EditorAssetLibrary


def dump_ops(path):
    rtg = eal.load_asset(path)
    if not rtg:
        f.write(f'missing {path}\n')
        return
    c = unreal.IKRetargeterController.get_controller(rtg)
    for i in range(c.get_num_retarget_ops()):
        name = c.get_op_name(i)
        f.write(f'{path} op {i} {name} enabled={c.get_retarget_op_enabled(i)}\n')
        try:
            oc = c.get_op_controller(i)
        except Exception:
            oc = c.get_op_controller(name)
        if oc is None:
            continue
        try:
            s = oc.get_settings()
            for p in [n for n in dir(s) if not n.startswith('_') and not callable(getattr(s, n, None))]:
                f.write(f'    {p} = {getattr(s, p)}\n')
        except Exception as e:
            f.write(f'    settings error {e}\n')


dump_ops('/Game/GSTN/Animation/Ported57/SkidOwner/RTG_RootCorrection')
dump_ops('/Game/GSTN/Animation/Mixamo/Source/RTG_Mixamo_to_UEFN')


clip = os.environ.get('PROBE_CLIP', '/Game/GSTN/Animation/Mixamo/Source/AS_GSTN_WalkTurn180_Mixamo_Mixamo_UEFN')
anim = eal.load_asset(clip)
mesh = eal.load_asset('/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
if anim and mesh:
    L = unreal.AnimationLibrary.get_sequence_length(anim)
    n = unreal.AnimationLibrary.get_num_frames(anim)
    opts = unreal.AnimPoseEvaluationOptions()
    for i in range(n + 1):
        t = min(L, i / 30.0)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, t, opts)
        row = [f't={t:.3f}']
        for bn in ('root', 'pelvis', 'foot_l', 'foot_r', 'ik_foot_l', 'ik_foot_r'):
            x = unreal.AnimPoseExtensions.get_bone_pose(pose, bn, unreal.AnimPoseSpaces.WORLD)
            r = x.rotation.rotator()
            row.append(f'{bn}=({x.translation.x:.1f},{x.translation.y:.1f},{x.translation.z:.1f} y{r.yaw:.0f})')
        f.write(' '.join(row) + '\n')
f.close()
print('ESITO: OK probe_root')
