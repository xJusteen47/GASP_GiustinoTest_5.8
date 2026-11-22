"""Give a custom pivot clip the stock GASP pivot setup (skid-walkturn-3, owner guidance 2026-11-20: fix the setup, not the clip).

Stock reference (Saved/Pivot/pivot_clips_dump.txt, pivot_curves_sample.txt, from pivot_rows_dump.py / pivot_curves_sample.py):
M_Relaxed_Run_Turn_L/R_180_* have EnableRootMotion + ForceRootLock (RefPose) and the curves contact_l/r, phase (kept from
curves_author.py on our clip), plus
  movedata_speed     = root speed (cm/s); Get_DynamicPlayRate plays the clip at capsule speed / this value;
  enable_warping     = 1 on the run in / run out, 0 in the turn core (yaw changing), ramps back;
  steeringtargettime = 1 until the turn is done, then falls to 0 (~0.4 s).
A curve the clip lacks reads 0 (gasp-structure-5.9.md section 8): without enable_warping the orientation/stride warping is OFF,
without movedata_speed the play rate is undefined. This script writes the three curves from the clip's own root track and turns
the root lock on. Env PIVOT_CLIP (asset path), PIVOT_WARP = stock (default: 0 while |yaw rate| > PIVOT_YAWRATE deg/s, smoothed)
or one (constant 1). Saves only with GSTN_SAVE=1. Report Saved/Pivot/clip_setup_<name>.txt, prints 1 line.
"""
import math
import os
import unreal

AL = unreal.AnimationLibrary
CLIP = os.environ.get("PIVOT_CLIP", "/Game/GSTN/Animation/Mixamo/AS_GSTN_RunPivot_WalkTurn180")
WARP = os.environ.get("PIVOT_WARP", "stock")
YAWRATE = float(os.environ.get("PIVOT_YAWRATE", "120"))
STEER_FALL = float(os.environ.get("PIVOT_STEER_FALL", "0.2"))
SAVE = os.environ.get("GSTN_SAVE") == "1"
seq = unreal.load_asset(CLIP)
name = seq.get_name()
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Pivot", "clip_setup_%s.txt" % name)
log = []
n = AL.get_num_frames(seq) + 1
L = seq.get_play_length()
times = [L * i / (n - 1) for i in range(n)]
root = [AL.get_bone_pose_for_frame(seq, "root", i, False) for i in range(n)]
xy = [(r.translation.x, r.translation.y) for r in root]
yaw = [r.rotation.rotator().yaw for r in root]
# unwrap yaw
for i in range(1, n):
    while yaw[i] - yaw[i - 1] > 180:
        yaw[i] -= 360
    while yaw[i] - yaw[i - 1] < -180:
        yaw[i] += 360


def rate(vals, i):
    a, b = max(i - 1, 0), min(i + 1, n - 1)
    return (vals[b] - vals[a]) / max(times[b] - times[a], 1e-6)


speed = []
for i in range(n):
    a, b = max(i - 1, 0), min(i + 1, n - 1)
    d = math.hypot(xy[b][0] - xy[a][0], xy[b][1] - xy[a][1])
    speed.append(d / max(times[b] - times[a], 1e-6))
yr = [abs(rate(yaw, i)) for i in range(n)]
total = yaw[-1] - yaw[0]
# turn end = first frame where 98% of the total yaw is reached
t_end = next((times[i] for i in range(n) if abs(yaw[i] - yaw[0]) >= 0.98 * abs(total)), L)
if WARP == "one":
    warp = [1.0] * n
else:
    raw = [0.0 if v > YAWRATE else 1.0 for v in yr]
    warp = [sum(raw[max(0, i - 2):i + 3]) / len(raw[max(0, i - 2):i + 3]) for i in range(n)]  # 5-frame box = short ramps
# seconds left until STEER_FALL after the turn end, clamped to 1 (Assistant 2026-11-20 reading; stock L 180 is 0.46 at its turn end)
steer = [max(0.0, min(1.0, t_end + STEER_FALL - t)) for t in times]
curves = {"movedata_speed": speed, "enable_warping": warp, "steeringtargettime": steer}
log.append("%s frames %d len %.3f total yaw %.1f turn end %.3f s, speed max %.0f mean(turn) %.0f, warp mode %s" % (
    name, n, L, total, t_end, max(speed), sum(s for s, t in zip(speed, times) if t <= t_end) / max(1, sum(1 for t in times if t <= t_end)), WARP))
for k, v in curves.items():
    log.append("  %-20s %s" % (k, " ".join("%.2f" % v[int(j * (n - 1) / 10)] for j in range(11))))
if SAVE:
    for k, v in curves.items():
        if AL.does_curve_exist(seq, k, unreal.RawCurveTrackTypes.RCT_FLOAT):
            AL.remove_curve(seq, k, False)
        AL.add_curve(seq, k, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
        AL.add_float_curve_keys(seq, k, times, v)
    seq.set_editor_property("enable_root_motion", True)
    seq.set_editor_property("force_root_lock", True)
    seq.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)
    log.append("saved %s" % unreal.EditorAssetLibrary.save_loaded_asset(seq, False))
log.append("ESITO: OK save=%s" % SAVE)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(log) + "\n")
print(log[0][:200])
