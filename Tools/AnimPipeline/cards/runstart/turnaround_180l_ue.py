"""Turnaround 180L (variant 28, turnaround-180l-c2): built cleanly from w2t180Lp (27) + 29 (w3) falling-forward physics.
Core principle (owner 2026-11-23):
  - Start from clean w2t180Lp (no stacking on old Cascadeur edits).
  - Derive local character axes (lat, fwd, up) from pelvis/spine (never from feet, which oppose during 180 pivot).
  - Apply 29 (w3) weight-unbalance ("falling forward", legs trailing behind and catching up, knee drop, 2-bone leg IK)
    starting smoothly inside the pivot (f2 -> f27) so pivot and acceleration form ONE continuous human arc.
  - Torso banking (inward roll toward L) + progressive lean already in the pivot, high/open shoulders,
    and L arm extending naturally behind the torso plane (backward along -fwd, NOT flared high sideways like a wing).
Idempotent: always reads clean SRC (w2t180Lp) and writes DST (w2t180Ls). Dry run unless GSTN_SAVE=1."""
import json
import math
import os

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t180Lp"
DST = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t180Ls"
SAVE = os.environ.get("GSTN_SAVE") == "1" or os.environ.get("CLIPFIX_WRITE") == "1"
OUT_JSON = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart", "AS_GSTN_RunStart_Generic_w2t180Ls_build.json")

# 29 (w3) weight-unbalance parameters adapted for continuous 180L pivot -> sprint
PELVIS_FWD_CM = float(os.environ.get("T180L_PELVIS_FWD", "11.0"))     # COM shift along character fwd (+11 cm)
PELVIS_DROP_CM = float(os.environ.get("T180L_PELVIS_DROP", "-5.0"))   # smooth athletic drop (-5 cm already in pivot)
SPINE_LEAN_PIVOT_DEG = float(os.environ.get("T180L_SPINE_LEAN_PIVOT", "14.0")) # lean ALREADY strong at f3-f5 (v88-v92)
SPINE_LEAN_RUN_DEG = float(os.environ.get("T180L_SPINE_LEAN_RUN", "5.5"))      # residual lean settling gradually to f32 (v152)
TORSO_BANK_DEG = float(os.environ.get("T180L_TORSO_BANK", "14.0"))    # clear inward roll toward L already at v88-v92
HEAD_LIFT_DEG = float(os.environ.get("T180L_HEAD_LIFT", "6.5"))       # keep head proud in f4-f22
KNEE_DROP = float(os.environ.get("T180L_KNEE_DROP", "0.28"))          # w3 lower knee lift in swing
SWING_LAG_CM = float(os.environ.get("T180L_SWING_LAG", "3.5"))        # swing foot trails behind COM early
CATCH_FWD_CM = float(os.environ.get("T180L_CATCH_FWD", "4.5"))        # swing foot catches up under COM

# Shoulders & arms (single continuous arc f2-f30, f0-f1 untouched)
CLAV_ELEV_DEG = float(os.environ.get("T180L_CLAV_ELEV", "4.0"))       # subtle high athletic shoulders
CLAV_OPEN_DEG = float(os.environ.get("T180L_CLAV_OPEN", "9.0"))       # open chest/shoulders
ARM_L_TUCK_IN_DEG = float(os.environ.get("T180L_ARM_L_TUCK_IN", "7.0"))  # pull L upperarm in/down at v92-96 while keeping f10 clearance >=14cm
ARM_R_OUT_DEG = float(os.environ.get("T180L_ARM_R_OUT", "6.0"))       # R arm clearance
ARM_L_BACK_DEG = float(os.environ.get("T180L_ARM_L_BACK", "30.0"))    # L arm drives cleanly BACK behind torso plane f2-f10
ARM_L_ELBOW_DEG = float(os.environ.get("T180L_ARM_L_ELBOW", "6.0"))   # open/extended natural sweep (no rigid triangle under armpit)
ARM_R_FWD_DEG = float(os.environ.get("T180L_ARM_R_FWD", "18.0"))      # R arm counterbalances forward/open in pivot


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


