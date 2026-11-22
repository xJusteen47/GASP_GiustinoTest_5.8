"""Dump clips' key bones (component space, 60 Hz) to JSON for pose_match.py. Headless:
  set CLIP_DUMP_OUT=<json> and CLIP_DUMP_CLIPS=<asset path>;<asset path>...  then
  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -nullrhi
Prints max 3 lines."""
import json, os
import unreal

BONES = ["root", "pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r"]
out, clips = os.environ["CLIP_DUMP_OUT"], [c for c in os.environ["CLIP_DUMP_CLIPS"].split(";") if c]
data = {}
for path in clips:
    seq = unreal.load_asset(path)
    n = unreal.AnimationLibrary.get_num_frames(seq) if hasattr(unreal.AnimationLibrary, "get_num_frames") else None
    length = seq.get_play_length()
    rows, t = [], 0.0
    opts = unreal.AnimPoseEvaluationOptions()
    while t <= length + 1e-4:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, opts)
        r = {}
        for b in BONES:
            tr = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
            r[b] = [tr.x, tr.y, tr.z]
        rows.append(r); t += 1.0 / 60
    data[path.rsplit("/", 1)[-1].split(".")[0]] = rows
json.dump(data, open(out, "w"))
print("clips dumped: " + ", ".join(f"{k}={len(v)}" for k, v in data.items()))
