"""Turnaround 180L (variant 28, turnaround-180l-c1): parametric UE headless pass on AS_GSTN_RunStart_Generic_w2t180Ls.
Implements 4 owner requirements over f0-f27 (30 fps):
  1. Single continuous arc f0-f27 (smooth pelvis/spine/arm continuity across hand-off f6-f8).
  2. High/open shoulders (clavicle elevation + retraction, upperarm outward abduction).
  3. Left arm clearly behind torso coronal plane during pivot/launch (f2-f12) with natural elbow flex.
  4. Torso banking (inward roll toward L) + dynamic lean already in the pivot (f2-f8), blended into run.
Idempotent via Saved/AnimPipeline/runstart/w2t180Ls_base_tracks.json. Dry run unless GSTN_SAVE=1."""
import json
import math
import os

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ASSET = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t180Ls"
SAVE = os.environ.get("GSTN_SAVE") == "1"
BASE_JSON = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart", "w2t180Ls_base_tracks.json")
OUT_JSON = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart", "AS_GSTN_RunStart_Generic_w2t180Ls_build.json")

CLAV_ELEV_DEG = float(os.environ.get("T180L_CLAV_ELEV", "7.0"))
CLAV_OPEN_DEG = float(os.environ.get("T180L_CLAV_OPEN", "5.0"))
ARM_OUT_DEG = float(os.environ.get("T180L_ARM_OUT", "10.0"))
ARM_L_BACK_DEG = float(os.environ.get("T180L_ARM_L_BACK", "26.0"))
ARM_L_ELBOW_DEG = float(os.environ.get("T180L_ARM_L_ELBOW", "14.0"))
TORSO_BANK_DEG = float(os.environ.get("T180L_TORSO_BANK", "11.0"))
TORSO_LEAN_DEG = float(os.environ.get("T180L_TORSO_LEAN", "5.0"))
HANDOFF_SMOOTH_PASSES = int(os.environ.get("T180L_SMOOTH_PASSES", "3"))

TRACKED_BONES = [
    "pelvis", "spine_01", "spine_02", "spine_03",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
]


def ss(a, b, x):
    if x <= a:
        return 0.0
    if x >= b:
        return 1.0
    t = (x - a) / float(b - a)
    return t * t * (3.0 - 2.0 * t)


def vsub(a, b):
    return unreal.Vector(a.x - b.x, a.y - b.y, a.z - b.z)


def vadd(a, b):
    return unreal.Vector(a.x + b.x, a.y + b.y, a.z + b.z)


def vmul(a, s):
    return unreal.Vector(a.x * s, a.y * s, a.z * s)


def vlen(a):
    return math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z)


def vnorm(a):
    l = vlen(a)
    return unreal.Vector(a.x / l, a.y / l, a.z / l) if l > 1e-6 else unreal.Vector(0.0, 1.0, 0.0)


def vdot(a, b):
    return a.x * b.x + a.y * b.y + a.z * b.z


def vcross(a, b):
    return unreal.Vector(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    )


def qinv(q):
    return unreal.Quat(-q.x, -q.y, -q.z, q.w)


def qnlerp(a, b, t):
    dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w
    s = -1.0 if dot < 0.0 else 1.0
    x = a.x * (1.0 - t) + s * b.x * t
    y = a.y * (1.0 - t) + s * b.y * t
    z = a.z * (1.0 - t) + s * b.z * t
    w = a.w * (1.0 - t) + s * b.w * t
    l = math.sqrt(x * x + y * y + z * z + w * w)
    return unreal.Quat(x / l, y / l, z / l, w / l)


def axis_quat(ax, deg):
    if abs(deg) < 1e-5:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    ax = vnorm(ax)
    r = math.radians(deg) * 0.5
    s, c = math.sin(r), math.cos(r)
    return unreal.Quat(ax.x * s, ax.y * s, ax.z * s, c)


def signed_axis_quat(ax, deg, ref_vec, target_dir):
    if abs(deg) < 1e-5:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    qp = axis_quat(ax, deg)
    qn = qinv(qp)
    dp = vdot(vsub(qp.rotate_vector(ref_vec), ref_vec), target_dir)
    dn = vdot(vsub(qn.rotate_vector(ref_vec), ref_vec), target_dir)
    return qp if dp >= dn else qn


def mk_tf(pos, quat, scl):
    return unreal.Transform(location=pos, rotation=quat.rotator(), scale=scl)


def serialize_tracks(seq, n):
    data = {"frames": n, "bones": {}}
    for b in TRACKED_BONES:
        tfs = [AL.get_bone_pose_for_frame(seq, b, f, False) for f in range(n)]
        data["bones"][b] = {
            "pos": [[t.translation.x, t.translation.y, t.translation.z] for t in tfs],
            "rot": [[t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w] for t in tfs],
            "scl": [[t.scale3d.x, t.scale3d.y, t.scale3d.z] for t in tfs],
        }
    return data