def solve_2bone_ik(hip_new, foot_new, hip_old, knee_old, foot_old):
    l1 = vlen(vsub(knee_old, hip_old))
    l2 = vlen(vsub(foot_old, knee_old))
    d_vec = vsub(foot_new, hip_new)
    d = vlen(d_vec)
    d_clamped = max(abs(l1 - l2) + 0.5, min(l1 + l2 - 0.5, d))
    dir_hf = vnorm(d_vec)
    old_hf = vnorm(vsub(foot_old, hip_old))
    off = vsub(knee_old, hip_old)
    pole = vsub(off, vmul(old_hf, vdot(off, old_hf)))
    if vlen(pole) < 1e-3:
        pole = unreal.Vector(0.0, 1.0, 0.0)
    else:
        pole = vsub(pole, vmul(dir_hf, vdot(pole, dir_hf)))
        pole = vnorm(pole) if vlen(pole) > 1e-3 else unreal.Vector(0.0, 1.0, 0.0)
    cos_a = max(-1.0, min(1.0, (l1 * l1 + d_clamped * d_clamped - l2 * l2) / (2.0 * l1 * d_clamped)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    knee_new = vadd(hip_new, vadd(vmul(dir_hf, l1 * cos_a), vmul(pole, l1 * sin_a)))
    foot_out = vadd(hip_new, vmul(dir_hf, d_clamped))
    return knee_new, foot_out


# Duplicate clean SRC -> DST so every run is 100% idempotent
if SAVE:
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    if not EAL.duplicate_asset(SRC, DST):
        raise RuntimeError("Failed to duplicate %s -> %s" % (SRC, DST))
    seq = unreal.load_asset(DST)
else:
    seq = unreal.load_asset(SRC)

if not seq:
    raise RuntimeError("Missing asset")

n = int(seq.get_editor_property("number_of_sampled_frames")) if hasattr(seq, "number_of_sampled_frames") else AL.get_num_frames(seq) + 1
opts = unreal.AnimPoseEvaluationOptions()
all_bones = [
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "head",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    "thigh_l", "calf_l", "foot_l", "ball_l", "ik_foot_root", "ik_foot_l",
    "thigh_r", "calf_r", "foot_r", "ball_r", "ik_foot_r",
]

world_frames = []
for f in range(n):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0, opts)
    world_frames.append({b: unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD) for b in all_bones})

# Pre-compute foot air weights (0 = planted, 1 = in swing)
foot_air_w = {"l": [], "r": []}
for s in ("l", "r"):
    fz = [world_frames[f]["foot_" + s].translation.z for f in range(n)]
    fmin = min(fz[:min(n, 28)])
    for f in range(n):
        foot_air_w[s].append(ss(fmin + 1.5, fmin + 6.0, fz[f]))

# Continuous character forward/lateral axes derived ONLY from pelvis/thighs & root yaw (never from feet)
# In UE mannequin, pelvis local +X/+Y/+Z varies, so anchor fwd_vec from final run heading (f20..f26) rotated by root yaw delta,
# or directly from cross(lat_vec, up_vec) checked against root_fwd_final!
r_p_end = world_frames[min(24, n - 1)]["pelvis"].translation
r_p_start = world_frames[min(14, n - 1)]["pelvis"].translation
run_dir_raw = vsub(r_p_end, r_p_start)
run_dir_raw.z = 0.0
run_dir = vnorm(run_dir_raw)

lat_vecs, fwd_vecs = [], []
for f in range(n):
    w = world_frames[f]
    up_vec = unreal.Vector(0.0, 0.0, 1.0)
    lat_raw = vsub(w["thigh_l"].translation, w["thigh_r"].translation)
    lat_raw.z = 0.0
    lat_vec = vnorm(lat_raw)
    cand = vnorm(vcross(lat_vec, up_vec))
    # Ensure continuous orientation: at f>=10, fwd must align with run_dir; propagate backward smoothly
    lat_vecs.append(lat_vec)
    fwd_vecs.append(cand)

