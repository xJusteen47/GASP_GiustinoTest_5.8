# Turnaround run starts (runstart-turnaround-desktop, 2026-11-21): export the stock B / LL / RL relaxed run starts (L and R foot) to
# FBX + meta (curves, notifies) and the root bone track per frame (position cm, yaw deg), read-only. Runs inside an open editor
# through the built-in MCP: python Tools/AnimPipeline/cards/runstart/runstart_turn_export_ue.py  (prints <= 5 lines)
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else '.'
ROOT = HERE.split(os.sep + 'Tools' + os.sep)[0]
OUT = os.path.join(ROOT, 'Saved', 'AnimPipeline', 'runstart', 'turn').replace(chr(92), '/')
CLIPS = ['M_Relaxed_Run_Reface_Start_%s_%s_%sfoot' % (a, s, f) for a in ('090', '180') for s in ('L', 'R') for f in ('L', 'R')] + ['M_Relaxed_Run_Start_F_%sfoot' % f for f in ('L', 'R')]  # turnarounds of the orient-to-movement game (Run F Loco Transitions); B/LL/RL are strafe starts
SCRIPT = r'''
import unreal, json, os, math
AL = unreal.AnimationLibrary
OUT = %(out)r
res = {}
for c in %(clips)r:
    a = unreal.load_asset('/Game/Characters/UEFN_Mannequin/Animations/Run/%%s.%%s' %% (c, c))
    n = int(AL.get_num_frames(a)) + 1
    fps = (n - 1) / float(AL.get_sequence_length(a))
    t = unreal.AssetExportTask(); t.object, t.filename, t.automated, t.replace_identical, t.prompt = a, OUT + '/' + c + '.fbx', True, True, False
    t.exporter = unreal.AnimSequenceExporterFBX(); o = unreal.FbxExportOption(); o.set_editor_property('export_preview_mesh', False); t.options = o
    ok = unreal.Exporter.run_asset_export_task(t)
    root, pel = [], []
    for f in range(n):
        tr = AL.get_bone_pose_for_frame(a, 'root', f, False)
        p = tr.translation; r = tr.rotation.rotator()
        root.append([p.x, p.y, p.z, r.yaw])
    curves = {}
    for nm in AL.get_animation_curve_names(a, unreal.RawCurveTrackTypes.RCT_FLOAT):
        ts, vs = AL.get_float_keys(a, nm); curves[str(nm)] = [list(ts), list(vs)]
    nts = []
    for ev in AL.get_animation_notify_events(a):
        try:
            nts.append([ev.get_editor_property('notify').get_class().get_name() if ev.get_editor_property('notify') else str(ev.get_editor_property('notify_name')), float(ev.get_editor_property('trigger_time_offset')) if False else float(AL.get_animation_notify_event_trigger_time(a, ev)) if hasattr(AL, 'get_animation_notify_event_trigger_time') else -1])
        except Exception as e:
            nts.append([str(e)[:80], -1])
    json.dump({'asset': a.get_path_name(), 'fps': fps, 'frames': n, 'root': root, 'curves': curves, 'notifies': nts}, open(OUT + '/' + c + '.meta.json', 'w'))
    res[c] = [ok, n, fps, round(root[-1][3], 1)]
json.dumps(res)
'''
try:                                     # headless: UnrealEditor-Cmd -run=pythonscript -script=<copy without spaces>
    import unreal
    OUT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()).rstrip('/') + '/Saved/AnimPipeline/runstart/turn'
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Characters'], True)
    os.makedirs(OUT, exist_ok=True)
    exec(SCRIPT.replace('json.dumps(res)', 'json.dump(res, open(OUT + "/export_result.json", "w"))') % {'out': OUT, 'clips': CLIPS}, {})
except ImportError:                      # outside the editor: through the built-in MCP of an open editor
    sys.path.insert(0, os.path.join(ROOT, 'Tools', 'AnimPipeline'))
    import ue_builtin
    os.makedirs(OUT, exist_ok=True)
    r = ue_builtin.execute(SCRIPT % {'out': OUT, 'clips': CLIPS})
    json.dump(r, open(OUT + '/export_result.json', 'w'), indent=1)
    print('ESITO: OK %d clip' % len(r) if all(v[0] for v in r.values()) else 'ESITO: FALLITO %s' % r)
