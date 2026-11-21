"""Bake the left-side copy of a pivot clip through the mannequin's Mirror Data Table (wc8ksn).

Why baked and not a MirroredOnly chooser row: stock GASP does not mirror its pivots (Turn_L_180 and Turn_R_180 are two unmirrored
assets) and runtime mirroring needs the mirrored flag wired into the ABP Blend Stack plus a mirror table on the PSD schema, which
we cannot verify headless (Unreal Assistant 2026-11-20, docs/notes/assistant/2026-11-21_pivot_overlord.md). A baked copy behaves
exactly like the stock pair. The mirror itself is the engine's convention, read from MDT_UEFN_Mannequin: its bone pairs (Name ->
MirroredName) and Mirror Axis. Per frame, in component space: position of bone B = reflect(position of its partner B');
rotation of B = reflect(rot B') * reflect(ref rot B')^-1 * ref rot B (the delta from the reference pose is mirrored, so the
different local axis conventions of left and right bones cancel). Locals are then recovered parent-first.
Curves *_l / *_r and the foley notify classes Run_L / Run_R swap sides; other curves and notifies are copied as they are.
Env MIRROR_SRC, MIRROR_DST, MIRROR_MDT. Saves. Report Saved/Pivot/mirror_<dst>.json, prints <= 2 lines.
"""
import json
import math
import os

import unreal

AL, EAL, APE = unreal.AnimationLibrary, unreal.EditorAssetLibrary, unreal.AnimPoseExtensions
SRC = os.environ.get("MIRROR_SRC", "/Game/GSTN/Animation/Pivot/AS_GSTN_RunPivot_Overlord_R")
DST = os.environ.get("MIRROR_DST", "/Game/GSTN/Animation/Pivot/AS_GSTN_RunPivot_Overlord_L")
MDT = os.environ.get("MIRROR_MDT", "/Game/Characters/UEFN_Mannequin/Rigs/MDT_UEFN_Mannequin")
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Pivot")
rep = {"src": SRC, "dst": DST, "mdt": MDT}

mdt = unreal.load_asset(MDT)
axis = str(mdt.get_editor_property("mirror_axis"))
rep["axis"] = axis
names = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(mdt, "Name")
mirr = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(mdt, "MirroredName")
kinds = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(mdt, "MirrorEntryType")
pair = {}
for a, b, k in zip(names, mirr, kinds):
    if "Bone" in k or k in ("0", "Bone"):
        pair[a] = b
        pair.setdefault(b, a)
rep["mdt_bone_pairs"] = len(pair)
# the enum prints as "<AxisType.X: 1>": match ".X", not the string end (r1 bug: fell through to Z and flipped the body upside down)
ax = 0 if ".X" in axis else 1 if ".Y" in axis else 2 if ".Z" in axis else -1
if ax < 0:
    raise RuntimeError("unknown mirror axis " + axis)


def rpos(v):
    p = [v.x, v.y, v.z]
    p[ax] = -p[ax]
    return p


def rquat(q):
    # reflection across the plane normal to `ax`: the axial vector part keeps its `ax` component, the others flip
    v = [q[0], q[1], q[2]]
    for i in range(3):
        if i != ax:
            v[i] = -v[i]
    return (v[0], v[1], v[2], q[3])


def qmul(a, b):
    ax_, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax_ * bw + ay * bz - az * by, aw * by - ax_ * bz + ay * bw + az * bx,
            aw * bz + ax_ * by - ay * bx + az * bw, aw * bw - ax_ * bx - ay * by - az * bz)


def qinv(a):
    return (-a[0], -a[1], -a[2], a[3])


def qt(t):
    r = t.rotation
    return (r.x, r.y, r.z, r.w)


src = unreal.load_asset(SRC)
n = int(src.get_editor_property("number_of_sampled_keys"))
tracks = [str(x) for x in AL.get_animation_track_names(src)]
opt = unreal.AnimPoseEvaluationOptions()
try:
    opt.set_editor_property("evaluation_type", unreal.AnimDataEvalType.RAW)
except Exception:
    pass
W = unreal.AnimPoseSpaces.WORLD
L = unreal.AnimPoseSpaces.LOCAL
p0 = APE.get_anim_pose_at_frame(src, 0, opt)
bones = [str(b) for b in APE.get_bone_names(p0)]
ref = {b: APE.get_ref_bone_pose(p0, b, W) for b in bones}
partner = {}
for b in bones:
    m = pair.get(b)
    if not m:  # MDT rows only list the renamed pairs: fall back to the _l/_r suffix rule (same as its find-and-replace)
        m = b[:-2] + "_r" if b.endswith("_l") else b[:-2] + "_l" if b.endswith("_r") else b
    partner[b] = m if m in ref else b
rep["unpaired_sided"] = [b for b in bones if b.endswith(("_l", "_r")) and partner[b] == b][:10]

keys = {b: ([], [], []) for b in tracks}
for f in range(n):
    pose = APE.get_anim_pose_at_frame(src, f, opt)
    cs = {b: APE.get_bone_pose(pose, b, W) for b in bones}
    out = APE.get_anim_pose_at_frame(src, f, opt)
    for b in bones:  # get_bone_names is in skeleton order: parents before children
        m = partner[b]
        q = qmul(qmul(rquat(qt(cs[m])), qinv(rquat(qt(ref[m])))), qt(ref[b]))
        t = unreal.Transform()
        p = rpos(cs[m].translation)
        t.translation = unreal.Vector(*p)
        t.rotation = unreal.Quat(*q)
        t.scale3d = cs[m].scale3d
        r = APE.set_bone_pose(out, t, b, W)  # the pose is a struct passed by ref: Python gets the changed copy back
        if r is not None:
            out = r
    for b in tracks:
        lt = APE.get_bone_pose(out, b, L)
        keys[b][0].append(lt.translation)
        keys[b][1].append(lt.rotation)
        keys[b][2].append(lt.scale3d)

