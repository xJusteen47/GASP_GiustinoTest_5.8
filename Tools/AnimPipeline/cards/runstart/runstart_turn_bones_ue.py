# Turnaround run starts: component-space positions (cm, UE mesh space: +Y forward, +X left, +Z up) of a few bones per frame of the
# stock starts, read-only, through the built-in MCP of an open editor. Writes Saved/AnimPipeline/runstart/turn/<clip>.bones.json.
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else '.'; ROOT = HERE.split(os.sep + 'Tools' + os.sep)[0]
OUT = os.path.join(ROOT, 'Saved', 'AnimPipeline', 'runstart', 'turn').replace(chr(92), '/')
CLIPS = ['M_Relaxed_Run_Reface_Start_%s_%s_%sfoot' % (a, s, f) for a in ('090', '180') for s in ('L', 'R') for f in ('L', 'R')] + ['M_Relaxed_Run_Start_F_%sfoot' % f for f in ('L', 'R')]  # turnarounds of the orient-to-movement game (Run F Loco Transitions); B/LL/RL are strafe starts
SCRIPT = r'''
import unreal, json
AL = unreal.AnimationLibrary
CH = {'pelvis': ['root', 'pelvis'], 'thigh_l': ['root', 'pelvis', 'thigh_l'], 'thigh_r': ['root', 'pelvis', 'thigh_r'],
      'spine_05': ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05'],
      'clavicle_l': ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'clavicle_l'],
      'clavicle_r': ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'clavicle_r'],
      'foot_l': ['root', 'pelvis', 'thigh_l', 'calf_l', 'foot_l'], 'foot_r': ['root', 'pelvis', 'thigh_r', 'calf_r', 'foot_r'],
      'ball_l': ['root', 'pelvis', 'thigh_l', 'calf_l', 'foot_l', 'ball_l'], 'ball_r': ['root', 'pelvis', 'thigh_r', 'calf_r', 'foot_r', 'ball_r']}
res = {}
for c in %(clips)r:
    a = unreal.load_asset('/Game/Characters/UEFN_Mannequin/Animations/Run/%%s.%%s' %% (c, c))
    n = int(AL.get_num_frames(a)) + 1
    D = {k: [] for k in CH}
    for f in range(n):
        cache = {}
        for k, chain in CH.items():
            t = None
            for i, b in enumerate(chain):
                key = tuple(chain[:i + 1])
                if key not in cache:
                    l = AL.get_bone_pose_for_frame(a, b, f, False)
                    cache[key] = l if t is None else unreal.MathLibrary.compose_transforms(l, t)
                t = cache[key]
            p = t.translation; D[k].append([round(p.x, 2), round(p.y, 2), round(p.z, 2)])
    json.dump(D, open(%(out)r + '/' + c + '.bones.json', 'w'))
    res[c] = n
json.dumps(res)
'''
try:                                     # headless: UnrealEditor-Cmd -run=pythonscript -script=<copy without spaces>
    import unreal
    ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()).rstrip('/')
    OUT = ROOT + '/Saved/AnimPipeline/runstart/turn'
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Characters'], True)
    _g = {}
    exec(SCRIPT % {'out': OUT, 'clips': CLIPS}, _g)
    open(OUT + '/bones_done.txt', 'w').write('OK')
except ImportError:                      # outside the editor: through the built-in MCP of an open editor
    sys.path.insert(0, os.path.join(ROOT, 'Tools', 'AnimPipeline'))
    import ue_builtin
    r = ue_builtin.execute(SCRIPT % {'out': OUT, 'clips': CLIPS}, timeout=600)
    print('ESITO: OK', r)
