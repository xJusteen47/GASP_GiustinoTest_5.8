"""Turnaround 180R / 090L / 090R (variant 28): agile Link-OoT + 29 (w3) physics mirrored from turnaround_180l_ue.py.
Env: TURN_LIST=180R,090L,090R (default), GSTN_SAVE=1 to save assets.
Reads clean AS_GSTN_RunStart_Generic_w2t<TURN>f (variant 26/27 base) and writes AS_GSTN_RunStart_Generic_w2t<TURN>s."""
import json
import math
import os

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SAVE = os.environ.get("GSTN_SAVE") == "1" or os.environ.get("CLIPFIX_WRITE") == "1"
TURNS = [t.strip() for t in os.environ.get("TURN_LIST", "180R,090L,090R").split(",") if t.strip()]

PELVIS_FWD_CM = float(os.environ.get("T_PELVIS_FWD", "12.0"))
PELVIS_DROP_CM = float(os.environ.get("T_PELVIS_DROP", "-1.5"))
SPINE_LEAN_PIVOT_DEG = float(os.environ.get("T_SPINE_LEAN_PIVOT", "10.0"))
SPINE_LEAN_RUN_DEG = float(os.environ.get("T_SPINE_LEAN_RUN", "5.0"))
TORSO_BANK_DEG = float(os.environ.get("T_TORSO_BANK", "5.0"))
HEAD_LIFT_DEG = float(os.environ.get("T_HEAD_LIFT", "6.5"))
KNEE_DROP = float(os.environ.get("T_KNEE_DROP", "0.30"))
SWING_LAG_CM = float(os.environ.get("T_SWING_LAG", "4.0"))
CATCH_FWD_CM = float(os.environ.get("T_CATCH_FWD", "5.5"))
LAT_WIDEN_CM = float(os.environ.get("T_LAT_WIDEN", "2.5"))
PIVOT_HEEL_DEG = float(os.environ.get("T_PIVOT_HEEL", "14.0"))

CLAV_ST_ELEV_DEG = float(os.environ.get("T_CLAV_ST_ELEV", "7.0"))
CLAV_PV_ELEV_DEG = float(os.environ.get("T_CLAV_PV_ELEV", "3.0"))
CLAV_OPEN_DEG = float(os.environ.get("T_CLAV_OPEN", "6.0"))
ARM_ST_TUCK_IN_DEG = float(os.environ.get("T_ARM_ST_TUCK_IN", "14.0"))
ARM_PV_OUT_DEG = float(os.environ.get("T_ARM_PV_OUT", "3.0"))
ARM_ST_BACK_DEG = float(os.environ.get("T_ARM_ST_BACK", "24.0"))
ARM_ST_ELBOW_DEG = float(os.environ.get("T_ARM_ST_ELBOW", "16.0"))
ARM_PV_FWD_DEG = float(os.environ.get("T_ARM_PV_FWD", "22.0"))
ARM_PV_ELBOW_DEG = float(os.environ.get("T_ARM_PV_ELBOW", "24.0"))


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


