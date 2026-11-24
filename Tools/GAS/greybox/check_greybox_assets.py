"""greybox-capsula: read-only check of the generated greybox assets (never saves; safe through Tools/CI/Save-Assets.ps1).

Reports: the capsule ABP's MoveOverrides as the game loads them (class default object), and for a few capsule clips how far the body
chain bones turn from the reference pose at the clip's key times (a clip that wrote no rotation shows ~0 degrees everywhere).
Report: Saved/Greybox/check_greybox_assets.txt.
"""
import math
import os

import unreal

CAPSULE_ABP = '/Game/GSTN/Greybox/ABP_GSTN_GreyboxCapsule'
CLIPS = ['/Game/GSTN/Greybox/Clips/AS_GSTN_Greybox_Run', '/Game/GSTN/Greybox/Clips/AS_GSTN_Greybox_Move_AM_OverlordDropkick',
         '/Game/GSTN/Greybox/Clips/AS_GSTN_Greybox_Move_AM_OverlordDAirV10', '/Game/GSTN/Greybox/Clips/AS_GSTN_Greybox_Move_AM_OverlordJab1V3']
BONES = ['pelvis', 'spine_03', 'spine_05', 'head', 'thigh_l', 'calf_l', 'upperarm_r']
OUT = unreal.Paths.project_saved_dir() + 'Greybox/check_greybox_assets.txt'
lines = []


def p(*a):
    lines.append(' '.join(str(x) for x in a))


def angle(q1, q2):
    d = abs(q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w)
    return math.degrees(2.0 * math.acos(min(1.0, d)))


def run():
    bp = unreal.EditorAssetLibrary.load_asset(CAPSULE_ABP)
    gen = unreal.load_object(None, CAPSULE_ABP + '.ABP_GSTN_GreyboxCapsule_C')
    p('abp', bp, 'generated', gen)
    cdo = unreal.get_default_object(gen) if gen else None
    if cdo:
        mo = cdo.get_editor_property('move_overrides')
        p('move_overrides on the CDO:', len(mo))
        for k, v in list(mo.items())[:30]:
            p('  ', k, '->', v)
    for path in CLIPS:
        seq = unreal.EditorAssetLibrary.load_asset(path)
        if not seq:
            p('clip missing', path)
            continue
        skel = seq.get_editor_property('skeleton')
        ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
        length = seq.get_play_length()
        p('clip', path, 'length %.3f' % length, 'frames', unreal.AnimationLibrary.get_num_frames(seq) if hasattr(unreal, 'AnimationLibrary') else '?')
        opts = unreal.AnimPoseEvaluationOptions()
        for t in (0.0, length * 0.3, length * 0.5):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, opts)
            row = []
            for b in BONES:
                a = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL).rotation
                r = unreal.AnimPoseExtensions.get_bone_pose(ref, b, unreal.AnimPoseSpaces.LOCAL).rotation
                row.append('%s %.0f' % (b, angle(a, r)))
            pel = unreal.AnimPoseExtensions.get_bone_pose(pose, 'pelvis', unreal.AnimPoseSpaces.WORLD).translation
            p('   t %.2f deg from ref:' % t, ', '.join(row), '| pelvis z %.1f' % pel.z)


try:
    run()
except Exception as exc:
    import traceback
    p('EXCEPTION', exc)
    p(traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')
print('[check_greybox_assets] report', OUT)
