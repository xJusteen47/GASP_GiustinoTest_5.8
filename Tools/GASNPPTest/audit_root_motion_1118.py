"""Root motion audit (2026-11-18, read only): for every AnimSequence under the folders below, the root and pelvis component-space
travel start -> end and the max root yaw change. 'Correct' root motion = the root carries the travel (pelvis minus root ~ in place)
and EnableRootMotion is on. Writes Saved/Diag1118/root_audit.tsv, prints one line.
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this> -unattended -nullrhi"""
import os, math
import unreal

FOLDERS = ['/Game/GSTN/Animation/Ported57/Skid', '/Game/GSTN/Animation/Ported57/SkidOwner',
           '/Game/GSTN/Animation/Ported57/WallSlide', '/Game/GiustinoMoves']
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Saved', 'Diag1118')
os.makedirs(OUT, exist_ok=True)
AL = unreal.AnimationLibrary


def _cs(anim, bone, t):
    """Component-space transform of bone at time t (walks the parent chain through the skeleton reference)."""
    tr = AL.get_bone_pose_for_time(anim, bone, t, False)
    return tr


def _pelvis_cs(anim, t):
    root = AL.get_bone_pose_for_time(anim, 'root', t, False)
    pel = AL.get_bone_pose_for_time(anim, 'pelvis', t, False)   # pelvis is a child of root on the UE mannequin
    return root, root.multiply(pel) if hasattr(root, 'multiply') else pel


rows = ['asset\troot_motion_on\tlength_s\troot_travel_cm\troot_yaw_deg\tpelvis_travel_cm\tpelvis_vs_root_cm']
for folder in FOLDERS:
    for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=True, include_folder=False):
        a = unreal.load_asset(path)
        if not isinstance(a, unreal.AnimSequence):
            continue
        try:
            L = a.get_play_length()
            r0, p0 = _pelvis_cs(a, 0.0)
            r1, p1 = _pelvis_cs(a, L)
            rt = r1.translation - r0.translation
            yaw = r1.rotation.rotator().yaw - r0.rotation.rotator().yaw
            pt = p1.translation - p0.translation
            local0 = AL.get_bone_pose_for_time(a, 'pelvis', 0.0, False).translation
            local1 = AL.get_bone_pose_for_time(a, 'pelvis', L, False).translation
            rel = math.hypot(local1.x - local0.x, local1.y - local0.y)
            rows.append(f'{path.split(".")[0]}\t{a.get_editor_property("enable_root_motion")}\t{L:.2f}\t{math.hypot(rt.x, rt.y):.1f}\t{yaw:.1f}\t'
                        f'{math.hypot(pt.x, pt.y):.1f}\t{rel:.1f}')
        except Exception as e:
            rows.append(f'{path}\tERR {e!r}'[:200])
open(os.path.join(OUT, 'root_audit.tsv'), 'w').write('\n'.join(rows) + '\n')
print(f'ROOT_AUDIT {len(rows) - 1} clips -> {OUT}')
