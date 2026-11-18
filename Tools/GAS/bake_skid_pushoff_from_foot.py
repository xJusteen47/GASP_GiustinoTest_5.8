"""Skid attempt 2 (skid-rootmotion-1 review: run restart FAIL): re-bake the PUSH-OFF part of DA_Locomotion_Default's skid root track
from the planted foot instead of the root bone.

The owner clip's root is pelvis-derived and moves only ~85 cm/s in its tail while the legs run, so the capsule crept in a run pose and
then the walking mode jumped to full speed. With the mesh following the clip's root yaw (Offset Root Bone, Accumulate), component space
is world space up to a constant, so a planted foot stays still when capsule velocity = -d/dt(foot - root) in component space. For every
sample after the clip's rest (Speed <= 0) the speed along the entry direction is taken from the lower (planted) foot, smoothed over 3
samples and kept <= 0 (the rest frame and the brake, which passed review, do not change). Summary: Saved/Logs/ci/bake_skid_pushoff.txt
"""
import math
import os
import unreal

PROFILE = "/Game/GASNPPTest/Combat/DA_Locomotion_Default"
CLIP = "/Game/Blueprints/Animation/Ported57/SkidOwner/SS_180_Turn_Root180"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "bake_skid_pushoff.txt")


def bone(pose, name):
    t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
    return t.translation


def main():
    lines = []
    profile = unreal.load_asset(PROFILE)
    stages = profile.get_editor_property("run_stages")
    skid = stages.get_editor_property("skid")
    track = skid.get_editor_property("root_track")
    clip = unreal.load_asset(CLIP)
    speed = list(track.get_editor_property("speed"))
    rate = track.get_editor_property("sample_rate")
    n = len(speed)
    length = clip.get_play_length()
    opts = unreal.AnimPoseEvaluationOptions()
    rel = []
    for i in range(n):
        t = min(i / rate, length)
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, t, opts)
        root = bone(pose, "root")
        fl, fr = bone(pose, "foot_l"), bone(pose, "foot_r")
        rel.append(((fl.x - root.x, fl.y - root.y, fl.z), (fr.x - root.x, fr.y - root.y, fr.z)))
    # Entry direction as in the C++ bake: where the root goes over its first 5 cm (re-derive from the root speed sign is enough here:
    # push-off is along -entry). Take entry from the first 5 cm of root travel.
    pose0 = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, 0.0, opts)
    r0 = bone(pose0, "root")
    entry = None
    for i in range(1, n):
        p = bone(unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, min(i / rate, length), opts), "root")
        d = (p.x - r0.x, p.y - r0.y)
        m = math.hypot(*d)
        if m > 5.0:
            entry = (d[0] / m, d[1] / m)
            break
    if entry is None:
        return ["ESITO: FAIL no entry direction"]
    rest = next((i for i, v in enumerate(speed) if v <= 0.0), n)
    raw = []
    for i in range(n):
        a, b = max(i - 1, 0), min(i + 1, n - 1)
        span = (b - a) / rate
        planted = 0 if rel[i][0][2] <= rel[i][1][2] else 1
        pa, pb = rel[a][planted], rel[b][planted]
        v = -((pb[0] - pa[0]) * entry[0] + (pb[1] - pa[1]) * entry[1]) / span if span > 0 else 0.0
        raw.append(v)
    new = list(speed)
    for i in range(rest, n):
        window = raw[max(i - 1, rest):min(i + 2, n)]
        new[i] = min(sum(window) / len(window), 0.0)
    track.set_editor_property("speed", new)
    skid.set_editor_property("root_track", track)
    stages.set_editor_property("skid", skid)
    profile.set_editor_property("run_stages", stages)
    lines.append("entry=(%.2f,%.2f) rest=%d n=%d" % (entry[0], entry[1], rest, n))
    for i in range(rest - 2, n, 2):
        lines.append("t=%.3f root=%7.1f foot=%7.1f new=%7.1f" % (i / rate, speed[i], raw[i], new[i]))
    saved = unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
    lines.append("ESITO: %s saved=%s end speed=%.0f" % ("OK" if saved else "FAIL", saved, new[-1]))
    return lines


try:
    result = main()
except Exception as exc:
    result = ["ESITO: FAIL %r" % exc]
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as handle:
    handle.write("\n".join(result) + "\n")
