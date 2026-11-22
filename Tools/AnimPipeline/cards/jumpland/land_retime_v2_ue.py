"""Land v2 (land-v2-1): faster recovery after the land by RETIMING the stock clip, no new pose. Runs in the GUI editor (boot
land_v2_boot_vd2.py under Tools/CI/Run-Timed.ps1 -Preset Capture); headless UnrealEditor-Cmd is not used.

Owner (2026-11-18 / brief jumpland-identity.md trait 2): the land is a near-hard cut with impact and weight, shorter than the stock
mocap (stock settles ~f80 at 30 fps, recovery > 1.7 s in game). What this does:
  1. AS_GSTN_LandStand_Retime_v2 = M_Relaxed_Jump_F_Land_Stand_Heavy_Lfoot with a time warp (every bone track, curves resampled):
     src frames [0, A] at 1x (contact f29, pelvis min pm, A = pm + 6: the impact frames are the stock ones, untouched), then the
     recovery [A, S] (S = settle, f80) played at up to K x with eased speed ramps, then TAIL s at 1x and the rest trimmed: GASP's
     "almost complete" exit (time remaining <= 0.75 s) then fires right at the settle pose, so the state machine hands over to the
     idle loop ~1 s earlier. Root motion and the pelvis drop are the stock values (checked below, rule 10: <= 15 % change).
  2. Chooser: sibling row of the stock land row in CHT_GSTN_StockMover_PoseMatch / Stand Mode Transitions (stops_rows_ue.py,
     RS_ROWS=new: same cells, pose-matched, stock row disabled), BlendTime 0.2 -> BLEND (near-hard cut, divergence #8).
Env: LANDV2_K (2.5), LANDV2_TAIL (0.8 s), LANDV2_BLEND (0.05 s). Saves. Report Saved/AnimPipeline/jumpland/landv2_build.json; prints <= 5 lines.
"""
import json
import math
import os
import re
import runpy

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
SRC = "/Game/Characters/UEFN_Mannequin/Animations/Jump/M_Relaxed_Jump_F_Land_Stand_Heavy_Lfoot"
DST = "/Game/GSTN/Locomotion/StockBase/JumpLand/AS_GSTN_LandStand_Retime_v2"
K = float(os.environ.get("LANDV2_K", 2.5))
TAIL = float(os.environ.get("LANDV2_TAIL", 0.8))
BLEND = float(os.environ.get("LANDV2_BLEND", 0.05))
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "AnimPipeline", "jumpland")
rep = {}


def nkeys(a):
    return int(a.get_editor_property("number_of_sampled_keys"))


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
    n = math.sqrt(sum(x * x for x in r)) or 1.0
    return [x / n for x in r]


def warp(n_src, a, s, tail_f):
    """src frame (float) for every new frame: 1x to a, eased ramp to K, K, eased ramp back to 1x landing on s, 1x for tail_f."""
    out, f = [], 0.0
    while f < a:
        out.append(f); f += 1.0
    up = [1.0 + (K - 1.0) * (0.5 - 0.5 * math.cos(math.pi * (i + 1) / 5)) for i in range(5)]
    down = list(reversed(up))[1:] + [1.0]
    for v in up:
        out.append(f); f += v
    while f + sum(down) < s:
        out.append(f); f += K
    for v in down:
        out.append(f); f += v
    end = min(n_src - 1.0, s + tail_f)
    while f < end:
        out.append(f); f += 1.0
    out.append(end)
    return out


src = unreal.load_asset(SRC)
n = nkeys(src)
fps = (n - 1) / src.get_play_length()
tracks = [str(x) for x in AL.get_animation_track_names(src)]
pose = {b: [AL.get_bone_pose_for_frame(src, b, f, False) for f in range(n)] for b in tracks}
pz = [t.translation.z for t in pose["pelvis"]]
lo, hi = int(20 * fps / 30), int(60 * fps / 30)
pm = min(range(lo, hi), key=lambda f: pz[f])
A, S = pm + round(6 * fps / 30), round(80 * fps / 30)
W = warp(n, A, S, TAIL * fps)
rep.update(src_frames=n, fps=round(fps, 2), pelvis_min_frame=pm, A=A, S=S, K=K, new_frames=len(W),
           src_len=round(src.get_play_length(), 3), new_len=round((len(W) - 1) / fps, 3))


def sample(b, x):
    i = min(int(x), n - 2); t = x - i
    p, q = pose[b][i], pose[b][i + 1]
    lerp = lambda u, v: unreal.Vector(u.x + (v.x - u.x) * t, u.y + (v.y - u.y) * t, u.z + (v.z - u.z) * t)
    r = slerp([p.rotation.x, p.rotation.y, p.rotation.z, p.rotation.w], [q.rotation.x, q.rotation.y, q.rotation.z, q.rotation.w], t)
    return lerp(p.translation, q.translation), unreal.Quat(*r), lerp(p.scale3d, q.scale3d)


if not EAL.does_asset_exist(DST):
    EAL.duplicate_asset(SRC, DST)