def process_turn(turn):
    deg = int(turn[:3])
    side = turn[3]
    st, pv = ("l", "r") if side == "L" else ("r", "l")
    bank_sign = 1.0 if side == "L" else -1.0  # inward roll toward turn side (+lat for L, -lat for R)
    piv_scale = 0.8 if deg == 90 else 1.0     # 90-deg turn pivots ~1-2f faster

    src = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t%sf" % turn
    if not EAL.does_asset_exist(src):
        src = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t%sp" % turn
    dst = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t%ss" % turn

    if SAVE:
        if EAL.does_asset_exist(dst):
            EAL.delete_asset(dst)
        if not EAL.duplicate_asset(src, dst):
            raise RuntimeError("Failed to duplicate %s -> %s" % (src, dst))
        seq = unreal.load_asset(dst)
    else:
        seq = unreal.load_asset(src)

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

    foot_air_w = {"l": [], "r": []}
    for s in ("l", "r"):
        fz = [world_frames[f]["foot_" + s].translation.z for f in range(n)]
        fmin = min(fz[:min(n, 28)])
        for f in range(n):
            foot_air_w[s].append(ss(fmin + 1.5, fmin + 6.0, fz[f]))

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
        lat_vecs.append(lat_vec)
        fwd_vecs.append(vnorm(vcross(lat_vec, up_vec)))

    ref_fwd = run_dir
    for f in range(n - 1, -1, -1):
        if vdot(fwd_vecs[f], ref_fwd) < 0.0:
            fwd_vecs[f] = vmul(fwd_vecs[f], -1.0)
        ref_fwd = fwd_vecs[f]

    pel_z_sm = [world_frames[f]["pelvis"].translation.z for f in range(n)]
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
        inward_dir = vmul(lat_vec, bank_sign)

        e_w3 = ss(0.8, 3.2, float(f)) * (1.0 - ss(18.0, 33.0, float(f)))
        e_piv_lean = ss(0.8, 3.0, float(f)) * (1.0 - ss(6.5 * piv_scale, 11.0 * piv_scale, float(f)))
        e_run_lean = ss(6.0 * piv_scale, 10.0 * piv_scale, float(f)) * ((1.0 - ss(18.0, 34.0, float(f))) ** 1.4)
        e_bank = ss(0.8, 3.0, float(f)) * (1.0 - ss(10.0 * piv_scale, 22.0, float(f)))
        e_head = ss(3.5, 7.5, float(f)) * (1.0 - ss(16.0, 25.0, float(f)))
        e_sh = ss(1.2, 3.5, float(f)) * (1.0 - ss(20.0, 31.0, float(f)))
        e_arm_st_back = ss(1.0, 3.2, float(f)) * (1.0 - ss(6.0 * piv_scale, 9.5 * piv_scale, float(f)))
        e_arm_pv_fwd = ss(1.0, 3.2, float(f)) * (1.0 - ss(7.5 * piv_scale, 13.0 * piv_scale, float(f)))

        pel_pos = vadd(w["pelvis"].translation, vmul(fwd_vec, PELVIS_FWD_CM * e_w3))
        pel_pos.z = pel_z_sm[f] + PELVIS_DROP_CM * e_w3
        nw["pelvis"] = mk_tf(pel_pos, w["pelvis"].rotation, w["pelvis"].scale3d)

        q_sp_cum = unreal.Quat(0.0, 0.0, 0.0, 1.0)
        sp_weights = (("spine_01", 0.40), ("spine_02", 0.35), ("spine_03", 0.25))
        sp_par = {"spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02"}
        lean_deg = SPINE_LEAN_PIVOT_DEG * e_piv_lean + SPINE_LEAN_RUN_DEG * e_run_lean
        for sp, wt in sp_weights:
            par = sp_par[sp]
            pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
            q_bank = signed_axis_quat(fwd_vec, TORSO_BANK_DEG * wt * e_bank, up_vec, inward_dir)
            q_lean = signed_axis_quat(lat_vec, lean_deg * wt, up_vec, fwd_vec)
            q_sp_cum = (q_lean * q_bank) * q_sp_cum
            nw[sp] = mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

        for sp, par in (("spine_04", "spine_03"), ("spine_05", "spine_04")):
            pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
            nw[sp] = mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

        nk_pos = vadd(nw["spine_05"].translation, q_sp_cum.rotate_vector(vsub(w["neck_01"].translation, w["spine_05"].translation)))
        q_nk = signed_axis_quat(lat_vec, HEAD_LIFT_DEG * e_head, fwd_vec, up_vec)
        nw["neck_01"] = mk_tf(nk_pos, q_nk * q_sp_cum * w["neck_01"].rotation, w["neck_01"].scale3d)

        e_lag = ss(1.5, 4.5, float(f)) * (1.0 - ss(10.0, 16.0, float(f)))
        e_catch = ss(9.0, 14.0, float(f)) * (1.0 - ss(20.0, 26.0, float(f)))
        e_piv_foot = ss(1.2, 3.2, float(f)) * (1.0 - ss(5.5 * piv_scale, 9.5 * piv_scale, float(f)))
        e_widen = ss(2.0, 6.0, float(f)) * (1.0 - ss(11.0, 16.0, float(f)))
        for s in ("l", "r"):
            th, ca, fo = "thigh_" + s, "calf_" + s, "foot_" + s
            hip_old, knee_old, foot_old = w[th].translation, w[ca].translation, w[fo].translation
            hip_new = vadd(nw["pelvis"].translation, vsub(hip_old, w["pelvis"].translation))
            w_air = foot_air_w[s][f]
            foot_tgt = unreal.Vector(foot_old.x, foot_old.y, foot_old.z)
            if e_widen > 1e-4:
                foot_tgt = vadd(foot_tgt, vmul(lat_vec, (LAT_WIDEN_CM if s == "l" else -LAT_WIDEN_CM) * e_widen))
            if w_air > 1e-4:
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
            q_fo_push = unreal.Quat(0.0, 0.0, 0.0, 1.0)
            if s == pv and e_piv_foot > 1e-4:
                ball_dir = vsub(w["ball_" + pv].translation, foot_old)
                q_fo_push = signed_axis_quat(lat_vec, PIVOT_HEEL_DEG * e_piv_foot, ball_dir, vmul(up_vec, -1.0))
            nw[th] = mk_tf(hip_new, q_th * w[th].rotation, w[th].scale3d)
            nw[ca] = mk_tf(knee_new, q_ca * w[ca].rotation, w[ca].scale3d)
            nw[fo] = mk_tf(foot_new, q_fo_push * w[fo].rotation, w[fo].scale3d)

        for s, sign in (("l", 1.0), ("r", -1.0)):
            cl, ua, la, ha = "clavicle_" + s, "upperarm_" + s, "lowerarm_" + s, "hand_" + s
            out_dir = vmul(lat_vec, sign)
            clav_elev = CLAV_ST_ELEV_DEG if s == st else CLAV_PV_ELEV_DEG

            cl_pos = vadd(nw["spine_05"].translation, q_sp_cum.rotate_vector(vsub(w[cl].translation, w["spine_05"].translation)))
            ua_dir0 = q_sp_cum.rotate_vector(vsub(w[ua].translation, w[cl].translation))
            q_cl_elev = signed_axis_quat(fwd_vec, clav_elev * e_sh, ua_dir0, up_vec)
            q_cl_open = signed_axis_quat(up_vec, CLAV_OPEN_DEG * e_sh, ua_dir0, vmul(fwd_vec, -1.0))
            q_cl = q_cl_open * q_cl_elev
            nw[cl] = mk_tf(cl_pos, q_cl * q_sp_cum * w[cl].rotation, w[cl].scale3d)

            ua_pos = vadd(cl_pos, q_cl.rotate_vector(ua_dir0))
            la_dir0 = (q_cl * q_sp_cum).rotate_vector(vsub(w[la].translation, w[ua].translation))
            q_ua_lat = unreal.Quat(0.0, 0.0, 0.0, 1.0)
            q_ua_swing = unreal.Quat(0.0, 0.0, 0.0, 1.0)
            if s == st:
                if e_arm_st_back > 1e-4:
                    q_ua_lat = signed_axis_quat(fwd_vec, ARM_ST_TUCK_IN_DEG * e_arm_st_back, la_dir0, vmul(up_vec, -1.0))
                    la_dir1 = q_ua_lat.rotate_vector(la_dir0)
                    q_ua_swing = signed_axis_quat(lat_vec, ARM_ST_BACK_DEG * e_arm_st_back, la_dir1, vmul(fwd_vec, -1.0))
                e_clr = ss(6.5, 9.5, float(f)) * (1.0 - ss(12.0, 16.0, float(f)))
                if e_clr > 1e-4:
                    q_ua_lat = signed_axis_quat(fwd_vec, 10.0 * e_clr, la_dir0, out_dir) * q_ua_lat
            else:
                q_ua_lat = signed_axis_quat(fwd_vec, ARM_PV_OUT_DEG * e_sh, la_dir0, out_dir)
                if e_arm_pv_fwd > 1e-4:
                    q_ua_swing = signed_axis_quat(lat_vec, ARM_PV_FWD_DEG * e_arm_pv_fwd, la_dir0, fwd_vec)
            q_ua = q_ua_swing * q_ua_lat
            nw[ua] = mk_tf(ua_pos, q_ua * q_cl * q_sp_cum * w[ua].rotation, w[ua].scale3d)

            la_pos = vadd(ua_pos, q_ua.rotate_vector(la_dir0))
            ha_dir0 = (q_ua * q_cl * q_sp_cum).rotate_vector(vsub(w[ha].translation, w[la].translation))
            q_la = unreal.Quat(0.0, 0.0, 0.0, 1.0)
            el_deg = (ARM_ST_ELBOW_DEG * e_arm_st_back) if s == st else (ARM_PV_ELBOW_DEG * e_arm_pv_fwd)
            if el_deg > 1e-3:
                hinge = vcross(la_dir0, ha_dir0)
                ax = vnorm(hinge) if vlen(hinge) > 1e-3 else lat_vec
                qp = axis_quat(ax, el_deg)
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
            loc = nw["foot_" + b[-1]].make_relative(nw["ik_foot_root"]) if b.startswith("ik_foot_") else nw[b].make_relative(nw[parents[b]])
            tracks_out[b][0].append(loc.translation)
            tracks_out[b][1].append(loc.rotation)
            tracks_out[b][2].append(loc.scale3d)

    for b in ("spine_01", "spine_02", "spine_03", "neck_01", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"):
        pos, rot, scl = tracks_out[b]
        for _ in range(4):
            nr = list(rot)
            for f in range(2, min(25, n - 1)):
                w_sm = 0.5 * ss(1.0, 3.0, float(f)) * (1.0 - ss(17.0, 25.0, float(f)))
                nr[f] = qnlerp(rot[f], qnlerp(rot[f - 1], rot[f + 1], 0.5), w_sm)
            rot = nr
        tracks_out[b] = (pos, rot, scl)

    for b in ("thigh_l", "calf_l", "thigh_r", "calf_r"):
        pos, rot, scl = tracks_out[b]
        for _ in range(2):
            nr = list(rot)
            for f in range(8, min(16, n - 1)):
                w_sm = 0.5 * ss(7.0, 9.5, float(f)) * (1.0 - ss(13.0, 16.0, float(f)))
                nr[f] = qnlerp(rot[f], qnlerp(rot[f - 1], rot[f + 1], 0.5), w_sm)
            rot = nr
        tracks_out[b] = (pos, rot, scl)

    saved = False
    if SAVE:
        ctrl = seq.get_editor_property("controller")
        ctrl.open_bracket(unreal.Text("turnaround %s agile Link-OoT" % turn), False)
        for b, (pos, rot, scl) in tracks_out.items():
            ctrl.set_bone_track_keys(b, pos, rot, scl, False)
        ctrl.close_bracket(False)
        saved = bool(EAL.save_asset(dst, only_if_is_dirty=False))
    print("ESITO: OK %s src=%s dst=%s frames=%d saved=%s" % (turn, src.split("/")[-1], dst.split("/")[-1], n, saved))


for t in TURNS:
    process_turn(t)
