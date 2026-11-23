"""RunStart Generic w3 (runstart-w3, 2026-11-22): additive layer on top of w2 (w2 untouched, rule 13b).
Owner ask: lower knee lift in f0-f14, torso/COM shifted forward at the start ("falling forward", legs trailing behind
instead of bending in place), fading out cleanly by f22 into the stock loop.
Headless via Tools/CI/Save-Assets.ps1 (dry run unless GSTN_SAVE=1). Prints <= 4 lines."""
import json
import math
import os

import unreal

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2"
DST = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w3"
SAVE = os.environ.get("GSTN_SAVE") == "1" or os.environ.get("CLIPFIX_WRITE") == "1"
OUT = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart", "AS_GSTN_RunStart_Generic_w3_build.json")

PELVIS_DY = float(os.environ.get("W3_PELVIS_DY", "12.0"))    # cm forward (+Y)
PELVIS_DZ = float(os.environ.get("W3_PELVIS_DZ", "-4.0"))    # cm vertical (lower COM/head)
SPINE_PITCH = float(os.environ.get("W3_SPINE_PITCH", "3.5")) # deg forward (continuous diagonal line, no chest collapse)
KNEE_DROP = float(os.environ.get("W3_KNEE_DROP", "0.35"))    # fraction of swing foot height above 14 cm
SWING_LAG = float(os.environ.get("W3_SWING_LAG", "4.0"))     # cm trailing (-Y) on swing foot in f0-f10 (torso leads)
CATCH_FWD = float(os.environ.get("W3_CATCH_FWD", "5.5"))     # cm forward (+Y) on swing foot in f10-f22 (legs catch up)
LEG_L_PUSH13 = float(os.environ.get("W3_LEG_L_PUSH13", "10.0")) # cm back (-Y) on foot_l push-off at f13 (v107)
LAT_WIDEN = float(os.environ.get("W3_LAT_WIDEN", "6.0"))     # cm lateral widening per foot for >= 10 cm ankle gap
ARM_FWD_SH = float(os.environ.get("W3_ARM_FWD_SH", "12.0"))  # deg forward swing on lead upperarm
ARM_FWD_EL = float(os.environ.get("W3_ARM_FWD_EL", "0.0"))   # keep lead elbow bent ~80-90 deg (no straight extension)
ARM_FWD_OUT = float(os.environ.get("W3_ARM_FWD_OUT", "5.0")) # deg outward open on lead upperarm
ARM_R_OUT = float(os.environ.get("W3_ARM_R_OUT", "10.0"))    # deg outward abduction on arm_r (f5-f13 silhouette)
ARM_R_FL5 = float(os.environ.get("W3_ARM_R_FL5", "40.0"))     # deg extra elbow flexion on arm_r around f5 (soften 171->~118 deg)
HEAD_UP13 = float(os.environ.get("W3_HEAD_UP13", "6.5"))     # deg chin/gaze lift on neck_01 at f13 (v107), f5-f21
ARM_L_OUT = float(os.environ.get("W3_ARM_L_OUT", "10.0"))    # deg outward abduction on arm_l (f5-f13 silhouette)
ARM_L_FORE_OUT = float(os.environ.get("W3_ARM_L_FORE_OUT", "16.0")) # deg forearm_l outward unhook at f5
ARM_L_FWD13 = float(os.environ.get("W3_ARM_L_FWD13", "14.0"))       # deg arm_l forward arc at f13 (v107)
ARM_BACK_SH = float(os.environ.get("W3_ARM_BACK_SH", "16.0"))# deg back & up swing on rear upperarm
ARM_BACK_FL = float(os.environ.get("W3_ARM_BACK_FL", "22.0"))# deg extra elbow flexion on rear forearm (athletic sprint drive)
CLAV_SWING = float(os.environ.get("W3_CLAV_SWING", "6.0"))   # deg clavicle protraction/retraction
CHEST_TWIST = float(os.environ.get("W3_CHEST_TWIST", "3.5")) # deg chest counter-rotation vs pelvis
ARM_DRAG = float(os.environ.get("W3_ARM_DRAG", "0.20"))      # 1-frame elastic drag on forearms/wrists