dst = unreal.load_asset(DST)
ctrl = dst.get_editor_property("controller")
ctrl.open_bracket(unreal.Text("land v2 retime"), False)
try:
    ctrl.set_number_of_frames(unreal.FrameNumber(len(W) - 1), False)
except Exception as e:
    rep["set_frames_err"] = str(e)[:200]
    ctrl.resize_number_of_frames(unreal.FrameNumber(len(W) - 1), unreal.FrameNumber(0), unreal.FrameNumber(n - 1), False)
for b in tracks:
    ks = [sample(b, x) for x in W]
    ctrl.set_bone_track_keys(b, [k[0] for k in ks], [k[1] for k in ks], [k[2] for k in ks], False)


def new_time(ts):
    x = ts * fps
    for j in range(len(W) - 1):
        if W[j] <= x <= W[j + 1]:
            return (j + (x - W[j]) / max(W[j + 1] - W[j], 1e-6)) / fps
    return None


curves = 0
for c in AL.get_animation_curve_names(src, unreal.RawCurveTrackTypes.RCT_FLOAT):
    c = str(c)
    times, vals = AL.get_float_keys(src, c)
    nt, nv = [], []
    for t, v in zip(times, vals):
        u = new_time(t)
        if u is not None and (not nt or u > nt[-1] + 1e-4):
            nt.append(u); nv.append(v)
    if AL.does_curve_exist(dst, c, unreal.RawCurveTrackTypes.RCT_FLOAT):
        AL.remove_curve(dst, c, False)
    AL.add_curve(dst, c, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
    if nt:
        AL.add_float_curve_keys(dst, c, nt, nv)
    curves += 1
ctrl.close_bracket(False)
rep["curves_resampled"] = curves
try:
    rep["notifies"] = [[str(ev.get_editor_property("notify_name")), round(float(ev.get_editor_property("trigger_time_offset")), 3)]
                       for ev in AL.get_animation_notify_events(dst)][:20]
except Exception as e:
    rep["notifies"] = "unread: " + str(e)[:120]

# rule 10 checks: impact amplitude (pelvis drop from the entry frame to the min) and root motion, src vs new
n2 = nkeys(dst)
pz2 = [AL.get_bone_pose_for_frame(dst, "pelvis", f, False).translation.z for f in range(n2)]
ent = round(31 * fps / 30)
rep["impact_drop_cm"] = [round(pz[ent] - pz[pm], 2), round(pz2[ent] - min(pz2[lo:hi]), 2)]
r0, r1 = pose["root"][0].translation, pose["root"][n - 1].translation
q1 = AL.get_bone_pose_for_frame(dst, "root", n2 - 1, False).translation
rep["root_motion_cm"] = [round(math.dist((r0.x, r0.y, r0.z), (r1.x, r1.y, r1.z)), 2), round(math.dist((r0.x, r0.y, r0.z), (q1.x, q1.y, q1.z)), 2)]
rep["dst_frames"] = n2
EAL.save_loaded_asset(dst, False)

# chooser rows: sibling row (stock way), stock row disabled, then BlendTime of our row -> BLEND
os.environ.update(RS_ROWS="new", RS_TAG="landv2_new", GSTN_SAVE="1", RS_PAIRS=SRC + "=" + DST)
runpy.run_path(PROJ + "Tools/AnimPipeline/cards/runstop/stops_rows_ue.py", run_name="__main__")
TS = unreal.GSTN_ChooserTableToolset.get_default_object()
root = unreal.load_asset("/Game/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch")


def tables(c, seen):
    if c and c not in seen:
        seen.append(c)
        for x in TS.call_method("GetChooserTable", (c,)).nested_choosers:
            tables(x, seen)
    return seen


for t in tables(root, []):
    for i, r in enumerate(TS.call_method("GetChooserTable", (t,)).rows):
        if r.asset and r.asset.get_path_name().split(".")[0] == DST:
            for ci, cell in enumerate(r.cells):
                s = str(cell)
                if "BlendTime_" in s:
                    new = re.sub(r"(BlendTime_[0-9A-Za-z_]+=)[0-9.]+", lambda m: m.group(1) + "%.6f" % BLEND, s)
                    TS.call_method("SetChooserCell", (t, ci, i, new))
                    after = str(TS.call_method("GetChooserTable", (t,)).rows[i].cells[ci])
                    rep["row"] = [t.get_name(), i, re.search(r"BlendTime_[^,]+", after).group(0)[-8:],
                                  bool(r.get_editor_property("bDisabled"))]
EAL.save_loaded_asset(root, False)
os.makedirs(OUT, exist_ok=True)
json.dump(rep, open(os.path.join(OUT, "landv2_build.json"), "w"), indent=1)
print("landv2 build: frames %d -> %d, pm %d A %d S %d, drop %s, root %s" % (n, n2, pm, A, S, rep["impact_drop_cm"], rep["root_motion_cm"]))
print("landv2 row: %s" % rep.get("row", "NOT FOUND"))
