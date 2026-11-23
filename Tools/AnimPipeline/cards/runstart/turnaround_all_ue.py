"""Turnaround 180R / 090L / 090R: 180L-c3 (29w3) baseline, strict w2t<TURN>p source.
Env: TURN_LIST=180R,090L,090R (default), GSTN_SAVE=1 to save assets,
TURN_OVERRIDE_JSON='{"*": {...}, "180R": {...}}' for per-turn param overrides.
Reads clean AS_GSTN_RunStart_Generic_w2t<TURN>p and writes AS_GSTN_RunStart_Generic_w2t<TURN>s."""
import json
import math
import os
import sys
from pathlib import Path

import unreal

_mcp_dir = None
_env_mcp = os.environ.get("GSTN_MCP_DIR")
if _env_mcp:
    _p = Path(_env_mcp)
    if (_p / "geom_intent_ue.py").is_file():
        _mcp_dir = _p
if _mcp_dir is None and "GSTN_DUMP_MIRROR" in os.environ:
    _p = Path(os.environ["GSTN_DUMP_MIRROR"]).resolve().parents[2] / "Tools" / "AnimPipeline" / "gstn_anim_mcp"
    if (_p / "geom_intent_ue.py").is_file():
        _mcp_dir = _p
if _mcp_dir is None:
    _p = Path(__file__).resolve().parents[2] / "gstn_anim_mcp"
    if (_p / "geom_intent_ue.py").is_file():
        _mcp_dir = _p
if _mcp_dir is not None:
    sys.path.insert(0, str(_mcp_dir))

from geom_intent_ue import (  # noqa: E402
    axis_quat,
    compute_envelopes,
    qinv,
    qnlerp,
    signed_axis_quat_directed,
    solve_2bone_ik,
    ss,
    tuck_in_quat,
    vadd,
    vcross,
    vdot,
    vlen,
    vmul,
    vnorm,
    vsub,
)

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SAVE = os.environ.get("GSTN_SAVE") == "1" or os.environ.get("CLIPFIX_WRITE") == "1"
TURNS = [t.strip() for t in os.environ.get("TURN_LIST", "180R,090L,090R").split(",") if t.strip()]

TURN_PARAMS = {
    "180R": {
        "pelvis_fwd": 6.0, "pelvis_drop": -1.5, "spine_lean_pivot": 10.0, "spine_lean_run": 5.0,
        "torso_bank": 5.0, "head_lift": 6.5, "knee_drop": 0.04, "swing_lag": 2.0,
        "catch_fwd": 9.0, "step_narrow": 0.5, "pivot_heel": 6.0,
        "clav_st_elev": 7.0, "clav_pv_elev": 3.0, "clav_open": 6.0,
        "arm_st_tuck_in": 14.0, "arm_pv_out": 3.0, "arm_st_back": 24.0,
        "arm_st_elbow": 18.0, "arm_pv_fwd": 20.0, "arm_pv_elbow": 24.0, "arm_clr": 10.0,
    },
    "090L": {
        "pelvis_fwd": 4.5, "pelvis_drop": -1.5, "spine_lean_pivot": 8.0, "spine_lean_run": 5.0,
        "torso_bank": 4.0, "head_lift": 6.5, "knee_drop": 0.04, "swing_lag": 2.0,
        "catch_fwd": 5.5, "step_narrow": 2.5, "pivot_heel": 10.0,
        "clav_st_elev": 7.0, "clav_pv_elev": 3.0, "clav_open": 6.0,
        "arm_st_tuck_in": 16.0, "arm_pv_out": 3.0, "arm_st_back": 20.0,
        "arm_st_elbow": 20.0, "arm_pv_fwd": 22.0, "arm_pv_elbow": 24.0, "arm_clr": 8.0,
    },
    "090R": {
        "pelvis_fwd": 6.0, "pelvis_drop": -1.5, "spine_lean_pivot": 8.0, "spine_lean_run": 5.0,
        "torso_bank": 4.0, "head_lift": 6.5, "knee_drop": 0.04, "swing_lag": 2.0,
        "catch_fwd": 9.0, "step_narrow": 2.5, "pivot_heel": 10.0,
        "clav_st_elev": 7.0, "clav_pv_elev": 3.0, "clav_open": 6.0,
        "arm_st_tuck_in": 16.0, "arm_pv_out": 3.0, "arm_st_back": 22.0,
        "arm_st_elbow": 20.0, "arm_pv_fwd": 22.0, "arm_pv_elbow": 24.0, "arm_clr": 8.0,
    },
}

