"""Prove that a moved approved AnimSequence kept its animation: every bone, every frame, old package vs new package.

A rename rewrites the package name inside the .uasset, so the git blob of an approved asset always changes when the Content
reorganisation moves it (regress.py then FAILs on approved/dair-v9). This compares what the blob cannot: the bone poses.
The caller puts the approved file back at its old path first (from the tag) and removes it afterwards.

Run headless: UnrealEditor-Cmd.exe GASP_Giustino2.uproject -run=pythonscript -script="<abs>/Tools/Reorg/compare_moved_anim.py"
Env CMP_OLD, CMP_NEW: the two package paths. Writes Saved/Logs/ci/compare_moved_anim.txt (last line "CONFRONTO: OK|FAIL ...").
"""
import os

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(ROOT, 'Saved', 'Logs', 'ci', 'compare_moved_anim.txt')
OLD, NEW = os.environ['CMP_OLD'], os.environ['CMP_NEW']
TOL_CM, TOL_DEG = 0.001, 0.001

lines = []


def done(msg):
    lines.append(msg)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(msg)


a, b = unreal.load_asset(OLD), unreal.load_asset(NEW)
if not a or not b:
    done(f'CONFRONTO: FAIL non caricati old={bool(a)} new={bool(b)}')
    raise SystemExit
lib = unreal.AnimationLibrary
na, nb = lib.get_num_frames(a), lib.get_num_frames(b)
bones_a = [str(n) for n in lib.get_animation_track_names(a)]
bones_b = [str(n) for n in lib.get_animation_track_names(b)]
lines.append(f'frames {na} / {nb}, tracce {len(bones_a)} / {len(bones_b)}, skeleton {a.get_editor_property("skeleton").get_path_name()} / '
             f'{b.get_editor_property("skeleton").get_path_name()}')
if na != nb or bones_a != bones_b:
    done('CONFRONTO: FAIL frame o tracce diverse')
    raise SystemExit
worst_cm = worst_deg = 0.0
where = ''
for fr in range(na):
    pa = lib.get_bone_poses_for_frame(a, bones_a, fr, False)
    pb = lib.get_bone_poses_for_frame(b, bones_b, fr, False)
    for name, ta, tb in zip(bones_a, pa, pb):
        d = (ta.translation - tb.translation).length()
        q = ta.rotation.inversed() * tb.rotation
        deg = abs(q.get_angle()) * 57.29577951308232
        deg = min(deg, 360.0 - deg)
        if d > worst_cm or deg > worst_deg:
            worst_cm, worst_deg = max(worst_cm, d), max(worst_deg, deg)
            where = f'{name} f{fr}'
curves_a = sorted(str(c) for c in lib.get_animation_curve_names(a, unreal.RawCurveTrackTypes.RCT_FLOAT))
curves_b = sorted(str(c) for c in lib.get_animation_curve_names(b, unreal.RawCurveTrackTypes.RCT_FLOAT))
for c in curves_a:
    ka, kb = lib.get_float_keys(a, c), lib.get_float_keys(b, c)
    if [list(x) for x in ka] != [list(x) for x in kb]:
        lines.append(f'curva diversa {c}')
        curves_b = curves_b + ['<diversa>']
notif_a = sorted(str(n) for n in lib.get_animation_notify_event_names(a))
notif_b = sorted(str(n) for n in lib.get_animation_notify_event_names(b))
lines.append(f'max delta {worst_cm:.6f} cm, {worst_deg:.6f} deg ({where}); curve {len(curves_a)} / {len(curves_b)}; '
             f'notify {len(notif_a)} / {len(notif_b)}')
ok = worst_cm <= TOL_CM and worst_deg <= TOL_DEG and curves_a == curves_b and notif_a == notif_b
done(f'CONFRONTO: {"OK" if ok else "FAIL"} {OLD} vs {NEW}')
