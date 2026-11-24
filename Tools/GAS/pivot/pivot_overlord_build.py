"""Overlord run pivot (wc8ksn): the stock run 180 turn with its turn core retimed to the ~0.3 s Mover reversal (skid-reversal-1).

Why a retime and not a new pose: the brief asks for the Toad timing (~0.26-0.3 s from the stick reversal to running) and a brute
look; the timing is data we can make here, the pose ("Ganondorf inside Astro Bot": heavier brake, lower hips) needs Cascadeur on
the desktop. So this bakes AS_GSTN_RunPivot_Overlord_R = M_Relaxed_Run_Turn_R_180_Lfoot with a smooth time warp:
  core = the frames where the root planar speed is below CORE_FRAC x the run speed (decel + 180 + reaccel, ~0.8 s stock);
  inside the core the playback rate is r(u) = 1 + (Kp - 1) sin^2(pi u), u in [0, 1] over the NEW core, so the rate is 1x at both
  ends (no velocity pop at the seams) and peaks in the middle where the stock root is slowest; Kp is solved so the new core lasts
  PIVOT_CORE_S. Run-in and run-out stay at 1x.
Every bone track is resampled (slerp), the stock float curves are remapped in time (contact_l/r, phase, enable_warping,
steeringtargettime), movedata_speed is recomputed from the NEW root (Get_DynamicPlayRate divides capsule speed by it), notifies
are re-added at remapped times. Root motion + ForceRootLock RefPose come from the duplicate (stock setup, skid-walkturn-3 sec. 2).
The left side is NOT a second clip: it is the same asset on a MirroredOnly chooser row (pivot_overlord_rows.py).
Env PIVOT_SRC, PIVOT_DST, PIVOT_CORE_S (0.30), PIVOT_CORE_FRAC (0.9). Saves. Report Saved/Pivot/overlord_build.json, prints <= 3 lines.
"""
import json
import math
import os

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
SRC = os.environ.get("PIVOT_SRC", "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Turn_R_180_Lfoot")
DST = os.environ.get("PIVOT_DST", "/Game/GSTN/Animation/Pivot/AS_GSTN_RunPivot_Overlord_R")
CORE_S = float(os.environ.get("PIVOT_CORE_S", "0.30"))
FRAC = float(os.environ.get("PIVOT_CORE_FRAC", "0.9"))
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Pivot")
rep = {"src": SRC, "dst": DST, "core_s": CORE_S, "frac": FRAC}


def find_src(path):
    if EAL.does_asset_exist(path):
        return path
    name = path.split("/")[-1]
    for p in EAL.list_assets("/Game/Characters", True, False):
        if p.split(".")[0].endswith("/" + name):
            return p.split(".")[0]
    raise RuntimeError("source not found: " + name)


SRC = find_src(SRC)
rep["src"] = SRC
src = unreal.load_asset(SRC)
n = int(src.get_editor_property("number_of_sampled_keys"))
fps = (n - 1) / src.get_play_length()
tracks = [str(x) for x in AL.get_animation_track_names(src)]
pose = {b: [AL.get_bone_pose_for_frame(src, b, f, False) for f in range(n)] for b in tracks}
rxy = [(t.translation.x, t.translation.y) for t in pose["root"]]


def speeds(xy, f_s):
    m = len(xy)
    out = []
    for i in range(m):
        a, b = max(i - 1, 0), min(i + 1, m - 1)
        out.append(math.hypot(xy[b][0] - xy[a][0], xy[b][1] - xy[a][1]) * f_s / max(b - a, 1))
    return out


