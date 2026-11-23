"""NOT RUN IN UE (written in the cloud, unverified): generic joints dump for ANY AnimSequence and frame range.

Modelled on cards/runstart/runstart_clip_joints_ue.py (same JSON, same Y-up conversion UE z -> y, UE y -> z), plus clavicles and
a frame range, so anim_silhouette_check / anim_motion_overlay / anim_capture_move's readability gate work on combat clips.
Run headless (Tools/CI/Save-Assets.ps1 -Script <this>, saves nothing). Env:
  CJD_ASSETS = comma-separated /Game/... AnimSequence paths (a bare name is looked up in /Game/GSTN/Animation/RunStart/)
  CJD_RANGE  = optional "first-last" (sampled frames of the clip, inclusive); default the whole clip
Writes Saved/AnimPipeline/runstart/joints_<name>.json {"fps", "joints": {...}, "first_frame": N}; prints one line per asset.
Joints are component space with root motion included, cm, the character facing +x."""
import json
import os

import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart")
os.makedirs(OUT, exist_ok=True)
MAP = {"root": "root"}
for _b in ("pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "head"):
    MAP[_b] = _b
for s in "lr":
    MAP["clavicle_" + s] = "clavicle_" + s
    for pt, bone in (("thigh_MainPoint_", "thigh_"), ("calf_MainPoint_", "calf_"), ("foot_MainPoint_", "foot_"), ("ball_MainPoint_", "ball_"),
                     ("upperarm_MainPoint_", "upperarm_"), ("lowerarm_MainPoint_", "lowerarm_"), ("hand_MainPoint_", "hand_")):
        MAP[pt + s] = bone + s
AL = unreal.AnimationLibrary
rng = os.environ.get("CJD_RANGE", "").strip()
for path in [a.strip() for a in os.environ.get("CJD_ASSETS", "").split(",") if a.strip()]:
    if "/" not in path:
        path = "/Game/GSTN/Animation/RunStart/" + path
    name = path.rsplit("/", 1)[-1]
    seq = unreal.load_asset(path)
    if not seq:
        print("CJD %s: missing" % path)
        continue
    n = int(seq.get_editor_property("number_of_sampled_frames")) if hasattr(seq, "number_of_sampled_frames") else AL.get_num_frames(seq) + 1
    fps = 30.0
    if hasattr(seq, "sampling_frame_rate"):
        r = seq.get_editor_property("sampling_frame_rate")
        fps = float(r.numerator) / max(1, int(r.denominator))
    a, b = 0, n - 1
    if rng:
        a, b = [int(x) for x in rng.split("-")]
        a, b = max(0, a), min(n - 1, b)
    opts = unreal.AnimPoseEvaluationOptions()
    J = {k: [] for k in MAP}
    for f in range(a, b + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / fps, opts)
        for k, bone in MAP.items():
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
            J[k].append([t.x, t.z, t.y])
    with open(os.path.join(OUT, "joints_%s.json" % name), "w", encoding="utf-8") as fh:
        json.dump({"fps": fps, "first_frame": a, "joints": J}, fh)
    print("CJD %s: frames %d-%d of %d" % (name, a, b, n))