_override_json = os.environ.get("TURN_OVERRIDE_JSON")
if _override_json:
    _overrides = json.loads(_override_json)
    _global = _overrides.get("*", {})
    for _t, _params in TURN_PARAMS.items():
        _params.update(_global)
        _params.update(_overrides.get(_t, {}))


def mk_tf(pos, quat, scl):
    return unreal.Transform(location=pos, rotation=quat.rotator(), scale=scl)


def process_turn(turn):
    p = TURN_PARAMS[turn]
    PELVIS_FWD_CM = p["pelvis_fwd"]
    PELVIS_DROP_CM = p["pelvis_drop"]
    SPINE_LEAN_PIVOT_DEG = p["spine_lean_pivot"]
    SPINE_LEAN_RUN_DEG = p["spine_lean_run"]
    TORSO_BANK_DEG = p["torso_bank"]
    HEAD_LIFT_DEG = p["head_lift"]
    KNEE_DROP = p["knee_drop"]
    SWING_LAG_CM = p["swing_lag"]
    CATCH_FWD_CM = p["catch_fwd"]
    STEP_NARROW_CM = p["step_narrow"]
    PIVOT_HEEL_DEG = p["pivot_heel"]
    CLAV_ST_ELEV_DEG = p["clav_st_elev"]
    CLAV_PV_ELEV_DEG = p["clav_pv_elev"]
    CLAV_OPEN_DEG = p["clav_open"]
    ARM_ST_TUCK_IN_DEG = p["arm_st_tuck_in"]
    ARM_PV_OUT_DEG = p["arm_pv_out"]
    ARM_ST_BACK_DEG = p["arm_st_back"]
    ARM_ST_ELBOW_DEG = p["arm_st_elbow"]
    ARM_PV_FWD_DEG = p["arm_pv_fwd"]
    ARM_PV_ELBOW_DEG = p["arm_pv_elbow"]
    ARM_CLR_DEG = p["arm_clr"]

    deg = int(turn[:3])
    side = turn[3]
    st, pv = ("l", "r") if side == "L" else ("r", "l")
    bank_sign = 1.0 if side == "L" else -1.0  # inward roll toward turn side (+lat for L, -lat for R)
    piv_scale = 0.8 if deg == 90 else 1.0     # 90-deg turn pivots ~1-2f faster

    src = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t%sp" % turn
    if not EAL.does_asset_exist(src):
        raise RuntimeError("Missing required base27 asset: %s (w2t*f fallback forbidden)" % src)
    dst = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t%ss" % turn

    if SAVE:
        if EAL.does_asset_exist(dst):
            EAL.delete_asset(dst)
        if not EAL.duplicate_asset(src, dst):
            raise RuntimeError("Failed to duplicate %s -> %s" % (src, dst))
        seq = unreal.load_asset(dst)
    else:
        seq = unreal.load_asset(src)
    if not seq:
        raise RuntimeError("Failed to load %s" % src)

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
        fwd_vecs.append(vnorm(vcross(up_vec, lat_vec)))

    ref_fwd = run_dir
    for f in range(n - 1, -1, -1):
        if vdot(fwd_vecs[f], ref_fwd) < 0.0:
            fwd_vecs[f] = vmul(fwd_vecs[f], -1.0)
        ref_fwd = fwd_vecs[f]

    pel_z_sm = [world_frames[f]["pelvis"].translation.z for f in range(n)]
    for _ in range(4):
        nz = list(pel_z_sm)
        for f in range(2, min(27, n - 1)):
            w_sm = 0.55 * ss(1.0, 4.0, float(f)) * (1.0 - ss(22.0, 27.0, float(f)))
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

        env = compute_envelopes(f, n, turn_deg=float(deg), turn_side=side, start_from_frame=2)

        if f >= 2:
            pel_pos = vadd(w["pelvis"].translation, vmul(fwd_vec, PELVIS_FWD_CM * env["w3"]))
            pel_pos.z = pel_z_sm[f] + PELVIS_DROP_CM * env["w3"]
            nw["pelvis"] = mk_tf(pel_pos, w["pelvis"].rotation, w["pelvis"].scale3d)

            q_sp_cum = unreal.Quat(0.0, 0.0, 0.0, 1.0)
            sp_weights = (("spine_01", 0.40), ("spine_02", 0.35), ("spine_03", 0.25))
            sp_par = {"spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02"}
            lean_deg = SPINE_LEAN_PIVOT_DEG * env["piv_lean"] + SPINE_LEAN_RUN_DEG * env["run_lean"]
            for sp, wt in sp_weights:
                par = sp_par[sp]
                pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
                q_bank = signed_axis_quat_directed(fwd_vec, TORSO_BANK_DEG * wt * env["bank"], up_vec, inward_dir)
                q_lean = signed_axis_quat_directed(lat_vec, lean_deg * wt, up_vec, fwd_vec)
                q_sp_cum = (q_lean * q_bank) * q_sp_cum
                nw[sp] = mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

            for sp, par in (("spine_04", "spine_03"), ("spine_05", "spine_04")):
                pos_sp = vadd(nw[par].translation, q_sp_cum.rotate_vector(vsub(w[sp].translation, w[par].translation)))
                nw[sp] = mk_tf(pos_sp, q_sp_cum * w[sp].rotation, w[sp].scale3d)

            nk_pos = vadd(nw["spine_05"].translation, q_sp_cum.rotate_vector(vsub(w["neck_01"].translation, w["spine_05"].translation)))
            q_nk = signed_axis_quat_directed(lat_vec, HEAD_LIFT_DEG * env["head"], fwd_vec, up_vec)
            nw["neck_01"] = mk_tf(nk_pos, q_nk * q_sp_cum * w["neck_01"].rotation, w["neck_01"].scale3d)

            for s in ("l", "r"):
                th, ca, fo = "thigh_" + s, "calf_" + s, "foot_" + s
                hip_old, knee_old, foot_old = w[th].translation, w[ca].translation, w[fo].translation
                hip_new = vadd(nw["pelvis"].translation, vsub(hip_old, w["pelvis"].translation))
                w_air = foot_air_w[s][f]
                foot_tgt = unreal.Vector(foot_old.x, foot_old.y, foot_old.z)
                if s == st and env["piv_foot"] > 1e-4:
                    foot_tgt = vadd(foot_tgt, vmul(lat_vec, (-1.0 if st == "l" else 1.0) * STEP_NARROW_CM * env["piv_foot"]))
                if w_air > 1e-4:
                    excess_z = max(0.0, foot_old.z - 14.0)
                    foot_tgt.z = foot_old.z - KNEE_DROP * excess_z * env["w3"] * w_air
                    foot_tgt = vadd(foot_tgt, vmul(fwd_vec, (-SWING_LAG_CM * env["lag"] + CATCH_FWD_CM * env["catch"]) * w_air))
                knee_new, foot_new = solve_2bone_ik(hip_new, foot_tgt, hip_old, knee_old, foot_old)
                q_th = axis_quat(vcross(vsub(knee_old, hip_old), vsub(knee_new, hip_new)),
                                 math.degrees(math.acos(max(-1.0, min(1.0, vdot(vnorm(vsub(knee_old, hip_old)), vnorm(vsub(knee_new, hip_new)))))))) \
                    if vlen(vcross(vsub(knee_old, hip_old), vsub(knee_new, hip_new))) > 1e-4 else unreal.Quat(0.0, 0.0, 0.0, 1.0)
                q_ca = axis_quat(vcross(vsub(foot_old, knee_old), vsub(foot_new, knee_new)),
                                 math.degrees(math.acos(max(-1.0, min(1.0, vdot(vnorm(vsub(foot_old, knee_old)), vnorm(vsub(foot_new, knee_new)))))))) \
                    if vlen(vcross(vsub(foot_old, knee_old), vsub(foot_new, knee_new))) > 1e-4 else unreal.Quat(0.0, 0.0, 0.0, 1.0)
                q_fo_push = unreal.Quat(0.0, 0.0, 0.0, 1.0)
                if s == pv and env["piv_foot"] > 1e-4:
                    ball_dir = vsub(w["ball_" + pv].translation, foot_old)
                    bd_flat = unreal.Vector(ball_dir.x, ball_dir.y, 0.0)
                    ax_foot = vnorm(vcross(bd_flat, up_vec)) if vlen(bd_flat) > 1e-3 else lat_vec
                    q_fo_push = signed_axis_quat_directed(ax_foot, PIVOT_HEEL_DEG * env["piv_foot"], ball_dir, vmul(up_vec, -1.0))
                nw[th] = mk_tf(hip_new, q_th * w[th].rotation, w[th].scale3d)
                nw[ca] = mk_tf(knee_new, q_ca * w[ca].rotation, w[ca].scale3d)
                nw[fo] = mk_tf(foot_new, q_fo_push * w[fo].rotation, w[fo].scale3d)

            for s, sign in (("l", 1.0), ("r", -1.0)):
                cl, ua, la, ha = "clavicle_" + s, "upperarm_" + s, "lowerarm_" + s, "hand_" + s
                out_dir = vmul(lat_vec, sign)
                clav_elev = CLAV_ST_ELEV_DEG if s == st else CLAV_PV_ELEV_DEG

                cl_pos = vadd(nw["spine_05"].translation, q_sp_cum.rotate_vector(vsub(w[cl].translation, w["spine_05"].translation)))
                ua_dir0 = q_sp_cum.rotate_vector(vsub(w[ua].translation, w[cl].translation))
                q_cl_elev = signed_axis_quat_directed(fwd_vec, clav_elev * env["sh"], ua_dir0, up_vec)
                q_cl_open = signed_axis_quat_directed(up_vec, CLAV_OPEN_DEG * env["sh"], ua_dir0, vmul(fwd_vec, -1.0))
                q_cl = q_cl_open * q_cl_elev
                nw[cl] = mk_tf(cl_pos, q_cl * q_sp_cum * w[cl].rotation, w[cl].scale3d)

                ua_pos = vadd(cl_pos, q_cl.rotate_vector(ua_dir0))
                la_dir0 = (q_cl * q_sp_cum).rotate_vector(vsub(w[la].translation, w[ua].translation))
                q_ua_lat = unreal.Quat(0.0, 0.0, 0.0, 1.0)
                q_ua_swing = unreal.Quat(0.0, 0.0, 0.0, 1.0)
                if s == st:
                    q_ua_lat = tuck_in_quat(fwd_vec, out_dir, up_vec, la_dir0, ARM_ST_TUCK_IN_DEG * env["arm_st_back"])
                    la_dir1 = q_ua_lat.rotate_vector(la_dir0)
                    q_ua_swing = signed_axis_quat_directed(lat_vec, ARM_ST_BACK_DEG * env["arm_st_back"], la_dir1, vmul(fwd_vec, -1.0))
                    if env["arm_clr"] > 1e-4:
                        q_ua_lat = signed_axis_quat_directed(fwd_vec, ARM_CLR_DEG * env["arm_clr"], la_dir0, out_dir) * q_ua_lat
                else:
                    q_ua_lat = signed_axis_quat_directed(fwd_vec, ARM_PV_OUT_DEG * env["sh"], la_dir0, out_dir)
                    la_dir1 = q_ua_lat.rotate_vector(la_dir0)
                    if env["arm_pv_fwd"] > 1e-4:
                        q_ua_swing = signed_axis_quat_directed(lat_vec, ARM_PV_FWD_DEG * env["arm_pv_fwd"], la_dir1, fwd_vec)
                q_ua = q_ua_swing * q_ua_lat
                nw[ua] = mk_tf(ua_pos, q_ua * q_cl * q_sp_cum * w[ua].rotation, w[ua].scale3d)

                la_pos = vadd(ua_pos, q_ua.rotate_vector(la_dir0))
                ha_dir0 = (q_ua * q_cl * q_sp_cum).rotate_vector(vsub(w[ha].translation, w[la].translation))
                q_la = unreal.Quat(0.0, 0.0, 0.0, 1.0)
                el_deg = (ARM_ST_ELBOW_DEG * env["arm_st_back"]) if s == st else (ARM_PV_ELBOW_DEG * env["arm_pv_fwd"])
                if el_deg > 1e-3:
                    la_dir_rot = q_ua.rotate_vector(la_dir0)
                    hinge = vcross(la_dir_rot, ha_dir0)
                    ax = vnorm(hinge) if vlen(hinge) > 1e-3 else lat_vec
                    qp = axis_quat(ax, el_deg)
                    qn = qinv(qp)
                    q_la = qp if vdot(vmul(la_dir_rot, -1.0), qp.rotate_vector(ha_dir0)) >= vdot(vmul(la_dir_rot, -1.0), qn.rotate_vector(ha_dir0)) else qn
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

    # 5-pass arm smoothing strictly over f2..11 (fixes f6..7 jitter while keeping f15..16 peak whip >=85% for --conserve)
    for b in ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"):
        pos, rot, scl = tracks_out[b]
        for _ in range(5):
            nr = list(rot)
            for f in range(2, min(12, n - 1)):
                w_sm = 0.5 * ss(1.0, 3.0, float(f)) * (1.0 - ss(8.0, 12.0, float(f)))
                nr[f] = qnlerp(rot[f], qnlerp(rot[f - 1], rot[f + 1], 0.5), w_sm)
            rot = nr
        tracks_out[b] = (pos, rot, scl)

    saved = False
    if SAVE:
        ctrl = seq.get_editor_property("controller")
        ctrl.open_bracket(unreal.Text("turnaround %s agile Link-OoT" % turn), False)
        for b, (pos, rot, scl) in tracks_out.items():
            if not ctrl.set_bone_track_keys(b, pos, rot, scl, False):
                raise RuntimeError("set_bone_track_keys failed for %s (%s)" % (b, turn))
        ctrl.close_bracket(False)
        saved = bool(EAL.save_asset(dst, only_if_is_dirty=False))

    build_path = Path(PROJ) / "Saved" / "AnimPipeline" / "runstart" / ("AS_GSTN_RunStart_Generic_w2t%ss_build.json" % turn)
    build_path.parent.mkdir(parents=True, exist_ok=True)
    with open(str(build_path), "w") as fh:
        json.dump({"turn": turn, "src": src, "dst": dst, "frames": n, "saved": saved, "params": p}, fh, indent=2, sort_keys=True)

    print("ESITO: OK %s src=%s dst=%s frames=%d saved=%s" % (turn, src.split("/")[-1], dst.split("/")[-1], n, saved))


for t in TURNS:
    process_turn(t)
