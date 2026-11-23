"""Dump the imported clip's joints for the gates (turnarounds-3, 2026-11-21): runstart_pivot_gates.py --json runs on what UE plays
(after Cascadeur's solve and the FBX import), not only on the composer targets. Run headless: Tools/CI/Save-Assets.ps1 -Script <this>
(saves nothing). Env RCJ_ASSETS = comma-separated AnimSequence names in /Game/GSTN/Animation/RunStart/.
Writes Saved/AnimPipeline/runstart/joints_<asset>.json {"fps", "joints": {rig point name: [[x, y, z] ...]}} in component space
(root motion included) converted to Y up (UE z -> y, UE y -> z); prints 1 line per asset."""
import json
import os

import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart")
os.makedirs(OUT, exist_ok=True)
MAP = {"root": "root"}
for s in "lr":
    for pt, bone in (("thigh_MainPoint_", "thigh_"), ("calf_MainPoint_", "calf_"), ("foot_MainPoint_", "foot_"), ("ball_MainPoint_", "ball_"),
                     ("upperarm_MainPoint_", "upperarm_"), ("lowerarm_MainPoint_", "lowerarm_"), ("hand_MainPoint_", "hand_")):
        MAP[pt + s] = bone + s
AL = unreal.AnimationLibrary
for name in [a.strip().rsplit("/", 1)[-1] for a in os.environ.get("RCJ_ASSETS", "").split(",") if a.strip()]:
    seq = unreal.load_asset("/Game/GSTN/Animation/RunStart/" + name)
    if not seq:
        print("RCJ %s: missing" % name); continue
    n = int(seq.get_editor_property("number_of_sampled_frames")) if hasattr(seq, "number_of_sampled_frames") else AL.get_num_frames(seq) + 1
    opts = unreal.AnimPoseEvaluationOptions()
    J = {k: [] for k in MAP}
    AX = {"foot_axes_" + s: [] for s in "lr"}   # foot bone X/Y/Z axes (Y up), for the roll gate (owner: rolled right foot)
    for f in range(n):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0, opts)
        for k, b in MAP.items():
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
            J[k].append([t.x, t.z, t.y])
        for s in "lr":
            q = unreal.AnimPoseExtensions.get_bone_pose(pose, "foot_" + s, unreal.AnimPoseSpaces.WORLD).rotation
            AX["foot_axes_" + s].append([[a.x, a.z, a.y] for a in (q.get_axis_x(), q.get_axis_y(), q.get_axis_z())])
    with open(os.path.join(OUT, f"joints_{name}.json"), "w", encoding="utf-8") as fh:
        json.dump({"fps": 30, "joints": J, "axes": AX}, fh)
    print("RCJ %s: %d frames" % (name, n))