# Anchor sign from late run frames (f20..26) and enforce temporal continuity backward to f0
ref_fwd = run_dir
for f in range(n - 1, -1, -1):
    if vdot(fwd_vecs[f], ref_fwd) < 0.0:
        fwd_vecs[f] = vmul(fwd_vecs[f], -1.0)
    ref_fwd = fwd_vecs[f]

# Smooth pelvis height dip over f0..f26 so there is no flat shelf + sudden +8.7 cm jump at f22-f26
pel_z_raw = [world_frames[f]["pelvis"].translation.z for f in range(n)]
pel_z_sm = list(pel_z_raw)
for _ in range(4):
    nz = list(pel_z_sm)
    for f in range(1, min(27, n - 1)):
        w_sm = 0.55 * ss(0.0, 3.0, float(f)) * (1.0 - ss(22.0, 27.0, float(f)))
        nz[f] = pel_z_sm[f] * (1.0 - w_sm) + 0.5 * (pel_z_sm[f - 1] + pel_z_sm[f + 1]) * w_sm
    pel_z_sm = nz

tracked = [
    "pelvis", "spine_01", "spine_02", "spine_03", "neck_01",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    "thigh_l", "calf_l", "foot_l", "ik_foot_l",
    "thigh_r", "calf_r", "foot_r", "ik_foot_r",
]
tracks_out = {b: ([], [], []) for b in tracked}