def apply_serialized_tracks(seq, data):
    ctrl = seq.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("restore base tracks"), False)
    for b, tr in data["bones"].items():
        pos = [unreal.Vector(*p) for p in tr["pos"]]
        rot = [unreal.Quat(*r) for r in tr["rot"]]
        scl = [unreal.Vector(*s) for s in tr["scl"]]
        ctrl.set_bone_track_keys(b, pos, rot, scl, False)
    ctrl.close_bracket(False)


seq = unreal.load_asset(ASSET)
if not seq:
    raise RuntimeError("Missing asset: " + ASSET)
n = int(seq.get_editor_property("number_of_sampled_frames")) if hasattr(seq, "number_of_sampled_frames") else AL.get_num_frames(seq) + 1

os.makedirs(os.path.dirname(BASE_JSON), exist_ok=True)
if not os.path.exists(BASE_JSON):
    base_data = serialize_tracks(seq, n)
    json.dump(base_data, open(BASE_JSON, "w"))
else:
    base_data = json.load(open(BASE_JSON))
    apply_serialized_tracks(seq, base_data)

opts = unreal.AnimPoseEvaluationOptions()
all_bones = TRACKED_BONES + ["spine_04", "spine_05", "thigh_l", "thigh_r", "foot_l", "ball_l", "foot_r", "ball_r"]

world_frames = []
for f in range(n):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0, opts)
    w = {b: unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD) for b in all_bones}
    world_frames.append(w)

# Smooth pelvis & spine world continuity across hand-off (f2..f14) before applying pose shaping
# First compute smoothed local rotations/positions on base tracks for hand-off continuity
ctrl = seq.get_editor_property("controller")
tracks_out = {b: ([], [], []) for b in TRACKED_BONES}

for f in range(n):
    w = world_frames[f]
    nw = {b: w[b] for b in all_bones}

    up_vec = unreal.Vector(0.0, 0.0, 1.0)
    lat_raw = vsub(w["thigh_l"].translation, w["thigh_r"].translation)
    lat_raw.z = 0.0
    lat_vec = vnorm(lat_raw)

    foot_dir = vadd(
        vsub(w["ball_l"].translation, w["foot_l"].translation),
        vsub(w["ball_r"].translation, w["foot_r"].translation),
    )
    foot_dir.z = 0.0
    fwd_cand = vnorm(vcross(lat_vec, up_vec))
    fwd_vec = fwd_cand if vdot(fwd_cand, foot_dir) >= 0.0 else vmul(fwd_cand, -1.0)

    # Envelopes over f0..f27
    e_sh = ss(1.0, 4.0, float(f)) * (1.0 - ss(18.0, 26.0, float(f)))
    e_bank = ss(1.0, 4.0, float(f)) * (1.0 - ss(14.0, 23.0, float(f)))
    e_arm_l_back = ss(1.5, 4.5, float(f)) * (1.0 - ss(9.0, 14.0, float(f)))

    # 1. Torso banking (roll inward toward L) + dynamic lean already in pivot (f2-f8 -> f23)
    q_sp_cum = unreal.Quat(0.0, 0.0, 0.0, 1.0)
    sp_weights = (("spine_01", 0.40), ("spine_02", 0.35), ("spine_03", 0.25))
    sp_par = {"spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02"}
    for sp, wt in sp_weights:
        par = sp_par[sp]
        pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
        q_bank = signed_axis_quat(fwd_vec, TORSO_BANK_DEG * wt * e_bank, up_vec, lat_vec)
        q_lean = signed_axis_quat(lat_vec, TORSO_LEAN_DEG * wt * e_bank, up_vec, fwd_vec)
        q_sp_step = q_lean * q_bank
        q_sp_cum = q_sp_step * q_sp_cum
        nw[sp] = mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

    for sp, par in (("spine_04", "spine_03"), ("spine_05", "spine_04")):
        pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
        nw[sp] = mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

    # 2. Shoulders high/open + Left arm behind torso plane
    for s, sign in (("l", 1.0), ("r", -1.0)):
        cl, ua, la, ha = "clavicle_" + s, "upperarm_" + s, "lowerarm_" + s, "hand_" + s
        out_dir = vmul(lat_vec, sign)

        cl_pos = vadd(nw["spine_05"].translation, q_sp_cum.rotate_vector(vsub(w[cl].translation, w["spine_05"].translation)))
        ua_dir0 = q_sp_cum.rotate_vector(vsub(w[ua].translation, w[cl].translation))
        q_cl_elev = signed_axis_quat(fwd_vec, CLAV_ELEV_DEG * e_sh, ua_dir0, up_vec)
        q_cl_open = signed_axis_quat(up_vec, CLAV_OPEN_DEG * e_sh, ua_dir0, vmul(fwd_vec, -1.0))
        q_cl = q_cl_open * q_cl_elev
        nw[cl] = mk_tf(cl_pos, q_cl * q_sp_cum * w[cl].rotation, w[cl].scale3d)

        ua_pos = vadd(cl_pos, q_cl.rotate_vector(ua_dir0))
        la_dir0 = (q_cl * q_sp_cum).rotate_vector(vsub(w[la].translation, w[ua].translation))
        q_ua_out = signed_axis_quat(fwd_vec, ARM_OUT_DEG * e_sh, la_dir0, out_dir)
        q_ua_back = unreal.Quat(0.0, 0.0, 0.0, 1.0)
        if s == "l" and e_arm_l_back > 1e-4:
            q_ua_back = signed_axis_quat(lat_vec, ARM_L_BACK_DEG * e_arm_l_back, la_dir0, vmul(fwd_vec, -1.0))
        q_ua = q_ua_back * q_ua_out
        nw[ua] = mk_tf(ua_pos, q_ua * q_cl * q_sp_cum * w[ua].rotation, w[ua].scale3d)

        la_pos = vadd(ua_pos, q_ua.rotate_vector(la_dir0))
        ha_dir0 = (q_ua * q_cl * q_sp_cum).rotate_vector(vsub(w[ha].translation, w[la].translation))
        q_la = unreal.Quat(0.0, 0.0, 0.0, 1.0)
        if s == "l" and e_arm_l_back > 1e-4:
            hinge = vcross(la_dir0, ha_dir0)
            ax = vnorm(hinge) if vlen(hinge) > 1e-3 else lat_vec
            qp = axis_quat(ax, ARM_L_ELBOW_DEG * e_arm_l_back)
            qn = qinv(qp)
            q_la = qp if vdot(vmul(la_dir0, -1.0), qp.rotate_vector(ha_dir0)) >= vdot(vmul(la_dir0, -1.0), qn.rotate_vector(ha_dir0)) else qn
        nw[la] = mk_tf(la_pos, q_la * q_ua * q_cl * q_sp_cum * w[la].rotation, w[la].scale3d)

        ha_pos = vadd(la_pos, q_la.rotate_vector(ha_dir0))
        nw[ha] = mk_tf(ha_pos, q_la * q_ua * q_cl * q_sp_cum * w[ha].rotation, w[ha].scale3d)

    parents = {
        "pelvis": "root",
        "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02",
        "clavicle_l": "spine_05", "upperarm_l": "clavicle_l", "lowerarm_l": "upperarm_l", "hand_l": "lowerarm_l",
        "clavicle_r": "spine_05", "upperarm_r": "clavicle_r", "lowerarm_r": "upperarm_r", "hand_r": "lowerarm_r",
    }
    for b in TRACKED_BONES:
        if b == "pelvis":
            loc = AL.get_bone_pose_for_frame(seq, b, f, False)
        else:
            loc = nw[b].make_relative(nw[parents[b]])
        tracks_out[b][0].append(loc.translation)
        tracks_out[b][1].append(loc.rotation)
        tracks_out[b][2].append(loc.scale3d)

