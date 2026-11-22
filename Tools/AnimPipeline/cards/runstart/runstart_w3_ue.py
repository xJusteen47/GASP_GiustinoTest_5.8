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

PELVIS_DY = float(os.environ.get("W3_PELVIS_DY", "9.0"))     # cm forward (+Y)
PELVIS_DZ = float(os.environ.get("W3_PELVIS_DZ", "-1.5"))    # cm vertical
SPINE_PITCH = float(os.environ.get("W3_SPINE_PITCH", "6.0")) # deg forward
KNEE_DROP = float(os.environ.get("W3_KNEE_DROP", "0.35"))    # fraction of swing foot height above 14 cm
SWING_LAG = float(os.environ.get("W3_SWING_LAG", "2.0"))     # cm trailing (-Y) on swing foot


def ss(a, b, x):
    if x <= a:
        return 0.0
    if x >= b:
        return 1.0
    t = (x - a) / float(b - a)
    return t * t * (3.0 - 2.0 * t)


def env(f):
    return ss(0.0, 4.0, float(f)) * (1.0 - ss(12.0, 22.0, float(f)))


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
    return knee_new


def pitch_quat(deg, ref_vec):
    q_pos = unreal.Quat(unreal.Vector(1.0, 0.0, 0.0), math.radians(deg))
    q_neg = unreal.Quat(unreal.Vector(1.0, 0.0, 0.0), math.radians(-deg))
    if q_pos.rotate_vector(ref_vec).y >= q_neg.rotate_vector(ref_vec).y:
        return q_pos
    return q_neg


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
                "thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r"]

tracks = {b: ([], [], []) for b in ["pelvis", "spine_01", "neck_01",
                                     "thigh_l", "calf_l", "foot_l", "ik_foot_l",
                                     "thigh_r", "calf_r", "foot_r", "ik_foot_r"]}

max_dy = 0.0
for f in range(n):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, f / 30.0, opts)
    w = {b: unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD) for b in bones_needed}
    e = env(f)
    if e <= 1e-6:
        nw = dict(w)
    else:
        nw = {}
        nw["root"] = w["root"]
        nw["ik_foot_root"] = w["ik_foot_root"]
        # 1. Pelvis forward shift + slight sink
        dy = PELVIS_DY * e
        dz = PELVIS_DZ * e
        max_dy = max(max_dy, dy)
        p_tf = mk_tf(vadd(w["pelvis"].translation, unreal.Vector(0.0, dy, dz)), w["pelvis"].rotation, w["pelvis"].scale3d)
        nw["pelvis"] = p_tf

        # 2. Spine_01 forward pitch (about +X, increasing +Y of neck-spine)
        sp_pos = vadd(w["spine_01"].translation, unreal.Vector(0.0, dy, dz))
        q_sp = pitch_quat(SPINE_PITCH * e, vsub(w["neck_01"].translation, w["spine_01"].translation))
        nw["spine_01"] = mk_tf(sp_pos, q_sp * w["spine_01"].rotation, w["spine_01"].scale3d)

        # 3. Neck_01 counter-pitch 50% to keep gaze forward
        nk_off = vsub(w["neck_01"].translation, w["spine_01"].translation)
        nk_pos = vadd(sp_pos, q_sp.rotate_vector(nk_off))
        q_nk = pitch_quat(-0.5 * SPINE_PITCH * e, unreal.Vector(0.0, 0.0, 10.0))
        nw["neck_01"] = mk_tf(nk_pos, q_nk * q_sp * w["neck_01"].rotation, w["neck_01"].scale3d)

        # 4. Legs 2-bone IK
        for s in ("l", "r"):
            th, ca, fo = "thigh_" + s, "calf_" + s, "foot_" + s
            hip_old = w[th].translation
            knee_old = w[ca].translation
            foot_old = w[fo].translation
            hip_new = vadd(hip_old, unreal.Vector(0.0, dy, dz))
            fz = foot_old.z
            if fz > 15.0:
                fz_new = fz - KNEE_DROP * (fz - 14.0) * e
                fy_new = foot_old.y - SWING_LAG * e
            else:
                fz_new = fz
                fy_new = foot_old.y
            foot_new = unreal.Vector(foot_old.x, fy_new, fz_new)
            knee_new = solve_2bone_ik(hip_new, foot_new, hip_old, knee_old, foot_old)

            q_th_delta = unreal.Quat.find_between_vectors(vnorm(vsub(knee_old, hip_old)), vnorm(vsub(knee_new, hip_new)))
            q_ca_delta = unreal.Quat.find_between_vectors(vnorm(vsub(foot_old, knee_old)), vnorm(vsub(foot_new, knee_new)))

            nw[th] = mk_tf(hip_new, q_th_delta * w[th].rotation, w[th].scale3d)
            nw[ca] = mk_tf(knee_new, q_ca_delta * w[ca].rotation, w[ca].scale3d)
            nw[fo] = mk_tf(foot_new, w[fo].rotation, w[fo].scale3d)

    # Convert modified bones to parent-local space
    parents = {
        "pelvis": "root",
        "spine_01": "pelvis",
        "neck_01": "spine_01",  # note: spine_02..05 are untouched in local space, wait!
        "thigh_l": "pelvis", "calf_l": "thigh_l", "foot_l": "calf_l", "ik_foot_l": "ik_foot_root",
        "thigh_r": "pelvis", "calf_r": "thigh_r", "foot_r": "calf_r", "ik_foot_r": "ik_foot_root",
    }
    # For neck_01, its actual parent is spine_05 (which rotated by q_sp with spine_01!).
    # So we only need pelvis, spine_01, and legs; neck_01's parent is spine_05, so we compute neck_01 relative to spine_05_new!
    sp05_old = unreal.AnimPoseExtensions.get_bone_pose(pose, "spine_05", unreal.AnimPoseSpaces.WORLD)
    if e <= 1e-6:
        sp05_new = sp05_old
    else:
        sp05_off = vsub(sp05_old.translation, w["spine_01"].translation)
        sp05_new = mk_tf(vadd(nw["spine_01"].translation, q_sp.rotate_vector(sp05_off)), q_sp * sp05_old.rotation, sp05_old.scale3d)

    for b, par in parents.items():
        if b.startswith("ik_foot_"):
            src_fo = "foot_" + b[-1]
            loc = nw[src_fo].make_relative(nw["ik_foot_root"])
        elif b == "neck_01":
            loc = nw["neck_01"].make_relative(sp05_new)
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
    ctrl.close_bracket(False)
    rep["saved"] = bool(EAL.save_asset(DST, only_if_is_dirty=False))

os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(rep, open(OUT, "w"), indent=1)
print("W3 %s frames=%d max_dy=%.1f saved=%s" % (DST.split("/")[-1], n, max_dy, rep["saved"]))
