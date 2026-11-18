# Read only: per-key jump of head / feet / pelvis (component space, walking the parent chain) in the skid clips, to tell a pop
# inside the clip (posa) from one made by the game. Writes Saved/SkidOwner/diag_skid_clip_jumps.txt, then quits the editor.
import os
import unreal

AL = unreal.AnimationLibrary
CLIPS = ['/Game/Blueprints/Animation/Ported57/Skid/SS_180_Turn', '/Game/Blueprints/Animation/Ported57/SkidOwner/SS_180_Turn_Root180']
CHAINS = {'head': ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'neck_01', 'neck_02', 'head'],
          'foot_l': ['root', 'pelvis', 'thigh_l', 'calf_l', 'foot_l'], 'foot_r': ['root', 'pelvis', 'thigh_r', 'calf_r', 'foot_r'],
          'pelvis': ['root', 'pelvis']}
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'SkidOwner', 'diag_skid_clip_jumps.txt')
rows = []
try:
    for path in CLIPS:
        a = unreal.load_asset(path)
        n = AL.get_num_keys(a)
        tracks = set(str(t) for t in AL.get_animation_track_names(a))
        rows.append(f'{path} keys={n} len={a.get_play_length():.3f}')
        prev = None
        for i in range(n):
            cur = {}
            for k, chain in CHAINS.items():
                t = None
                for b in chain:
                    if b not in tracks:
                        continue
                    lt = AL.get_bone_pose_for_frame(a, b, i, False)
                    t = lt if t is None else lt.multiply(t)
                cur[k] = t.translation
            if prev:
                rows.append(f'  k{i:02d} ' + ' '.join(f'{k}={(cur[k] - prev[k]).length():5.1f}' for k in CHAINS))
            prev = cur
except Exception as e:
    rows.append(f'ERROR {e!r}')
open(OUT, 'w').write('\n'.join(rows) + '\n')
print(f'ESITO: {"FAIL" if any(r.startswith("ERROR") for r in rows) else "OK"} diag skid clip jumps -> {OUT}')
unreal.SystemLibrary.quit_editor()