def ss(a, b, x):
    if x <= a:
        return 0.0
    if x >= b:
        return 1.0
    t = (x - a) / float(b - a)
    return t * t * (3.0 - 2.0 * t)


def env(f):
    # Contact-anchored envelope: accelerated ease-in f0->f6 (1st contact R), hold through f14 (2nd contact L at f11-13), fade f14->f26
    x = float(f)
    if x <= 0.0 or x >= 26.0:
        return 0.0
    if x < 6.0:
        t = x / 6.0
        return t * t * (1.2 - 0.2 * t)   # accelerated ease-in into 1st contact
    if x <= 14.0:
        return 1.0
    return 1.0 - ss(14.0, 26.0, x)


def env_trunk_settle(f):
    # Residual acceleration lean at f21 (v122) with slow-in damped settle to f29 (v137); 0 for f <= 13
    x = float(f)
    if x <= 13.0 or x >= 29.0:
        return 0.0
    return ss(13.0, 19.0, x) * ((1.0 - ss(21.0, 29.0, x)) ** 1.5)


def contact_dip(f):
    # Extra pelvis/spine absorption (-Z) at 1st contact (R f6-f8) and 2nd contact (L f11-f13), rebound between
    x = float(f)
    d1 = math.exp(-((x - 7.0) / 1.4) ** 2)   # 1st contact R at f6-f8
    d2 = math.exp(-((x - 12.0) / 1.4) ** 2)  # 2nd contact L at f11-f13
    return -2.2 * d1 - 1.6 * d2


def env_lat(f):
    return ss(0.0, 2.0, float(f)) * (1.0 - ss(19.0, 22.0, float(f)))


def env_lag(f):
    return ss(0.0, 3.0, float(f)) * (1.0 - ss(9.0, 16.0, float(f)))


def env_catch(f):
    return ss(9.0, 14.0, float(f)) * (1.0 - ss(19.0, 25.0, float(f)))


def qinv(q):
    return unreal.Quat(-q.x, -q.y, -q.z, q.w)


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
    return unreal.Vector(a.x / l, a.y / l, a.z / l) if l > 1e-6 else unreal.Vector(0, 1, 0)


def vdot(a, b):
    return a.x * b.x + a.y * b.y + a.z * b.z


def mk_tf(pos, quat, scl):
    return unreal.Transform(location=pos, rotation=quat.rotator(), scale=scl)


def qnlerp(a, b, t):
    dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w
    s = -1.0 if dot < 0.0 else 1.0
    x = a.x * (1.0 - t) + s * b.x * t
    y = a.y * (1.0 - t) + s * b.y * t
    z = a.z * (1.0 - t) + s * b.z * t
    w = a.w * (1.0 - t) + s * b.w * t
    l = math.sqrt(x * x + y * y + z * z + w * w)
    return unreal.Quat(x / l, y / l, z / l, w / l)


def quat_between(u, v):
    u, v = vnorm(u), vnorm(v)
    d = max(-1.0, min(1.0, vdot(u, v)))
    if d > 0.999999:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    if d < -0.999999:
        return unreal.Quat(1.0, 0.0, 0.0, 0.0)
    c = unreal.Vector(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x)
    w = 1.0 + d
    l = math.sqrt(c.x * c.x + c.y * c.y + c.z * c.z + w * w)
    return unreal.Quat(c.x / l, c.y / l, c.z / l, w / l)


