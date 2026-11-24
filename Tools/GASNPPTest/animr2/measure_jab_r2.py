import unreal, json, math
D = '/Game/GSTN/Characters/Overlord/Moves/Jab/'
def pose(anim, f, bone):
    p = unreal.AnimPoseExtensions.get_anim_pose_at_time(anim, f / 60.0, unreal.AnimPoseEvaluationOptions())
    t = unreal.AnimPoseExtensions.get_bone_pose(p, bone, unreal.AnimPoseSpaces.WORLD).translation
    return (t.x, t.y, t.z)
a1 = unreal.load_asset(D + 'AS_OverlordJab1V3'); a2 = unreal.load_asset(D + 'AS_OverlordJab2V3')
out = {'len': (a1.get_play_length(), a2.get_play_length())}
r = lambda v: [round(x, 1) for x in v]
out['j1'] = {f: r(pose(a1, f, 'hand_r')) for f in (0, 6, 8, 12)}
out['j2'] = {f: r(pose(a2, f, 'hand_r')) for f in (0, 9, 11, 26)}
d = lambda a, b: round(math.dist(a, b), 2)
out['f26'] = {b: d(pose(a2, 26, b), pose(a2, 52, b)) for b in ('pelvis', 'foot_l', 'foot_r')}
out['pelvis_f9'] = r(pose(a2, 9, 'pelvis')); out['pelvis_idle'] = r(pose(a2, 52, 'pelvis'))
print(json.dumps(out))