if EAL.does_asset_exist(DST):
    EAL.delete_asset(DST)
EAL.duplicate_asset(SRC, DST)
dst = unreal.load_asset(DST)
ctrl = dst.get_editor_property("controller")
ctrl.open_bracket(unreal.Text("mirror bake"), False)
for b in tracks:
    ctrl.set_bone_track_keys(b, keys[b][0], keys[b][1], keys[b][2], False)


def side_swap(s):
    for a, b in (("_l", "_r"), ("_L", "_R")):
        if s.endswith(a):
            return s[:-2] + b
        if s.endswith(b):
            return s[:-2] + a
    return s


swapped = []
cnames = [str(c) for c in AL.get_animation_curve_names(src, unreal.RawCurveTrackTypes.RCT_FLOAT)]
data = {c: AL.get_float_keys(src, c) for c in cnames}
for c in cnames:
    tgt = side_swap(c)
    if tgt != c and tgt in data:
        if AL.does_curve_exist(dst, tgt, unreal.RawCurveTrackTypes.RCT_FLOAT):
            AL.remove_curve(dst, tgt, False)
        AL.add_curve(dst, tgt, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
        AL.add_float_curve_keys(dst, tgt, list(data[c][0]), list(data[c][1]))
        swapped.append(c + "->" + tgt)
ctrl.close_bracket(False)
rep["curves_swapped"] = swapped

nsw = []
for tn in [str(x) for x in AL.get_animation_notify_track_names(dst)]:
    for ev in AL.get_animation_notify_events_for_track(dst, tn):
        nfy = ev.get_editor_property("notify")
        if not nfy:
            continue
        cp = nfy.get_class().get_path_name()
        alt = cp.replace("Run_L", "#").replace("Run_R", "Run_L").replace("#", "Run_R")
        if alt != cp:
            nsw.append((tn, float(AL.get_anim_notify_event_trigger_time(ev)), alt))
if nsw:
    # rebuild only the foley track(s): remove and re-add with swapped classes, keep other events of those tracks
    for tn in set(x[0] for x in nsw):
        keep = []
        for e in AL.get_animation_notify_events_for_track(dst, tn):
            nsc, nfy = e.get_editor_property("notify_state_class"), e.get_editor_property("notify")
            t = float(AL.get_anim_notify_event_trigger_time(e))
            if nsc:
                keep.append(("state", t, float(AL.get_anim_notify_event_duration(e)), nsc.get_class()))
            elif nfy:
                cp = nfy.get_class().get_path_name()
                alt = cp.replace("Run_L", "#").replace("Run_R", "Run_L").replace("#", "Run_R")
                cls = unreal.load_object(None, alt) or nfy.get_class()
                keep.append(("event", t, 0.0, cls))
        AL.remove_animation_notify_events_by_track(dst, tn)
        for kind, t, d, cls in keep:
            if kind == "state":
                AL.add_animation_notify_state_event(dst, tn, t, d, cls)
            else:
                AL.add_animation_notify_event(dst, tn, t, cls)
rep["notifies_swapped"] = len(nsw)

# checks: root path mirrored (x flips, y same), yaw negated, left foot of dst == mirrored right foot of src at mid clip
m = int(dst.get_editor_property("number_of_sampled_keys"))
mid = m // 2
rs, rd = AL.get_bone_pose_for_frame(src, "root", m - 1, False), AL.get_bone_pose_for_frame(dst, "root", m - 1, False)
rep["root_end_src_dst"] = [[round(rs.translation.x, 1), round(rs.translation.y, 1), round(rs.rotation.rotator().yaw, 1)],
                           [round(rd.translation.x, 1), round(rd.translation.y, 1), round(rd.rotation.rotator().yaw, 1)]]
ps_, pd_ = APE.get_anim_pose_at_frame(src, mid, opt), APE.get_anim_pose_at_frame(dst, mid, opt)
fr, fl = APE.get_bone_pose(ps_, "foot_r", W).translation, APE.get_bone_pose(pd_, "foot_l", W).translation
e = rpos(fr)
rep["foot_mirror_err_cm"] = round(math.dist(e, (fl.x, fl.y, fl.z)), 2)
# independent of rpos: heights must match, the body must stay upright, the lateral coordinate must change sign
hs, hd = APE.get_bone_pose(ps_, "head", W).translation, APE.get_bone_pose(pd_, "head", W).translation
rep["upright_check"] = {"head_z": [round(hs.z, 1), round(hd.z, 1)], "foot_z": [round(fr.z, 1), round(fl.z, 1)],
                        "foot_lateral": [round([fr.x, fr.y, fr.z][ax], 1), round([fl.x, fl.y, fl.z][ax], 1)]}
if abs(hs.z - hd.z) > 1.0 or hd.z < 50:
    raise RuntimeError("mirror check failed: %s" % rep["upright_check"])
rep["saved"] = bool(EAL.save_loaded_asset(dst, False))
os.makedirs(OUT, exist_ok=True)
json.dump(rep, open(os.path.join(OUT, "mirror_%s.json" % DST.split("/")[-1]), "w"), indent=1)
print("mirror bake %s: axis %s, pairs %d, foot err %.2f cm, root end %s, saved %s" % (
    DST.split("/")[-1], axis, len(pair), rep["foot_mirror_err_cm"], rep["root_end_src_dst"], rep["saved"]))