for f in range(n):
    w = world_frames[f]
    nw = {b: w[b] for b in all_bones}
    up_vec = unreal.Vector(0.0, 0.0, 1.0)
    lat_vec = lat_vecs[f]
    fwd_vec = fwd_vecs[f]

    # Envelopes: keep f0..f1 clean, attack early by f2.5..f3.5 (v88-v92), and settle lean gradually to f33 (v152)
    e_w3 = ss(0.8, 3.2, float(f)) * (1.0 - ss(18.0, 33.0, float(f)))
    e_piv_lean = ss(0.8, 3.0, float(f)) * (1.0 - ss(6.5, 11.0, float(f)))
    e_run_lean = ss(6.0, 10.0, float(f)) * ((1.0 - ss(18.0, 34.0, float(f))) ** 1.4)
    e_bank = ss(0.8, 3.0, float(f)) * (1.0 - ss(10.0, 22.0, float(f)))
    e_head = ss(3.5, 7.5, float(f)) * (1.0 - ss(16.0, 25.0, float(f)))
    e_sh = ss(1.2, 3.5, float(f)) * (1.0 - ss(20.0, 31.0, float(f)))
    e_arm_l_back = ss(1.2, 3.5, float(f)) * (1.0 - ss(6.0, 9.5, float(f)))
    e_arm_r_fwd = ss(1.2, 3.5, float(f)) * (1.0 - ss(7.5, 13.0, float(f)))

    # 1. Pelvis: w3 forward COM shift + smooth continuous Z curve
    pel_pos = vadd(w["pelvis"].translation, vmul(fwd_vec, PELVIS_FWD_CM * e_w3))
    pel_pos.z = pel_z_sm[f] + PELVIS_DROP_CM * e_w3
    nw["pelvis"] = mk_tf(pel_pos, w["pelvis"].rotation, w["pelvis"].scale3d)

    # 2. Spine: early pivot banking + early pivot lean (v88-92) without excessive dive at v104-108
    q_sp_cum = unreal.Quat(0.0, 0.0, 0.0, 1.0)
    sp_weights = (("spine_01", 0.40), ("spine_02", 0.35), ("spine_03", 0.25))
    sp_par = {"spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02"}
    lean_deg = SPINE_LEAN_PIVOT_DEG * e_piv_lean + SPINE_LEAN_RUN_DEG * e_run_lean
    for sp, wt in sp_weights:
        par = sp_par[sp]
        pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
        q_bank = signed_axis_quat(fwd_vec, TORSO_BANK_DEG * wt * e_bank, up_vec, lat_vec)
        q_lean = signed_axis_quat(lat_vec, lean_deg * wt, up_vec, fwd_vec)
        q_step = q_lean * q_bank
        q_sp_cum = q_step * q_sp_cum
        nw[sp] = mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

    for sp, par in (("spine_04", "spine_03"), ("spine_05", "spine_04")):
        pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
        nw[sp] = mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

    # Head/neck lift in f4..f21 so head stays proud instead of buried between shoulders
    nk_pos = vadd(nw["spine_05"].translation, q_sp_cum.rotate_vector(vsub(w["neck_01"].translation, w["spine_05"].translation)))
    q_nk = signed_axis_quat(lat_vec, HEAD_LIFT_DEG * e_head, fwd_vec, up_vec)
    nw["neck_01"] = mk_tf(nk_pos, q_nk * q_sp_cum * w["neck_01"].rotation, w["neck_01"].scale3d)

    # 3. Legs: 29 (w3) trailing/catching swing feet + lower knee lift + 2-bone IK (keeps planted feet & bone lengths)
    e_lag = ss(1.5, 4.5, float(f)) * (1.0 - ss(10.0, 16.0, float(f)))
    e_catch = ss(9.0, 14.0, float(f)) * (1.0 - ss(20.0, 26.0, float(f)))
    for s in ("l", "r"):
        th, ca, fo = "thigh_" + s, "calf_" + s, "foot_" + s
        hip_old, knee_old, foot_old = w[th].translation, w[ca].translation, w[fo].translation
        hip_new = vadd(nw["pelvis"].translation, vsub(hip_old, w["pelvis"].translation))
        w_air = foot_air_w[s][f]
        foot_tgt = unreal.Vector(foot_old.x, foot_old.y, foot_old.z)
        if w_air > 1e-4:
            # Lower knee lift + swing lag early / catch-up late (feet recovering unbalanced COM)
            excess_z = max(0.0, foot_old.z - 14.0)
            foot_tgt.z = foot_old.z - KNEE_DROP * excess_z * e_w3 * w_air
            foot_tgt = vadd(foot_tgt, vmul(fwd_vec, (-SWING_LAG_CM * e_lag + CATCH_FWD_CM * e_catch) * w_air))
        knee_new, foot_new = solve_2bone_ik(hip_new, foot_tgt, hip_old, knee_old, foot_old)
        q_th = axis_quat(vcross(vsub(knee_old, hip_old), vsub(knee_new, hip_new)),
                         math.degrees(math.acos(max(-1.0, min(1.0, vdot(vnorm(vsub(knee_old, hip_old)), vnorm(vsub(knee_new, hip_new)))))))) \
            if vlen(vcross(vsub(knee_old, hip_old), vsub(knee_new, hip_new))) > 1e-4 else unreal.Quat(0.0, 0.0, 0.0, 1.0)
        q_ca = axis_quat(vcross(vsub(foot_old, knee_old), vsub(foot_new, knee_new)),
                         math.degrees(math.acos(max(-1.0, min(1.0, vdot(vnorm(vsub(foot_old, knee_old)), vnorm(vsub(foot_new, knee_new)))))))) \
            if vlen(vcross(vsub(foot_old, knee_old), vsub(foot_new, knee_new))) > 1e-4 else unreal.Quat(0.0, 0.0, 0.0, 1.0)
        nw[th] = mk_tf(hip_new, q_th * w[th].rotation, w[th].scale3d)
        nw[ca] = mk_tf(knee_new, q_ca * w[ca].rotation, w[ca].scale3d)
        nw[fo] = mk_tf(foot_new, w[fo].rotation, w[fo].scale3d)

    # 4. Shoulders & Arms: high/open clavicles, L arm cleanly BACK behind torso (not sideways wing), R arm counterbalancing
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
        q_ua_lat = unreal.Quat(0.0, 0.0, 0.0, 1.0)
        q_ua_swing = unreal.Quat(0.0, 0.0, 0.0, 1.0)
        if s == "l":
            if e_arm_l_back > 1e-4:
                # Pull L upperarm in/down at v92-96 while keeping f10 clearance >=14cm
                q_ua_lat = signed_axis_quat(fwd_vec, ARM_L_TUCK_IN_DEG * e_arm_l_back, la_dir0, vmul(up_vec, -1.0))
                la_dir1 = q_ua_lat.rotate_vector(la_dir0)
                q_ua_swing = signed_axis_quat(lat_vec, ARM_L_BACK_DEG * e_arm_l_back, la_dir1, vmul(fwd_vec, -1.0))
            # Safe outward clearance as arm_l swings past thigh_l around f10
            e_l_clr = ss(6.5, 9.5, float(f)) * (1.0 - ss(12.0, 16.0, float(f)))
            if e_l_clr > 1e-4:
                q_ua_lat = signed_axis_quat(fwd_vec, 9.0 * e_l_clr, la_dir0, out_dir) * q_ua_lat
        else:
            q_ua_lat = signed_axis_quat(fwd_vec, ARM_R_OUT_DEG * e_sh, la_dir0, out_dir)
            if e_arm_r_fwd > 1e-4:
                q_ua_swing = signed_axis_quat(lat_vec, ARM_R_FWD_DEG * e_arm_r_fwd, la_dir0, fwd_vec)
        q_ua = q_ua_swing * q_ua_lat
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
        "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "neck_01": "spine_05",
        "clavicle_l": "spine_05", "upperarm_l": "clavicle_l", "lowerarm_l": "upperarm_l", "hand_l": "lowerarm_l",
        "clavicle_r": "spine_05", "upperarm_r": "clavicle_r", "lowerarm_r": "upperarm_r", "hand_r": "lowerarm_r",
        "thigh_l": "pelvis", "calf_l": "thigh_l", "foot_l": "calf_l", "ik_foot_l": "ik_foot_root",
        "thigh_r": "pelvis", "calf_r": "thigh_r", "foot_r": "calf_r", "ik_foot_r": "ik_foot_root",
    }
    for b in tracked:
        if b.startswith("ik_foot_"):
            loc = nw["foot_" + b[-1]].make_relative(nw["ik_foot_root"])
        else:
            loc = nw[b].make_relative(nw[parents[b]])
        tracks_out[b][0].append(loc.translation)
        tracks_out[b][1].append(loc.rotation)
        tracks_out[b][2].append(loc.scale3d)