def solve_2bone_ik(hip_new, foot_new, hip_old, knee_old, foot_old):
    L1 = vlen(vsub(knee_old, hip_old))
    L2 = vlen(vsub(foot_old, knee_old))
    d_vec = vsub(foot_new, hip_new)
    d = vlen(d_vec)
    d_clamped = max(abs(L1 - L2) + 0.5, min(L1 + L2 - 0.5, d))
    dir_hf = vnorm(d_vec)
    # Pole direction from old knee offset perpendicular to old hip->foot
    old_hf = vnorm(vsub(foot_old, hip_old))
    off = vsub(knee_old, hip_old)
    pole = vsub(off, vmul(old_hf, vdot(off, old_hf)))
    if vlen(pole) < 1e-3:
        pole = unreal.Vector(0.0, 1.0, 0.0)
    # Orthogonalize pole against new dir_hf
    pole = vsub(pole, vmul(dir_hf, vdot(pole, dir_hf)))
    pole = vnorm(pole)
    cos_a = max(-1.0, min(1.0, (L1 * L1 + d_clamped * d_clamped - L2 * L2) / (2.0 * L1 * d_clamped)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    knee_new = vadd(hip_new, vadd(vmul(dir_hf, L1 * cos_a), vmul(pole, L1 * sin_a)))
    foot_clamped = vadd(hip_new, vmul(dir_hf, d_clamped))
    return knee_new, foot_clamped


def pitch_quat(deg, ref_vec):
    r = math.radians(deg)
    q_pos = unreal.Quat(math.sin(0.5 * r), 0.0, 0.0, math.cos(0.5 * r))
    q_neg = unreal.Quat(-math.sin(0.5 * r), 0.0, 0.0, math.cos(0.5 * r))
    if q_pos.rotate_vector(ref_vec).y >= q_neg.rotate_vector(ref_vec).y:
        return q_pos
    return q_neg


def yaw_quat(deg):
    r = math.radians(deg)
    return unreal.Quat(0.0, 0.0, math.sin(0.5 * r), math.cos(0.5 * r))


unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/GSTN", "/Game/Characters"], True)

if SAVE:
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    assert EAL.duplicate_asset(SRC, DST), "duplicate failed"
    seq = unreal.load_asset(DST)
else:
    seq = unreal.load_asset(SRC)

n = int(seq.get_editor_property("number_of_sampled_keys"))
opts = unreal.AnimPoseEvaluationOptions()
bones_needed = ["root", "pelvis", "spine_01", "neck_01", "ik_foot_root",
                "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r",
                "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
                "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"]

tracks = {b: ([], [], []) for b in ["pelvis", "spine_01", "neck_01",
                                     "clavicle_l", "upperarm_l", "lowerarm_l",
                                     "clavicle_r", "upperarm_r", "lowerarm_r",
                                     "thigh_l", "calf_l", "foot_l", "ik_foot_l",
                                     "thigh_r", "calf_r", "foot_r", "ik_foot_r"]}

# Precompute w2 contralateral arm-lead signal (+ = L arm ahead, - = R arm ahead), 1-tap smoothed to preserve exact L/R phase
raw_hd = []
for f in range(n):
    p0 = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0, opts)
    el = unreal.AnimPoseExtensions.get_bone_pose(p0, "lowerarm_l", unreal.AnimPoseSpaces.WORLD).translation.y - unreal.AnimPoseExtensions.get_bone_pose(p0, "upperarm_l", unreal.AnimPoseSpaces.WORLD).translation.y
    er = unreal.AnimPoseExtensions.get_bone_pose(p0, "lowerarm_r", unreal.AnimPoseSpaces.WORLD).translation.y - unreal.AnimPoseExtensions.get_bone_pose(p0, "upperarm_r", unreal.AnimPoseSpaces.WORLD).translation.y
    hl = unreal.AnimPoseExtensions.get_bone_pose(p0, "hand_l", unreal.AnimPoseSpaces.WORLD).translation.y - unreal.AnimPoseExtensions.get_bone_pose(p0, "upperarm_l", unreal.AnimPoseSpaces.WORLD).translation.y
    hr = unreal.AnimPoseExtensions.get_bone_pose(p0, "hand_r", unreal.AnimPoseSpaces.WORLD).translation.y - unreal.AnimPoseExtensions.get_bone_pose(p0, "upperarm_r", unreal.AnimPoseSpaces.WORLD).translation.y
    raw_hd.append(math.tanh(((el - er) + 0.5 * (hl - hr)) / 18.0))
hd_sig = list(raw_hd)
for f in range(1, n - 1):
    hd_sig[f] = 0.2 * raw_hd[f - 1] + 0.6 * raw_hd[f] + 0.2 * raw_hd[f + 1]

max_dy = 0.0
w0 = None
foot_air_w = {"l": [], "r": []}
for f in range(n):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0, opts)
    w = {b: unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD) for b in bones_needed}
    if w0 is None:
        w0 = w
    for s_ in ("l", "r"):
        fz_ = w["foot_" + s_].translation.z
        w_air = ss(15.0, 22.0, float(fz_)) * ss(0.0, 2.0, float(f)) * (1.0 - ss(22.0, 26.0, float(f)))
        foot_air_w[s_].append(w_air)
    e = env(f)
    es = env_trunk_settle(f)
    if e <= 1e-6 and es <= 1e-6:
        nw = dict(w)
    else:
        nw = {}
        nw["root"] = w["root"]
        nw["ik_foot_root"] = w["ik_foot_root"]
        # 1. Pelvis forward shift + contact-anchored absorption dip
        dy = PELVIS_DY * e
        dz = PELVIS_DZ * e + contact_dip(f)
        max_dy = max(max_dy, dy)
        p_tf = mk_tf(vadd(w["pelvis"].translation, unreal.Vector(0.0, dy, dz)), w["pelvis"].rotation, w["pelvis"].scale3d)
        nw["pelvis"] = p_tf

        # 2. Spine_01 forward pitch (1-frame elastic lag + impact nod) + slight chest counter-rotation
        sp_pos = vadd(w["spine_01"].translation, unreal.Vector(0.0, dy, dz))
        e_sp = env(max(0, f - 1))
        sp_deg = SPINE_PITCH * e_sp - 0.3 * contact_dip(max(0, f - 1)) + 9.0 * env_trunk_settle(f)
        q_sp_pit = pitch_quat(sp_deg, vsub(w["neck_01"].translation, w["spine_01"].translation))
        q_sp_yaw = yaw_quat(CHEST_TWIST * e * hd_sig[f])
        q_sp = q_sp_yaw * q_sp_pit
        nw["spine_01"] = mk_tf(sp_pos, q_sp * w["spine_01"].rotation, w["spine_01"].scale3d)

        # 3. Neck_01 2-frame elastic lag keeping gaze forward + chin lift at f13 (f5-f21)
        nk_off = vsub(w["neck_01"].translation, w["spine_01"].translation)
        nk_pos = vadd(sp_pos, q_sp.rotate_vector(nk_off))
        e_nk = env(max(0, f - 2))
        nk_frac = -0.25 * (e_nk / max(e_sp, 1e-3)) if e_sp > 1e-3 else 0.0
        q_nk = unreal.Quat(q_sp_pit.x * nk_frac, 0.0, 0.0, math.sqrt(max(0.0, 1.0 - (q_sp_pit.x * nk_frac) ** 2)))
        e_h13 = ss(5.0, 13.0, float(f)) * (1.0 - ss(13.0, 21.0, float(f)))
        if e_h13 > 1e-4:
            rh = math.radians(HEAD_UP13 * e_h13)
            q_hp = unreal.Quat(math.sin(0.5 * rh), 0.0, 0.0, math.cos(0.5 * rh))
            q_hn = qinv(q_hp)
            vfwd = unreal.Vector(0.0, 10.0, 0.0)
            q_hup = q_hp if q_hp.rotate_vector(vfwd).z >= q_hn.rotate_vector(vfwd).z else q_hn
            q_nk = q_hup * q_nk
        nw["neck_01"] = mk_tf(nk_pos, q_nk * q_sp * w["neck_01"].rotation, w["neck_01"].scale3d)

        # 4. Legs 2-bone IK + lateral ankle clearance + staggered torso-lead / leg-catch-up
        lat_vec = vsub(w["thigh_l"].translation, w["thigh_r"].translation)
        lat_vec = vnorm(unreal.Vector(lat_vec.x, lat_vec.y, 0.0))
        el = env_lat(f)
        elag = env_lag(f)
        ecat = env_catch(f)
        for s, sign in (("l", 1.0), ("r", -1.0)):
            th, ca, fo = "thigh_" + s, "calf_" + s, "foot_" + s
            hip_old = w[th].translation
            knee_old = w[ca].translation
            foot_old = w[fo].translation
            hip_new = vadd(hip_old, unreal.Vector(0.0, dy, dz))
            fz = foot_old.z
            if fz > 15.0:
                fz_new = fz - KNEE_DROP * (fz - 14.0) * e
                fy_new = foot_old.y - SWING_LAG * elag + CATCH_FWD * ecat
            else:
                fz_new = fz
                fy_new = foot_old.y
            if s == "l":
                e_l13 = ss(8.0, 13.0, float(f)) * (1.0 - ss(13.0, 20.0, float(f)))
                fy_new -= LEG_L_PUSH13 * e_l13
            foot_new = vadd(unreal.Vector(foot_old.x, fy_new, fz_new), vmul(lat_vec, sign * LAT_WIDEN * el))
            knee_new, foot_new = solve_2bone_ik(hip_new, foot_new, hip_old, knee_old, foot_old)

            q_th_delta = quat_between(vsub(knee_old, hip_old), vsub(knee_new, hip_new))
            q_ca_delta = quat_between(vsub(foot_old, knee_old), vsub(foot_new, knee_new))

            nw[th] = mk_tf(hip_new, q_th_delta * w[th].rotation, w[th].scale3d)
            nw[ca] = mk_tf(knee_new, q_ca_delta * w[ca].rotation, w[ca].scale3d)
            # Slight yaw correction on R foot in f2..f8 to keep twist within stock loop range
            q_f_yaw = unreal.Quat(0.0, 0.0, math.sin(math.radians(8.0 * e)), math.cos(math.radians(8.0 * e))) if s == "r" else unreal.Quat(0.0, 0.0, 0.0, 1.0)
            q_fo = q_f_yaw * w[fo].rotation
            w_air = foot_air_w[s][f]
            if w_air > 1e-4:
                q_fo = qnlerp(q_fo, w0[fo].rotation, 0.35 * w_air)
            nw[fo] = mk_tf(foot_new, q_fo, w[fo].scale3d)

    # Convert modified bones to parent-local space
    # For neck_01 and clavicles, parent is spine_05 (rotated by q_sp with spine_01)
    sp05_old = unreal.AnimPoseExtensions.get_bone_pose(pose, "spine_05", unreal.AnimPoseSpaces.WORLD)
    if e <= 1e-6 and es <= 1e-6:
        sp05_new = sp05_old
    else:
        sp05_off = vsub(sp05_old.translation, w["spine_01"].translation)
        sp05_new = mk_tf(vadd(nw["spine_01"].translation, q_sp.rotate_vector(sp05_off)), q_sp * sp05_old.rotation, sp05_old.scale3d)
        # 5. Synchronize arms & shoulders to the forward fall
        hd = hd_sig[f]
        for s, sign in (("l", 1.0), ("r", -1.0)):
            cl, ua, la, ha = "clavicle_" + s, "upperarm_" + s, "lowerarm_" + s, "hand_" + s
            w_fwd = max(0.0, hd if s == "l" else -hd)
            w_rear = max(0.0, -hd if s == "l" else hd)
            # Clavicle protraction/retraction about Z
            cl_off = vsub(w[cl].translation, w["spine_01"].translation)
            cl_pos = vadd(nw["spine_01"].translation, q_sp.rotate_vector(cl_off))
            ua_dir0 = q_sp.rotate_vector(vsub(w[ua].translation, w[cl].translation))
            q_cl_pos = yaw_quat(CLAV_SWING * e * (w_fwd - w_rear))
            q_cl = q_cl_pos if (q_cl_pos.rotate_vector(ua_dir0).y - ua_dir0.y) * (w_fwd - w_rear) >= 0 else qinv(q_cl_pos)
            nw[cl] = mk_tf(cl_pos, q_cl * q_sp * w[cl].rotation, w[cl].scale3d)

            # Upperarm: forward swing + slight outward open (lead), or back & up swing (rear)
            ua_pos = vadd(cl_pos, q_cl.rotate_vector(ua_dir0))
            la_dir0 = (q_cl * q_sp).rotate_vector(vsub(w[la].translation, w[ua].translation))
            q_sh_fwd = pitch_quat(ARM_FWD_SH * e * w_fwd, la_dir0)
            rb = math.radians(ARM_BACK_SH * e * w_rear)
            q_bp = unreal.Quat(math.sin(0.5 * rb), 0.0, 0.0, math.cos(0.5 * rb))
            q_bn = qinv(q_bp)
            vp, vn = q_bp.rotate_vector(la_dir0), q_bn.rotate_vector(la_dir0)
            q_sh_back = q_bp if (vp.z - vp.y) >= (vn.z - vn.y) else q_bn
            el_out_env = ss(1.0, 5.0, float(f)) * (1.0 - ss(13.0, 21.0, float(f)))
            arm_out_deg = ARM_FWD_OUT * e * w_fwd + (ARM_R_OUT * el_out_env if s == "r" else ARM_L_OUT * el_out_env)
            q_out_pos = yaw_quat(arm_out_deg)
            # Outward = same sign as lat_vec (L - R)
            q_out = q_out_pos if vdot(vsub(q_out_pos.rotate_vector(la_dir0), la_dir0), vmul(lat_vec, sign)) >= 0 else qinv(q_out_pos)
            # Also roll upperarm outward about +Y (front axis) so elbow/forearm clear the flank in back view
            ra = math.radians((ARM_R_OUT if s == "r" else ARM_L_OUT) * el_out_env)
            qp_a = unreal.Quat(0.0, math.sin(0.5 * ra), 0.0, math.cos(0.5 * ra))
            q_r_abd = qp_a if vdot(vsub(qp_a.rotate_vector(la_dir0), la_dir0), vmul(lat_vec, sign)) >= 0 else qinv(qp_a)
            # Extra forward pitch on arm_l at f13 (v107)
            q_l_fwd13 = unreal.Quat(0.0, 0.0, 0.0, 1.0)
            if s == "l":
                e_f13 = ss(8.0, 13.0, float(f)) * (1.0 - ss(13.0, 21.0, float(f)))
                if e_f13 > 1e-4:
                    q_l_fwd13 = pitch_quat(ARM_L_FWD13 * e_f13, la_dir0)
            q_ua = q_l_fwd13 * q_r_abd * q_out * q_sh_fwd * q_sh_back
            nw[ua] = mk_tf(ua_pos, q_ua * q_cl * q_sp * w[ua].rotation, w[ua].scale3d)

            # Lowerarm: keep lead elbow bent (~80-90 deg); increase flexion on rear elbow; unhook arm_l at f5 + forward arc at f13
            la_pos = vadd(ua_pos, q_ua.rotate_vector(la_dir0))
            ha_dir0 = (q_ua * q_cl * q_sp).rotate_vector(vsub(w[ha].translation, w[la].translation))
            q_la_fwd = pitch_quat(ARM_FWD_EL * e * w_fwd, ha_dir0) if ARM_FWD_EL else unreal.Quat(0.0, 0.0, 0.0, 1.0)
            rf_deg = ARM_BACK_FL * e * w_rear + (ARM_R_FL5 * ss(1.0, 5.0, float(f)) * (1.0 - ss(7.0, 13.0, float(f))) if s == "r" else 0.0)
            if rf_deg > 1e-3:
                rf = math.radians(rf_deg)
                # Bend around the true anatomical elbow hinge axis (perpendicular to upperarm & forearm)
                ax = unreal.Vector(la_dir0.y * ha_dir0.z - la_dir0.z * ha_dir0.y,
                                   la_dir0.z * ha_dir0.x - la_dir0.x * ha_dir0.z,
                                   la_dir0.x * ha_dir0.y - la_dir0.y * ha_dir0.x)
                ax = vnorm(ax) if vlen(ax) > 1e-3 else unreal.Vector(1.0, 0.0, 0.0)
                s_h, c_h = math.sin(0.5 * rf), math.cos(0.5 * rf)
                q_fp = unreal.Quat(ax.x * s_h, ax.y * s_h, ax.z * s_h, c_h)
                q_fn = qinv(q_fp)
                # Closing the elbow increases dot(-la_dir0, ha_dir_new)
                q_la_back = q_fp if vdot(vmul(la_dir0, -1.0), q_fp.rotate_vector(ha_dir0)) >= vdot(vmul(la_dir0, -1.0), q_fn.rotate_vector(ha_dir0)) else q_fn
            else:
                q_la_back = unreal.Quat(0.0, 0.0, 0.0, 1.0)
            q_la_l = unreal.Quat(0.0, 0.0, 0.0, 1.0)
            if s == "l":
                e_hook = ss(1.0, 5.0, float(f)) * (1.0 - ss(7.0, 12.0, float(f)))
                if e_hook > 1e-4:
                    qh_p = yaw_quat(ARM_L_FORE_OUT * e_hook)
                    q_la_l = qh_p if vdot(vsub(qh_p.rotate_vector(ha_dir0), ha_dir0), lat_vec) >= 0 else qinv(qh_p)
                e_f13 = ss(8.0, 13.0, float(f)) * (1.0 - ss(13.0, 21.0, float(f)))
                if e_f13 > 1e-4:
                    q_la_l = pitch_quat(0.5 * ARM_L_FWD13 * e_f13, ha_dir0) * q_la_l
            q_la = q_la_l * q_la_fwd * q_la_back
            nw[la] = mk_tf(la_pos, q_la * q_ua * q_cl * q_sp * w[la].rotation, w[la].scale3d)

    parents = {
        "pelvis": "root",
        "spine_01": "pelvis",
        "neck_01": "spine_05",
        "clavicle_l": "spine_05", "upperarm_l": "clavicle_l", "lowerarm_l": "upperarm_l",
        "clavicle_r": "spine_05", "upperarm_r": "clavicle_r", "lowerarm_r": "upperarm_r",
        "thigh_l": "pelvis", "calf_l": "thigh_l", "foot_l": "calf_l", "ik_foot_l": "ik_foot_root",
        "thigh_r": "pelvis", "calf_r": "thigh_r", "foot_r": "calf_r", "ik_foot_r": "ik_foot_root",
    }

    for b, par in parents.items():
        if b.startswith("ik_foot_"):
            src_fo = "foot_" + b[-1]
            loc = nw[src_fo].make_relative(nw["ik_foot_root"])
        elif par == "spine_05":
            loc = nw[b].make_relative(sp05_new)
        else:
            loc = nw[b].make_relative(nw[par])
        tracks[b][0].append(loc.translation)
        tracks[b][1].append(loc.rotation)
        tracks[b][2].append(loc.scale3d)