sp = speeds(rxy, fps)
vrun = sorted(sp[: max(3, int(0.5 * fps))])[len(sp[: max(3, int(0.5 * fps))]) // 2]
fmin = min(range(n), key=lambda i: sp[i])
A = fmin
while A > 0 and sp[A - 1] < FRAC * vrun:
    A -= 1
S = fmin
while S < n - 1 and sp[S + 1] < FRAC * vrun:
    S += 1
A, S = max(A - 1, 0), min(S + 1, n - 1)
core_new = max(CORE_S * fps, 3.0)
Kp = 2.0 * ((S - A) / core_new - 1.0) + 1.0
rep.update(frames=n, fps=round(fps, 2), vrun=round(vrun, 1), slowest_frame=fmin, slowest_speed=round(sp[fmin], 1),
           core_src=[A, S, round((S - A) / fps, 3)], Kp=round(Kp, 3))
if Kp < 1.0:
    raise RuntimeError("core already shorter than PIVOT_CORE_S")

# new-frame -> source-frame map W
W = [float(f) for f in range(A + 1)]
steps = max(int(round(core_new)), 2)
# integrate the rate over the new core, then scale so the core ends exactly on S
raw = [0.0]
for j in range(steps):
    u = (j + 0.5) / steps
    raw.append(raw[-1] + 1.0 + (Kp - 1.0) * math.sin(math.pi * u) ** 2)
scale = (S - A) / raw[-1]
for j in range(1, steps + 1):
    W.append(A + raw[j] * scale)
W += [float(f) for f in range(S + 1, n)]
rep["new_frames"] = len(W)
rep["core_new"] = [A, A + steps, round(steps / fps, 3)]


def slerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0:
        b, d = [-x for x in b], -d
    if d > 0.9995:
        r = [x + (y - x) * t for x, y in zip(a, b)]
    else:
        th = math.acos(d)
        s0, s1 = math.sin((1 - t) * th) / math.sin(th), math.sin(t * th) / math.sin(th)
        r = [s0 * x + s1 * y for x, y in zip(a, b)]
    m = math.sqrt(sum(x * x for x in r)) or 1.0
    return [x / m for x in r]


def sample(b, x):
    i = min(int(x), n - 2)
    t = x - i
    p, q = pose[b][i], pose[b][i + 1]
    lerp = lambda u, v: unreal.Vector(u.x + (v.x - u.x) * t, u.y + (v.y - u.y) * t, u.z + (v.z - u.z) * t)
    r = slerp([p.rotation.x, p.rotation.y, p.rotation.z, p.rotation.w], [q.rotation.x, q.rotation.y, q.rotation.z, q.rotation.w], t)
    return lerp(p.translation, q.translation), unreal.Quat(*r), lerp(p.scale3d, q.scale3d)


def new_time(ts):
    x = ts * fps
    for j in range(len(W) - 1):
        if W[j] <= x <= W[j + 1]:
            return (j + (x - W[j]) / max(W[j + 1] - W[j], 1e-6)) / fps
    return (len(W) - 1) / fps if x > W[-1] else 0.0


# notifies of the source, read before the duplicate is changed
evs = []
tnames = [str(x) for x in AL.get_animation_notify_track_names(src)]
for trk, tn in enumerate(tnames):
    for ev in AL.get_animation_notify_events_for_track(src, tn):
        t = float(AL.get_anim_notify_event_trigger_time(ev))
        nsc = ev.get_editor_property("notify_state_class")
        nfy = ev.get_editor_property("notify")
        dur = float(AL.get_anim_notify_event_duration(ev)) if nsc else 0.0
        evs.append((t, dur, (nsc or nfy).get_class() if (nsc or nfy) else None, bool(nsc), trk))

if EAL.does_asset_exist(DST):
    EAL.delete_asset(DST)
EAL.duplicate_asset(SRC, DST)
dst = unreal.load_asset(DST)
ctrl = dst.get_editor_property("controller")
ctrl.open_bracket(unreal.Text("overlord pivot retime"), False)
try:
    ctrl.set_number_of_frames(unreal.FrameNumber(len(W) - 1), False)
except Exception as e:
    rep["set_frames_err"] = str(e)[:200]
    ctrl.resize_number_of_frames(unreal.FrameNumber(len(W) - 1), unreal.FrameNumber(0), unreal.FrameNumber(n - 1), False)
for b in tracks:
    ks = [sample(b, x) for x in W]
    ctrl.set_bone_track_keys(b, [k[0] for k in ks], [k[1] for k in ks], [k[2] for k in ks], False)

curves = []
for c in AL.get_animation_curve_names(src, unreal.RawCurveTrackTypes.RCT_FLOAT):
    c = str(c)
    if c == "movedata_speed":
        continue
    times, vals = AL.get_float_keys(src, c)
    nt, nv = [], []
    for t, v in zip(times, vals):
        u = new_time(t)
        if c == "steeringtargettime" and 0.0 < v < 0.999:
            # seconds left to the end of the turn (Assistant 2026-11-20 Q2): the value shrinks with the warp, not only the key time
            v = min(1.0, new_time(t + v) - u)
        if not nt or u > nt[-1] + 1e-4:
            nt.append(u)
            nv.append(v)
    if AL.does_curve_exist(dst, c, unreal.RawCurveTrackTypes.RCT_FLOAT):
        AL.remove_curve(dst, c, False)
    AL.add_curve(dst, c, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
    if nt:
        AL.add_float_curve_keys(dst, c, nt, nv)
    curves.append(c)
ctrl.close_bracket(False)

# movedata_speed from the new root (one key per frame, as the stock baked curve)
m = int(dst.get_editor_property("number_of_sampled_keys"))
nxy = [(lambda t: (t.translation.x, t.translation.y))(AL.get_bone_pose_for_frame(dst, "root", f, False)) for f in range(m)]
nsp = speeds(nxy, fps)
if AL.does_curve_exist(dst, "movedata_speed", unreal.RawCurveTrackTypes.RCT_FLOAT):
    AL.remove_curve(dst, "movedata_speed", False)
AL.add_curve(dst, "movedata_speed", unreal.RawCurveTrackTypes.RCT_FLOAT, False)
AL.add_float_curve_keys(dst, "movedata_speed", [f / fps for f in range(m)], nsp)
curves.append("movedata_speed")
rep["curves"] = curves

# notifies at remapped times
for tn in [str(x) for x in AL.get_animation_notify_track_names(dst)]:
    AL.remove_animation_notify_events_by_track(dst, tn)
for tn in tnames:
    if tn not in [str(x) for x in AL.get_animation_notify_track_names(dst)]:
        AL.add_animation_notify_track(dst, tn)
added = []
for t, dur, cls, is_state, trk in evs:
    if cls is None:
        continue
    tn = tnames[trk] if trk < len(tnames) else tnames[0]
    t0 = new_time(t)
    if is_state:
        t1 = new_time(t + dur)
        AL.add_animation_notify_state_event(dst, tn, t0, max(t1 - t0, 1.0 / fps), cls)
        added.append([cls.get_name(), round(t, 3), round(t0, 3), round(t1 - t0, 3)])
    else:
        AL.add_animation_notify_event(dst, tn, t0, cls)
        added.append([cls.get_name(), round(t, 3), round(t0, 3)])
rep["notifies"] = added

# report: yaw and speed along the new core, root motion src vs new
yaw = [AL.get_bone_pose_for_frame(dst, "root", f, False).rotation.rotator().yaw for f in range(m)]
c0, c1 = A, A + steps
rep["new_core_speed"] = [round(nsp[f]) for f in range(max(c0 - 2, 0), min(c1 + 3, m))]
rep["new_core_yaw"] = [round(yaw[f]) for f in range(max(c0 - 2, 0), min(c1 + 3, m))]
r0, r1 = pose["root"][A].translation, pose["root"][S].translation
rep["core_root_cm"] = round(math.hypot(r1.x - r0.x, r1.y - r0.y), 1)
rep["len"] = [round(src.get_play_length(), 3), round(dst.get_play_length(), 3)]
rep["root_lock"] = [bool(dst.get_editor_property("force_root_lock")), str(dst.get_editor_property("root_motion_root_lock")),
                    bool(dst.get_editor_property("enable_root_motion"))]
rep["saved"] = bool(EAL.save_loaded_asset(dst, False))
os.makedirs(OUT, exist_ok=True)
json.dump(rep, open(os.path.join(OUT, "overlord_build.json"), "w"), indent=1)
print("overlord pivot: core %s -> %s, Kp %.2f, len %s, saved %s" % (rep["core_src"], rep["core_new"], Kp, rep["len"], rep["saved"]))
print("ESITO: OK")