# Smooth upper-body local curves across f2..f24 (4 passes) keeping f0-f1 intact (no v77 hitch)
for b in ("spine_01", "spine_02", "spine_03", "neck_01", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"):
    pos, rot, scl = tracks_out[b]
    for _ in range(4):
        nr = list(rot)
        for f in range(2, min(25, n - 1)):
            w_sm = 0.5 * ss(1.0, 3.0, float(f)) * (1.0 - ss(17.0, 25.0, float(f)))
            nr[f] = qnlerp(rot[f], qnlerp(rot[f - 1], rot[f + 1], 0.5), w_sm)
        rot = nr
    tracks_out[b] = (pos, rot, scl)

saved = False
if SAVE:
    ctrl = seq.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("turnaround 180L c2 from 27 + 29 w3"), False)
    for b, (pos, rot, scl) in tracks_out.items():
        ctrl.set_bone_track_keys(b, pos, rot, scl, False)
    ctrl.close_bracket(False)
    saved = bool(EAL.save_asset(DST, only_if_is_dirty=False))

rep = {
    "src": SRC, "dst": DST, "frames": n, "saved": saved,
    "params": {
        "pelvis_fwd_cm": PELVIS_FWD_CM, "pelvis_drop_cm": PELVIS_DROP_CM,
        "spine_lean_pivot_deg": SPINE_LEAN_PIVOT_DEG, "spine_lean_run_deg": SPINE_LEAN_RUN_DEG,
        "torso_bank_deg": TORSO_BANK_DEG, "head_lift_deg": HEAD_LIFT_DEG,
        "arm_l_back_deg": ARM_L_BACK_DEG, "arm_l_elbow_deg": ARM_L_ELBOW_DEG,
        "arm_l_tuck_in_deg": ARM_L_TUCK_IN_DEG, "arm_r_fwd_deg": ARM_R_FWD_DEG, "arm_r_out_deg": ARM_R_OUT_DEG,
    },
}
os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
json.dump(rep, open(OUT_JSON, "w"), indent=2)
print("ESITO: OK 180L_c2 src=w2t180Lp frames=%d saved=%s" % (n, saved))