rep = {"frames": n, "max_pelvis_dy": round(max_dy, 2), "saved": False}
if SAVE:
    ctrl = seq.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("runstart w3 additive"), False)
    for b, (pos, rot, scl) in tracks.items():
        ctrl.set_bone_track_keys(b, pos, rot, scl, False)
    # Release useless toe curl on ball_l/ball_r toward neutral f0 ONLY when in flight/recovery (keep stance/push-off intact)
    for s_ in ("l", "r"):
        b = "ball_" + s_
        tfs = [AL.get_bone_pose_for_frame(seq, b, f, False) for f in range(n)]
        q0 = tfs[0].rotation
        rots = [qnlerp(t.rotation, q0, 0.9 * foot_air_w[s_][f]) for f, t in enumerate(tfs)]
        ctrl.set_bone_track_keys(b, [t.translation for t in tfs], rots, [t.scale3d for t in tfs], False)
    # Smooth clavicle + arm local rotations in f1..f18 (4 passes, fading out f12..f18) to keep jitter < 8.0 cm
    for b in ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"):
        tfs = [AL.get_bone_pose_for_frame(seq, b, f, False) for f in range(n)]
        rots = [t.rotation for t in tfs]
        for _ in range(4):
            nr = list(rots)
            for f in range(1, min(19, n - 1)):
                w_sm = 0.5 * (1.0 - ss(12.0, 19.0, float(f)))
                q_mid = qnlerp(rots[f - 1], rots[f + 1], 0.5)
                nr[f] = qnlerp(rots[f], q_mid, w_sm)
            rots = nr
        if b.startswith(("lowerarm_", "hand_")) and ARM_DRAG > 0.0:
            dr = list(rots)
            for f in range(1, min(25, n)):
                dr[f] = qnlerp(rots[f], rots[f - 1], ARM_DRAG * env(f))
            rots = dr
        ctrl.set_bone_track_keys(b, [t.translation for t in tfs], rots, [t.scale3d for t in tfs], False)
    ctrl.close_bracket(False)
    rep["saved"] = bool(EAL.save_asset(DST, only_if_is_dirty=False))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(rep, open(OUT, "w"), indent=1)
print("W3 %s frames=%d max_dy=%.1f saved=%s" % (DST.split("/")[-1], n, max_dy, rep["saved"]))