# Continuous single-arc smoothing across f1..f24 (strongest around hand-off f4..f11, 0 at f0 and f26+)
for b in TRACKED_BONES:
    pos, rot, scl = tracks_out[b]
    for _ in range(HANDOFF_SMOOTH_PASSES):
        nr = list(rot)
        np_ = list(pos)
        for f in range(1, min(26, n - 1)):
            w_arc = 0.45 * ss(0.0, 3.0, float(f)) * (1.0 - ss(16.0, 26.0, float(f)))
            nr[f] = qnlerp(rot[f], qnlerp(rot[f - 1], rot[f + 1], 0.5), w_arc)
            if b == "pelvis":
                mid_p = vmul(vadd(pos[f - 1], pos[f + 1]), 0.5)
                np_[f] = vadd(vmul(pos[f], 1.0 - w_arc), vmul(mid_p, w_arc))
        rot = nr
        pos = np_
    tracks_out[b] = (pos, rot, scl)

saved = False
if SAVE:
    ctrl.open_bracket(unreal.Text("turnaround 180L continuous arc"), False)
    for b, (pos, rot, scl) in tracks_out.items():
        ctrl.set_bone_track_keys(b, pos, rot, scl, False)
    ctrl.close_bracket(False)
    saved = bool(EAL.save_asset(ASSET, only_if_is_dirty=False))

rep = {
    "asset": ASSET,
    "frames": n,
    "saved": saved,
    "params": {
        "clav_elev_deg": CLAV_ELEV_DEG,
        "clav_open_deg": CLAV_OPEN_DEG,
        "arm_out_deg": ARM_OUT_DEG,
        "arm_l_back_deg": ARM_L_BACK_DEG,
        "arm_l_elbow_deg": ARM_L_ELBOW_DEG,
        "torso_bank_deg": TORSO_BANK_DEG,
        "torso_lean_deg": TORSO_LEAN_DEG,
        "handoff_smooth_passes": HANDOFF_SMOOTH_PASSES,
    },
}
os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
json.dump(rep, open(OUT_JSON, "w"), indent=2)
print("ESITO: OK 180L frames=%d saved=%s" % (n, saved))
