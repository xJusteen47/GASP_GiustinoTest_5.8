"""Pure UE 5.9 headless Base-27 builder for AS_GSTN_RunStart_Generic_w2t{180R,090L,090R}p.

Builds clean Base-27 turnaround AnimSequences directly in UE from:
  - M_Relaxed_Run_Start_F_Lfoot (f0 idle pose for all 88 bones)
  - M_Relaxed_Run_Loop_F (88f, 22f cycle, 12.5 cm/f along +Y, C0=1.25 L / 12.25 R)
  - docs/notes/reviews/runstart-t180a_h1/t180_game_tmap.json
  - AS_GSTN_RunStart_Generic_w2t180Lp template (88 tracks + root lock)

Env:
  BASE27_TURNS=180R,090L,090R (default; also accepts 180L)
  BASE27_TMAP=<path to t180_game_tmap.json>
  GSTN_SAVE=1 or CLIPFIX_WRITE=1 to save assets (dry-run otherwise)
"""
from __future__ import annotations

import json
import math
import os
from pathlib import Path

import unreal

AL = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SAVE = os.environ.get("GSTN_SAVE") == "1" or os.environ.get("CLIPFIX_WRITE") == "1"
TURNS = [t.strip() for t in os.environ.get("BASE27_TURNS", "180R,090L,090R").split(",") if t.strip()]

START_L = "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Start_F_Lfoot"
LOOP_F = "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Loop_F"
REF_TEMPLATE = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2t180Lp"
REF_FALLBACK = "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w2"
FOLDER = "/Game/GSTN/Animation/RunStart"

FOLEY_TRACKS = {"l": "Footstep Left", "r": "Footstep Right"}
FOLEY_FOOT_MAX_CM = 15.0
FOLEY_MIN_GAP_F = 5

# Exact 88-bone UEFN Mannequin parent map (topological order matches track names)
PARENTS: dict[str, str | None] = {
    "root": None,
    "attach": "root",
    "pelvis": "root",
    "spine_01": "pelvis",
    "spine_02": "spine_01",
    "spine_03": "spine_02",
    "spine_04": "spine_03",
    "spine_05": "spine_04",
    "neck_01": "spine_05",
    "neck_02": "neck_01",
    "head": "neck_02",
    "ik_foot_root": "root",
    "ik_foot_l": "ik_foot_root",
    "ik_foot_r": "ik_foot_root",
    "ik_hand_root": "root",
    "ik_hand_gun": "ik_hand_root",
    "ik_hand_l": "ik_hand_gun",
    "ik_hand_r": "ik_hand_gun",
}
for _s in ("l", "r"):
    PARENTS[f"clavicle_{_s}"] = "spine_05"
    PARENTS[f"upperarm_{_s}"] = f"clavicle_{_s}"
    PARENTS[f"lowerarm_{_s}"] = f"upperarm_{_s}"
    PARENTS[f"hand_{_s}"] = f"lowerarm_{_s}"
    PARENTS[f"weapon_{_s}"] = f"hand_{_s}"
    PARENTS[f"lowerarm_twist_01_{_s}"] = f"lowerarm_{_s}"
    PARENTS[f"lowerarm_twist_02_{_s}"] = f"lowerarm_{_s}"
    PARENTS[f"upperarm_twist_01_{_s}"] = f"upperarm_{_s}"
    PARENTS[f"upperarm_twist_02_{_s}"] = f"upperarm_{_s}"
    for _fg in ("index", "middle", "pinky", "ring"):
        PARENTS[f"{_fg}_metacarpal_{_s}"] = f"hand_{_s}"
        PARENTS[f"{_fg}_01_{_s}"] = f"{_fg}_metacarpal_{_s}"
        PARENTS[f"{_fg}_02_{_s}"] = f"{_fg}_01_{_s}"
        PARENTS[f"{_fg}_03_{_s}"] = f"{_fg}_02_{_s}"
    PARENTS[f"thumb_01_{_s}"] = f"hand_{_s}"
    PARENTS[f"thumb_02_{_s}"] = f"thumb_01_{_s}"
    PARENTS[f"thumb_03_{_s}"] = f"thumb_02_{_s}"
    PARENTS[f"thigh_{_s}"] = "pelvis"
    PARENTS[f"calf_{_s}"] = f"thigh_{_s}"
    PARENTS[f"foot_{_s}"] = f"calf_{_s}"
    PARENTS[f"ball_{_s}"] = f"foot_{_s}"
    PARENTS[f"calf_twist_01_{_s}"] = f"calf_{_s}"
    PARENTS[f"calf_twist_02_{_s}"] = f"calf_{_s}"
    PARENTS[f"thigh_twist_01_{_s}"] = f"thigh_{_s}"

UPPER_BONES = (
    "spine_01", "spine_02", "spine_03", "spine_04", "spine_05",
    "neck_01", "neck_02", "head",
)
ARM_GROUP_L = tuple(
    b for b in PARENTS
    if b.endswith("_l") and any(
        b.startswith(p) for p in ("clavicle_", "upperarm_", "lowerarm_", "hand_", "weapon_", "index_", "middle_", "pinky_", "ring_", "thumb_")
    )
)
ARM_GROUP_R = tuple(
    b for b in PARENTS
    if b.endswith("_r") and any(
        b.startswith(p) for p in ("clavicle_", "upperarm_", "lowerarm_", "hand_", "weapon_", "index_", "middle_", "pinky_", "ring_", "thumb_")
    )
)
ALL_UPPER_FROM_PELVIS = UPPER_BONES + ARM_GROUP_L + ARM_GROUP_R
FINGER_PREFIXES = ("thumb_", "index_", "middle_", "ring_", "pinky_")
HAND_CHILD_PREFIXES = ("weapon_", "index_", "middle_", "pinky_", "ring_", "thumb_")


def resolve_tmap_path() -> Path:
    candidates: list[Path] = []
    if os.environ.get("BASE27_TMAP"):
        candidates.append(Path(os.environ["BASE27_TMAP"]))
    if os.environ.get("GSTN_DUMP_MIRROR"):
        candidates.append(
            Path(os.environ["GSTN_DUMP_MIRROR"]).resolve().parents[2]
            / "docs" / "notes" / "reviews" / "runstart-t180a_h1" / "t180_game_tmap.json"
        )
    candidates.append(Path(PROJ) / "docs" / "notes" / "reviews" / "runstart-t180a_h1" / "t180_game_tmap.json")
    for p in candidates:
        if p.is_file():
            return p
    raise FileNotFoundError(f"Missing t180_game_tmap.json; checked: {[str(c) for c in candidates]}")


def ss(x: float) -> float:
    x = min(max(float(x), 0.0), 1.0)
    return x * x * (3.0 - 2.0 * x)


def ss2(a: float, b: float, x: float) -> float:
    if x <= a:
        return 0.0
    if x >= b:
        return 1.0
    return ss((x - a) / float(b - a))


def vsub(a: unreal.Vector, b: unreal.Vector) -> unreal.Vector:
    return unreal.Vector(a.x - b.x, a.y - b.y, a.z - b.z)


def vadd(a: unreal.Vector, b: unreal.Vector) -> unreal.Vector:
    return unreal.Vector(a.x + b.x, a.y + b.y, a.z + b.z)


def vmul(a: unreal.Vector, s: float) -> unreal.Vector:
    return unreal.Vector(a.x * s, a.y * s, a.z * s)


def vlerp(a: unreal.Vector, b: unreal.Vector, t: float) -> unreal.Vector:
    return unreal.Vector(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t)


def vlen(a: unreal.Vector) -> float:
    return math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z)


def vnorm(a: unreal.Vector) -> unreal.Vector:
    l = vlen(a)
    return unreal.Vector(a.x / l, a.y / l, a.z / l) if l > 1e-6 else unreal.Vector(0.0, 1.0, 0.0)


def vdot(a: unreal.Vector, b: unreal.Vector) -> float:
    return a.x * b.x + a.y * b.y + a.z * b.z


def vcross(a: unreal.Vector, b: unreal.Vector) -> unreal.Vector:
    return unreal.Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x)


def qinv(q: unreal.Quat) -> unreal.Quat:
    return unreal.Quat(-q.x, -q.y, -q.z, q.w)


def qnlerp(a: unreal.Quat, b: unreal.Quat, t: float) -> unreal.Quat:
    dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w
    s = -1.0 if dot < 0.0 else 1.0
    x = a.x * (1.0 - t) + s * b.x * t
    y = a.y * (1.0 - t) + s * b.y * t
    z = a.z * (1.0 - t) + s * b.z * t
    w = a.w * (1.0 - t) + s * b.w * t
    l = math.sqrt(x * x + y * y + z * z + w * w)
    if l < 1e-12:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    return unreal.Quat(x / l, y / l, z / l, w / l)


def axis_quat(ax: unreal.Vector, deg: float) -> unreal.Quat:
    if abs(deg) < 1e-6:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    axn = vnorm(ax)
    r = math.radians(deg) * 0.5
    s, c = math.sin(r), math.cos(r)
    return unreal.Quat(axn.x * s, axn.y * s, axn.z * s, c)


def signed_axis_quat(ax: unreal.Vector, deg: float, ref_vec: unreal.Vector, target_dir: unreal.Vector) -> unreal.Quat:
    if abs(deg) < 1e-6:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    qp = axis_quat(ax, deg)
    qn = qinv(qp)
    dp = vdot(vsub(qp.rotate_vector(ref_vec), ref_vec), target_dir)
    dn = vdot(vsub(qn.rotate_vector(ref_vec), ref_vec), target_dir)
    return qp if dp >= dn else qn


def align_quat(d0: unreal.Vector, d1: unreal.Vector) -> unreal.Quat:
    u0, u1 = vnorm(d0), vnorm(d1)
    if vlen(d0) < 1e-5 or vlen(d1) < 1e-5:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    dot = max(-1.0, min(1.0, vdot(u0, u1)))
    if dot > 1.0 - 1e-6:
        return unreal.Quat(0.0, 0.0, 0.0, 1.0)
    cr = vcross(u0, u1)
    if vlen(cr) < 1e-5:
        fb = unreal.Vector(0.0, 0.0, 1.0) if abs(u0.z) < 0.9 else unreal.Vector(1.0, 0.0, 0.0)
        return axis_quat(vcross(u0, fb), 180.0)
    ang = math.degrees(math.acos(dot))
    return axis_quat(cr, ang)


def rot_z(v: unreal.Vector, deg: float) -> unreal.Vector:
    r = math.radians(deg)
    c, s = math.cos(r), math.sin(r)
    return unreal.Vector(v.x * c - v.y * s, v.x * s + v.y * c, v.z)


def q_z(deg: float) -> unreal.Quat:
    return unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(deg)).quaternion()


def solve_2bone_ik(
    hip_new: unreal.Vector,
    foot_new: unreal.Vector,
    l1: float,
    l2: float,
    pole_hint: unreal.Vector,
) -> tuple[unreal.Vector, unreal.Vector]:
    d_vec = vsub(foot_new, hip_new)
    d = vlen(d_vec)
    d_clamped = max(abs(l1 - l2) + 0.05, min(l1 + l2 - 0.01, d))
    dir_hf = vnorm(d_vec)
    pole = vsub(pole_hint, vmul(dir_hf, vdot(pole_hint, dir_hf)))
    if vlen(pole) < 1e-4:
        fb = unreal.Vector(0.0, 1.0, 0.0) if abs(dir_hf.y) < 0.95 else unreal.Vector(1.0, 0.0, 0.0)
        pole = vsub(fb, vmul(dir_hf, vdot(fb, dir_hf)))
    pole = vnorm(pole)
    cos_a = max(-1.0, min(1.0, (l1 * l1 + d_clamped * d_clamped - l2 * l2) / max(1e-6, 2.0 * l1 * d_clamped)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    knee_new = vadd(hip_new, vadd(vmul(dir_hf, l1 * cos_a), vmul(pole, l1 * sin_a)))
    foot_out = vadd(hip_new, vmul(dir_hf, d_clamped))
    return knee_new, foot_out


def sample_seq_locals(seq: unreal.AnimSequence, bones: list[str]) -> list[dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]]:
    n = int(seq.get_editor_property("number_of_sampled_frames")) if hasattr(seq, "number_of_sampled_frames") else AL.get_num_frames(seq) + 1
    frames: list[dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]] = []
    for f in range(n):
        d: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]] = {}
        for b in bones:
            tf = AL.get_bone_pose_for_frame(seq, b, f, False)
            d[b] = (tf.translation, tf.rotation, tf.scale3d)
        frames.append(d)
    return frames


def topo_order(bones: list[str]) -> list[str]:
    ordered: list[str] = []
    seen: set[str] = set()
    remaining = list(bones)
    while remaining:
        progressed = False
        nxt: list[str] = []
        for b in remaining:
            p = PARENTS.get(b)
            if p is None or p in seen:
                ordered.append(b)
                seen.add(b)
                progressed = True
            else:
                nxt.append(b)
        if not progressed:
            ordered.extend(nxt)
            break
        remaining = nxt
    return ordered


def fk_from_locals(
    locals_dict: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]],
    order: list[str],
    root_pos: unreal.Vector | None = None,
    root_rot: unreal.Quat | None = None,
) -> dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]:
    world: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]] = {}
    for b in order:
        l_pos, l_rot, l_scl = locals_dict[b]
        p = PARENTS.get(b)
        if p is None:
            world[b] = (
                root_pos if root_pos is not None else l_pos,
                root_rot if root_rot is not None else l_rot,
                l_scl,
            )
        else:
            p_pos, p_rot, _ = world[p]
            w_pos = vadd(p_pos, p_rot.rotate_vector(l_pos))
            w_rot = p_rot * l_rot
            world[b] = (w_pos, w_rot, l_scl)
    return world


def world_to_locals(
    world: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]],
    order: list[str],
) -> dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]:
    loc: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]] = {}
    for b in order:
        w_pos, w_rot, w_scl = world[b]
        p = PARENTS.get(b)
        if p is None:
            loc[b] = (w_pos, w_rot, w_scl)
        else:
            p_pos, p_rot, _ = world[p]
            ip = qinv(p_rot)
            l_pos = ip.rotate_vector(vsub(w_pos, p_pos))
            l_rot = ip * w_rot
            loc[b] = (l_pos, l_rot, w_scl)
    return loc


def rotate_bone_and_descendants(
    world: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]],
    pivot: unreal.Vector,
    q_delta: unreal.Quat,
    bones_subset: tuple[str, ...] | list[str],
) -> None:
    for b in bones_subset:
        if b in world:
            p, r, s = world[b]
            world[b] = (vadd(pivot, q_delta.rotate_vector(vsub(p, pivot))), q_delta * r, s)


def shift_bones(
    world: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]],
    delta: unreal.Vector,
    bones_subset: tuple[str, ...] | list[str],
) -> None:
    for b in bones_subset:
        if b in world:
            p, r, s = world[b]
            world[b] = (vadd(p, delta), r, s)


def apply_2bone_leg_ik(
    world: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]],
    s: str,
    foot_target: unreal.Vector,
    lt: float,
    lc: float,
    pole_hint: unreal.Vector,
    keep_foot_rot: unreal.Quat | None = None,
) -> None:
    th, ca, fo, ba = f"thigh_{s}", f"calf_{s}", f"foot_{s}", f"ball_{s}"
    h_pos, h_rot, h_scl = world[th]
    k_old, k_rot, k_scl = world[ca]
    f_old, f_rot, f_scl = world[fo]
    b_old, b_rot, b_scl = world[ba]

    k_new, f_new = solve_2bone_ik(h_pos, foot_target, lt, lc, pole_hint)
    q_th = align_quat(vsub(k_old, h_pos), vsub(k_new, h_pos))
    q_ca = align_quat(vsub(f_old, k_old), vsub(f_new, k_new))

    world[th] = (h_pos, q_th * h_rot, h_scl)
    world[ca] = (k_new, q_ca * k_rot, k_scl)
    if keep_foot_rot is not None:
        df = vsub(f_new, f_old)
        world[fo] = (f_new, keep_foot_rot, f_scl)
        world[ba] = (vadd(b_old, df), b_rot, b_scl)
    else:
        df = vsub(f_new, f_old)
        world[fo] = (f_new, f_rot, f_scl)
        world[ba] = (vadd(b_old, df), b_rot, b_scl)

    for tw in (f"thigh_twist_01_{s}",):
        if tw in world:
            tp, tr, ts = world[tw]
            world[tw] = (vadd(h_pos, q_th.rotate_vector(vsub(tp, h_pos))), q_th * tr, ts)
    for tw in (f"calf_twist_01_{s}", f"calf_twist_02_{s}"):
        if tw in world:
            tp, tr, ts = world[tw]
            world[tw] = (vadd(k_new, q_ca.rotate_vector(vsub(tp, k_old))), q_ca * tr, ts)


def apply_2bone_arm_ik(
    world: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]],
    s: str,
    hand_target: unreal.Vector,
    elbow_hint: unreal.Vector,
    lu: float,
    lf: float,
    soft: float = 0.85,
) -> None:
    ua, la, ha = f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"
    u_pos, u_rot, u_scl = world[ua]
    l_old, l_rot, l_scl = world[la]
    h_old, h_rot, h_scl = world[ha]

    d = vsub(hand_target, u_pos)
    dl = vlen(d)
    ax = vnorm(d)
    reach = lu + lf
    r0 = soft * reach
    dsoft = dl if dl <= r0 else r0 + (reach - r0) * (1.0 - math.exp(-(dl - r0) / max(1e-5, reach - r0)))
    h_tgt_soft = vadd(u_pos, vmul(ax, dsoft))

    pole = vsub(elbow_hint, u_pos)
    l_new, h_new = solve_2bone_ik(u_pos, h_tgt_soft, lu, lf, pole)

    q_ua = align_quat(vsub(l_old, u_pos), vsub(l_new, u_pos))
    q_la = align_quat(vsub(h_old, l_old), vsub(h_new, l_new))

    world[ua] = (u_pos, q_ua * u_rot, u_scl)
    world[la] = (l_new, q_la * l_rot, l_scl)
    dh = vsub(h_new, h_old)
    world[ha] = (h_new, q_la * h_rot, h_scl)

    for b in PARENTS:
        if b.endswith(f"_{s}") and b.startswith(HAND_CHILD_PREFIXES) and b in world:
            cp, cr, cs = world[b]
            world[b] = (vadd(h_new, q_la.rotate_vector(vsub(cp, h_old))), q_la * cr, cs)
    for tw in (f"upperarm_twist_01_{s}", f"upperarm_twist_02_{s}"):
        if tw in world:
            tp, tr, ts = world[tw]
            world[tw] = (vadd(u_pos, q_ua.rotate_vector(vsub(tp, u_pos))), q_ua * tr, ts)
    for tw in (f"lowerarm_twist_01_{s}", f"lowerarm_twist_02_{s}"):
        if tw in world:
            tp, tr, ts = world[tw]
            world[tw] = (vadd(l_new, q_la.rotate_vector(vsub(tp, l_old))), q_la * tr, ts)


def build_rev_phase(
    side: str,
    loop_locals: list[dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]],
    order: list[str],
    tmap: dict,
) -> tuple[
    list[dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]],
    list[float],
    list[float],
    int,
    int,
    dict[str, float],
]:
    nl = len(loop_locals)
    c0 = 1.25 if side == "L" else 12.25
    r0, n_rev, hold, rate_sag = 2.3, 19, 12, 0.15
    tmap_cut, vl, s_min = 19.0, 12.5, 0.15

    def rate(u: float) -> float:
        if u <= hold:
            e = 1.0 - rate_sag * u / float(hold)
        else:
            e = (1.0 - rate_sag) * (1.0 - ss(min((u - hold) / float(n_rev - hold), 1.0)))
        return 1.0 + (r0 - 1.0) * e

    cs = [c0]
    for u in range(1, 100):
        cs.append(cs[-1] + 0.5 * (rate(u - 1) + rate(u)))

    uf = list(tmap["uf"])
    ds = list(tmap["dist"])
    cut = next(k for k, x in enumerate(uf) if x >= tmap_cut)
    uf, ds = uf[: cut + 1], ds[: cut + 1]
    while uf[-1] < 63.0:
        nxt_u = min(uf[-1] + 1.0, 63.0)
        ds.append(ds[-1] + vl * (nxt_u - uf[-1]))
        uf.append(nxt_u)
    nr = len(uf)

    c_list = [cs[int(x)] + (cs[int(x) + 1] - cs[int(x)]) * (x - int(x)) for x in uf]
    spd = [0.0] + [ds[k] - ds[k - 1] for k in range(1, nr)]
    rk = [
        (c_list[min(k + 1, nr - 1)] - c_list[max(k - 1, 0)])
        / float(min(k + 1, nr - 1) - max(k - 1, 0))
        for k in range(nr)
    ]
    s_raw = [min(1.0, max(s_min, spd[k] / max(1e-5, rk[k] * vl))) for k in range(nr)]
    s_sm = [s_raw[0]] + [(s_raw[i - 1] + s_raw[i] + s_raw[i + 1]) / 3.0 for i in range(1, nr - 1)] + [s_raw[-1]]

    loop_worlds = [
        fk_from_locals(loop_locals[f], order, root_pos=unreal.Vector(0.0, 0.0, 0.0), root_rot=unreal.Quat(0.0, 0.0, 0.0, 1.0))
        for f in range(nl)
    ]
    pel_mean_z = sum(loop_worlds[f]["pelvis"][0].z for f in range(nl)) / float(nl)
    arm_mean = {}
    for s in ("l", "r"):
        for pref in ("lowerarm_", "hand_"):
            bn = pref + s
            sh = f"upperarm_{s}"
            acc = unreal.Vector(0.0, 0.0, 0.0)
            for f in range(nl):
                acc = vadd(acc, vsub(loop_worlds[f][bn][0], loop_worlds[f][sh][0]))
            arm_mean[bn] = vmul(acc, 1.0 / float(nl))

    lw0 = loop_worlds[0]
    lengths = {
        "lt_l": vlen(vsub(lw0["calf_l"][0], lw0["thigh_l"][0])),
        "lc_l": vlen(vsub(lw0["foot_l"][0], lw0["calf_l"][0])),
        "lt_r": vlen(vsub(lw0["calf_r"][0], lw0["thigh_r"][0])),
        "lc_r": vlen(vsub(lw0["foot_r"][0], lw0["calf_r"][0])),
        "lu_l": vlen(vsub(lw0["lowerarm_l"][0], lw0["upperarm_l"][0])),
        "lf_l": vlen(vsub(lw0["hand_l"][0], lw0["lowerarm_l"][0])),
        "lu_r": vlen(vsub(lw0["lowerarm_r"][0], lw0["upperarm_r"][0])),
        "lf_r": vlen(vsub(lw0["hand_r"][0], lw0["lowerarm_r"][0])),
    }

    bounce, lean_deg, lean_in, lean_out = 0.3, 24.0, 4.0, 22.0
    arm_scale, arm_back = 1.7, 10.0
    drop_cm, drop_in, drop_hold, drop_out = 12.0, 4.0, 13.0, 22.0
    pin_u, ankle_min, ankle_hold, ankle_out = 19.0, 11.0, 14.0, 20.0

    rev_worlds: list[dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]] = []
    for k in range(nr):
        u = uf[k]
        c = c_list[k]
        a = int(math.floor(c))
        w = c - a
        ia, ib = a % nl, (a + 1) % nl
        la_d, lb_d = loop_locals[ia], loop_locals[ib]
        loc_k = {}
        for b in order:
            pa, ra, sa = la_d[b]
            pb, rb, _ = lb_d[b]
            loc_k[b] = (vlerp(pa, pb, w), qnlerp(ra, rb, w), sa)

        wk = fk_from_locals(
            loc_k,
            order,
            root_pos=unreal.Vector(0.0, ds[k], 0.0),
            root_rot=unreal.Quat(0.0, 0.0, 0.0, 1.0),
        )

        g = (ss(u / lean_in) * ((1.0 - ss((u - hold) / float(lean_out - hold))) if u > hold else 1.0)) if u < lean_out else 0.0
        gd = (ss(u / drop_in) * ((1.0 - ss((u - drop_hold) / float(drop_out - drop_hold))) if u > drop_hold else 1.0)) if u < drop_out else 0.0

        dy = (bounce - 1.0) * g * (wk["pelvis"][0].z - pel_mean_z) - drop_cm * gd
        if abs(dy) > 1e-5:
            shift_bones(wk, unreal.Vector(0.0, 0.0, dy), ("pelvis",) + ALL_UPPER_FROM_PELVIS)
            for s in ("l", "r"):
                for tw in (f"thigh_{s}", f"thigh_twist_01_{s}"):
                    if tw in wk:
                        tp, tr, ts = wk[tw]
                        wk[tw] = (vadd(tp, unreal.Vector(0.0, 0.0, dy)), tr, ts)

        if g > 1e-5:
            amp = 1.0 + (arm_scale - 1.0) * g
            for s in ("l", "r"):
                sh_pos = wk[f"upperarm_{s}"][0]
                la_b, ha_b = f"lowerarm_{s}", f"hand_{s}"
                la_off = vsub(wk[la_b][0], sh_pos)
                ha_off = vsub(wk[ha_b][0], sh_pos)
                la_new = vadd(sh_pos, vadd(arm_mean[la_b], vmul(vsub(la_off, arm_mean[la_b]), amp)))
                ha_new = vadd(sh_pos, vadd(arm_mean[ha_b], vmul(vsub(ha_off, arm_mean[ha_b]), amp)))
                la_new = vadd(la_new, unreal.Vector(0.0, -arm_back * g * 1.0, 0.0))
                ha_new = vadd(ha_new, unreal.Vector(0.0, -arm_back * g * 0.6, 0.0))
                dl_d = vsub(la_new, wk[la_b][0])
                dh_d = vsub(ha_new, wk[ha_b][0])
                wk[la_b] = (la_new, wk[la_b][1], wk[la_b][2])
                wk[ha_b] = (ha_new, wk[ha_b][1], wk[ha_b][2])
                for b in PARENTS:
                    if b.endswith(f"_{s}") and b.startswith(HAND_CHILD_PREFIXES) and b in wk:
                        cp, cr, cs_ = wk[b]
                        wk[b] = (vadd(cp, dh_d), cr, cs_)
                for tw in (f"lowerarm_twist_01_{s}", f"lowerarm_twist_02_{s}"):
                    if tw in wk:
                        tp, tr, ts = wk[tw]
                        wk[tw] = (vadd(tp, dl_d), tr, ts)

            q_lean = signed_axis_quat(unreal.Vector(1.0, 0.0, 0.0), lean_deg * g, unreal.Vector(0.0, 0.0, 1.0), unreal.Vector(0.0, 1.0, 0.0))
            rotate_bone_and_descendants(wk, wk["pelvis"][0], q_lean, ALL_UPPER_FROM_PELVIS)

            for s in ("l", "r"):
                apply_2bone_arm_ik(
                    wk, s, wk[f"hand_{s}"][0], wk[f"lowerarm_{s}"][0],
                    lengths[f"lu_{s}"], lengths[f"lf_{s}"], soft=0.85,
                )

        sc = s_sm[k]
        for s in ("l", "r"):
            hip = wk[f"thigh_{s}"][0]
            fo_pos = wk[f"foot_{s}"][0]
            if sc < 1.0 - 1e-5:
                d_hf = vsub(fo_pos, hip)
                fo_tgt = unreal.Vector(hip.x + d_hf.x * sc, hip.y + d_hf.y * sc, fo_pos.z)
            else:
                fo_tgt = fo_pos
            pole = unreal.Vector(0.0, 1.0, 0.0)
            apply_2bone_leg_ik(wk, s, fo_tgt, lengths[f"lt_{s}"], lengths[f"lc_{s}"], pole)

        rev_worlds.append(wk)

    # World contact pinning in rev (k < PIN_U, foot_z < 14 cm)
    pinned: list[tuple[str, int, int]] = []
    for s in ("l", "r"):
        run: list[int] = []
        for k in list(range(nr)) + [None]:
            if k is not None and uf[k] < pin_u and rev_worlds[k][f"foot_{s}"][0].z < 14.0:
                run.append(k)
                continue
            if len(run) > 1:
                mx = sum(rev_worlds[q][f"foot_{s}"][0].x for q in run) / float(len(run))
                my = sum(rev_worlds[q][f"foot_{s}"][0].y for q in run) / float(len(run))
                for q in run:
                    wk = rev_worlds[q]
                    fo_p = wk[f"foot_{s}"][0]
                    tgt = unreal.Vector(mx, my, fo_p.z)
                    apply_2bone_leg_ik(wk, s, tgt, lengths[f"lt_{s}"], lengths[f"lc_{s}"], unreal.Vector(0.0, 1.0, 0.0))
                pinned.append((s, run[0], run[-1]))
            run = []

    # Rev ANKLE_MIN=11.0 cm clamp
    for k in range(nr):
        u = uf[k]
        w_ank = 1.0 if u <= ankle_hold else (0.0 if u >= ankle_out else 1.0 - ss((u - ankle_hold) / (ankle_out - ankle_hold)))
        if w_ank <= 1e-4:
            continue
        wk = rev_worlds[k]
        fl_p, fr_p = wk["foot_l"][0], wk["foot_r"][0]
        gap = fl_p.x - fr_p.x
        push = (ankle_min - gap) * w_ank
        if push <= 0.01:
            continue
        sw = "l" if fl_p.z > fr_p.z else "r"
        dx = push if sw == "l" else -push
        fo_cur = wk[f"foot_{sw}"][0]
        apply_2bone_leg_ik(
            wk, sw, unreal.Vector(fo_cur.x + dx, fo_cur.y, fo_cur.z),
            lengths[f"lt_{sw}"], lengths[f"lc_{sw}"], unreal.Vector(0.0, 1.0, 0.0),
        )

    st, pv = ("l", "r") if side == "L" else ("r", "l")
    pv_runs = [p for p in pinned if p[0] == pv]
    k_rp = pv_runs[0][1] if pv_runs else 3
    st_pin0 = next((p[2] for p in pinned if p[0] == st and p[1] == 0), 0)
    return rev_worlds, c_list, ds, k_rp, st_pin0, lengths


def turn_foot_group(
    foot_pos: unreal.Vector,
    foot_rot: unreal.Quat,
    ball_pos: unreal.Vector,
    ball_rot: unreal.Quat,
    pivot: unreal.Vector,
    deg: float,
) -> tuple[unreal.Vector, unreal.Quat, unreal.Vector, unreal.Quat]:
    q = q_z(deg)
    return (
        vadd(pivot, rot_z(vsub(foot_pos, pivot), deg)),
        q * foot_rot,
        vadd(pivot, rot_z(vsub(ball_pos, pivot), deg)),
        q * ball_rot,
    )


def lerp_foot_group(
    a_fp: unreal.Vector, a_fr: unreal.Quat, a_bp: unreal.Vector, a_br: unreal.Quat,
    b_fp: unreal.Vector, b_fr: unreal.Quat, b_bp: unreal.Vector, b_br: unreal.Quat,
    w: float,
) -> tuple[unreal.Vector, unreal.Quat, unreal.Vector, unreal.Quat]:
    fp = vlerp(a_fp, b_fp, w)
    fr = qnlerp(a_fr, b_fr, w)
    da = vsub(a_bp, a_fp)
    db = vsub(b_bp, b_fp)
    la, lb = vlen(da), vlen(db)
    fa = unreal.Vector(da.x, da.y, 0.0)
    fb = unreal.Vector(db.x, db.y, 0.0)
    dy = math.degrees(math.atan2(fb.y, fb.x) - math.atan2(fa.y, fa.x))
    dy = (dy + 180.0) % 360.0 - 180.0
    ra = rot_z(da, dy * w)
    v = vlerp(ra, db, w)
    ln = vlen(v)
    tgt_l = la * (1.0 - w) + lb * w
    bp = vadd(fp, vmul(v, tgt_l / ln) if ln > 1e-6 else v)
    br = qnlerp(a_br, b_br, w)
    return fp, fr, bp, br


def add_foley(seq: unreal.AnimSequence, curves: dict[str, list[float]], foot_z: dict[str, list[float]]) -> dict:
    src = unreal.load_asset(LOOP_F)
    proto = {}
    for s, tr in FOLEY_TRACKS.items():
        evs = AL.get_animation_notify_events_for_track(src, tr)
        proto[s] = evs[0]
    for tr in FOLEY_TRACKS.values():
        if tr in [str(x) for x in AL.get_animation_notify_track_names(seq)]:
            AL.remove_animation_notify_track(seq, tr)
    out = {}
    for s, tr in FOLEY_TRACKS.items():
        AL.add_animation_notify_track(seq, tr, unreal.LinearColor(0.9, 0.6, 0.1, 1.0))
        pev = proto[s]
        pn = pev.get_editor_property("notify")
        c, last, out[s] = curves["contact_" + s], -99, []
        for f in range(1, len(c)):
            if not (c[f - 1] < 0.5 <= c[f]) or f - last < FOLEY_MIN_GAP_F:
                continue
            z = foot_z[s][f]
            if z > FOLEY_FOOT_MAX_CM:
                continue
            ev = AL.add_animation_notify_event(seq, tr, f / 30.0, pn.get_class())
            for k in ("event", "side", "volume_multiplier", "pitch_multiplier"):
                try:
                    ev.set_editor_property(k, pn.get_editor_property(k))
                except Exception:
                    pass
            last = f
            out[s].append(f)
        for ev in AL.get_animation_notify_events_for_track(seq, tr):
            for k in (
                "trigger_weight_threshold", "notify_trigger_chance", "trigger_on_dedicated_server",
                "trigger_on_follower", "notify_filter_type", "notify_filter_lod", "montage_tick_type",
            ):
                try:
                    ev.set_editor_property(k, pev.get_editor_property(k))
                except Exception:
                    pass
    return out


def build_turn(
    turn: str,
    start_seq: unreal.AnimSequence,
    loop_seq: unreal.AnimSequence,
    tmpl_path: str,
    tmap: dict,
) -> None:
    deg, side = int(turn[:3]), turn[3]
    big = deg == 180
    sg = 1.0 if side == "L" else -1.0
    st, pv = ("l", "r") if side == "L" else ("r", "l")

    frac = (
        [0.0, 0.05, 0.15, 0.33, 0.55, 0.75, 0.88, 0.95, 0.99, 1.0]
        if big else
        [0.0, 0.06, 0.20, 0.45, 0.70, 0.88, 0.96, 1.0]
    )
    h = 6 if big else 5
    pl = 4.0 if big else 3.5
    lo = 0.5
    to = float(h - 1)
    step_deg = 150.0 if big else 75.0
    lag_deg = 35.0 if big else 20.0
    piv_deg = 60.0 if big else 35.0
    heel_deg = 25.0
    drop_piv = 3.5
    lead_c, lead_h = 18.0, 35.0
    tuck = 0.5
    arc_step, arc_swing = 5.0, 14.0

    def fr(u: int) -> float:
        return frac[min(u, len(frac) - 1)]

    tmpl = unreal.load_asset(tmpl_path)
    bones = [str(x) for x in AL.get_animation_track_names(tmpl)]
    order = topo_order(bones)
    start_locals = sample_seq_locals(start_seq, order)
    loop_locals = sample_seq_locals(loop_seq, order)

    rev_worlds, c_list, ds_rev, k_rp, pin_end, lengths = build_rev_phase(side, loop_locals, order, tmap)
    nr = len(rev_worlds)
    nt = h + nr
    yaw_deg = [(+sg * deg) * (1.0 - fr(u)) for u in range(nt)]

    s0_local = start_locals[0]
    s0_world = fk_from_locals(s0_local, order, root_pos=unreal.Vector(0.0, 0.0, 0.0), root_rot=unreal.Quat(0.0, 0.0, 0.0, 1.0))
    w0_world = fk_from_locals(s0_local, order, root_pos=unreal.Vector(0.0, 0.0, 0.0), root_rot=q_z(yaw_deg[0]))

    r0_foot_st = rev_worlds[0][f"foot_{st}"][0]
    wl = vadd(w0_world[f"foot_{pv}"][0], rot_z(vsub(w0_world[f"foot_{st}"][0], w0_world[f"foot_{pv}"][0]), -step_deg * sg))
    wl.z = w0_world[f"foot_{st}"][0].z
    root_h = vsub(wl, rot_z(r0_foot_st, yaw_deg[h]))
    root_h.z = 0.0

    roots = [
        vmul(root_h, ss(u / float(h))) if u < h else vadd(root_h, unreal.Vector(0.0, ds_rev[u - h], 0.0))
        for u in range(nt)
    ]

    def world_rev(u: int) -> dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]:
        rw = rev_worlds[u - h]
        yd = yaw_deg[u]
        qz = q_z(yd)
        r_off = rw["root"][0]
        res = {}
        for b in order:
            p, r, s = rw[b]
            res[b] = (vadd(roots[u], rot_z(vsub(p, r_off), yd)), qz * r, s)
        return res

    wh_rev = world_rev(h)
    gh_st = (
        wh_rev[f"foot_{st}"][0], wh_rev[f"foot_{st}"][1],
        wh_rev[f"ball_{st}"][0], wh_rev[f"ball_{st}"][1],
    )
    g_plant = turn_foot_group(*gh_st, gh_st[2], +lag_deg * sg)
    g0_st = (w0_world[f"foot_{st}"][0], w0_world[f"foot_{st}"][1], w0_world[f"ball_{st}"][0], w0_world[f"ball_{st}"][1])
    g0_pv = (w0_world[f"foot_{pv}"][0], w0_world[f"foot_{pv}"][1], w0_world[f"ball_{pv}"][0], w0_world[f"ball_{pv}"][1])

    def pivot_foot_eval(u: float, h_sign: float = 1.0) -> tuple[unreal.Vector, unreal.Quat, unreal.Vector, unreal.Quat]:
        fp, fr_q, bp, br_q = turn_foot_group(*g0_pv, g0_pv[2], -piv_deg * sg * ss(u / to))
        fd = vsub(bp, fp)
        fd.z = 0.0
        lat = vnorm(vcross(unreal.Vector(0.0, 0.0, 1.0), fd))
        hw = ss((u - (to - 1.5)) / 1.5)
        qh = axis_quat(lat, h_sign * heel_deg * hw)
        fp_l = vadd(bp, qh.rotate_vector(vsub(fp, bp)))
        return fp_l, qh * fr_q, bp, qh * br_q

    h_sign = 1.0
    if pivot_foot_eval(to, 1.0)[0].z < g0_pv[0].z:
        h_sign = -1.0
    g_to = pivot_foot_eval(to, h_sign)
    u_rp = h + k_rp
    w_rp = world_rev(u_rp)
    g_rp = (w_rp[f"foot_{pv}"][0], w_rp[f"foot_{pv}"][1], w_rp[f"ball_{pv}"][0], w_rp[f"ball_{pv}"][1])

    rev0_world = rev_worlds[0]
    w_all: list[dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]] = []
    for u in range(nt):
        if u < h:
            w_leg = (u / float(h)) ** 2
            w_up = (u / float(h)) ** 3
            lead_w = ss(u / 1.0) * (1.0 - ss((u - (h - 2)) / 2.0))
            pw0 = w0_world["pelvis"][0]
            pwh = wh_rev["pelvis"][0]
            pw = vlerp(pw0, pwh, ss(u / float(h)))
            pw.z -= drop_piv * math.sin(math.pi * min(u / float(h), 1.0))
            pr = qnlerp(w0_world["pelvis"][1], wh_rev["pelvis"][1], ss(u / float(h)))

            wk: dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]] = {}
            wk["root"] = (roots[u], q_z(yaw_deg[u]), unreal.Vector(1.0, 1.0, 1.0))
            if "attach" in order:
                wk["attach"] = (roots[u], q_z(yaw_deg[u]), unreal.Vector(1.0, 1.0, 1.0))
            wk["pelvis"] = (pw, pr, unreal.Vector(1.0, 1.0, 1.0))

            s0_pel = s0_world["pelvis"][0]
            r0_pel = rev0_world["pelvis"][0]
            for b in order:
                if b in ("root", "attach", "pelvis"):
                    continue
                is_up = b in ALL_UPPER_FROM_PELVIS
                wb = w_up if is_up else w_leg
                rel_s0 = vsub(s0_world[b][0], s0_pel)
                rel_r0 = vsub(rev0_world[b][0], r0_pel)
                rel_p = vlerp(rel_s0, rel_r0, wb)
                sy = yaw_deg[u]
                if b.startswith(("neck", "head")):
                    sy += -(lead_c + lead_h) * sg * lead_w
                elif b.startswith(("spine_04", "spine_05", "clavicle", "upperarm", "lowerarm", "hand", "weapon", "thumb", "index", "middle", "ring", "pinky")):
                    sy += -lead_c * sg * lead_w
                elif b.startswith("spine_02"):
                    sy += -0.5 * lead_c * sg * lead_w
                w_rot_s0 = q_z(sy) * s0_world[b][1]
                w_rot_r0 = q_z(sy) * rev0_world[b][1]
                wk[b] = (vadd(pw, rot_z(rel_p, sy)), qnlerp(w_rot_s0, w_rot_r0, wb), unreal.Vector(1.0, 1.0, 1.0))
        else:
            wk = world_rev(u)

        # Arms tuck/swing target during pivot through H+2, plus outward clearance across H..H+4
        clr_w = ss((u - (h - 1)) / 2.0) * (1.0 - ss((u - (h + 4)) / 3.0))
        for s in ("l", "r"):
            lead = 1.0 if s == st else 0.0
            p_arm = min(max((u + lead) / float(h), 0.0), 1.2)
            tk = tuck * ss((u + lead) / 1.5) * (1.0 - ss((u - h) / 3.0))
            inw = vsub(wk["spine_04"][0], wk[f"upperarm_{s}"][0])
            inw.z = 0.0
            out_s = vmul(vnorm(inw), -1.0)
            la_b, ha_b = f"lowerarm_{s}", f"hand_{s}"
            if tk > 0.0:
                lead_ang = (-lead_c * sg * ss(u / 1.0) * (1.0 - ss((u - (h - 2)) / 2.0))) if u < h else 0.0
                fwd = rot_z(unreal.Vector(0.0, 1.0, 0.0), yaw_deg[u] + lead_ang)
                sh = wk[f"upperarm_{s}"][0]
                sw = ss(p_arm / 0.8) * (1.15 if lead else 0.9)
                e_tgt = vadd(
                    vadd(vsub(sh, unreal.Vector(0.0, 0.0, 21.0 - 5.0 * sw)), vmul(fwd, -((6.0 + 10.0 * sw) if lead else 3.0))),
                    vmul(out_s, 7.0 + 13.0 * sw),
                )
                h_tgt = vadd(
                    vadd(vsub(e_tgt, unreal.Vector(0.0, 0.0, 16.0 - 6.0 * sw)), vmul(fwd, -(6.0 + 10.0 * sw))),
                    vmul(out_s, 5.0 + 9.0 * sw),
                )
                de = vmul(vsub(e_tgt, wk[la_b][0]), tk)
                dh = vmul(vsub(h_tgt, wk[ha_b][0]), tk)
                wk[la_b] = (vadd(wk[la_b][0], de), wk[la_b][1], wk[la_b][2])
                wk[ha_b] = (vadd(wk[ha_b][0], dh), wk[ha_b][1], wk[ha_b][2])
                for b in PARENTS:
                    if b.endswith(f"_{s}") and b.startswith(HAND_CHILD_PREFIXES) and b in wk:
                        cp, cr, cs_ = wk[b]
                        wk[b] = (vadd(cp, dh), cr, cs_)
                for tw in (f"lowerarm_twist_01_{s}", f"lowerarm_twist_02_{s}"):
                    if tw in wk:
                        tp, tr, ts = wk[tw]
                        wk[tw] = (vadd(tp, de), tr, ts)
            if clr_w > 1e-4:
                clr_off = vmul(out_s, 2.5 * clr_w)
                wk[la_b] = (vadd(wk[la_b][0], clr_off), wk[la_b][1], wk[la_b][2])
                wk[ha_b] = (vadd(wk[ha_b][0], clr_off), wk[ha_b][1], wk[ha_b][2])
                for b in PARENTS:
                    if b.endswith(f"_{s}") and b.startswith(HAND_CHILD_PREFIXES) and b in wk:
                        cp, cr, cs_ = wk[b]
                        wk[b] = (vadd(cp, clr_off), cr, cs_)
                for tw in (f"lowerarm_twist_01_{s}", f"lowerarm_twist_02_{s}"):
                    if tw in wk:
                        tp, tr, ts = wk[tw]
                        wk[tw] = (vadd(tp, clr_off), tr, ts)

        # Step foot
        if u < h:
            if u <= lo:
                g_st = g0_st
            elif u < pl:
                s_ = ss((u - lo) / (pl - lo))
                fp, fr_q, bp, br_q = lerp_foot_group(*g0_st, *g_plant, s_)
                arc = arc_step * math.sin(math.pi * s_)
                g_st = (vadd(fp, unreal.Vector(0.0, 0.0, arc)), fr_q, vadd(bp, unreal.Vector(0.0, 0.0, arc)), br_q)
            else:
                g_st = turn_foot_group(*g_plant, g_plant[2], -lag_deg * sg * ss((u - pl) / (h - pl)))
            wk[f"foot_{st}"] = (g_st[0], g_st[1], wk[f"foot_{st}"][2])
            wk[f"ball_{st}"] = (g_st[2], g_st[3], wk[f"ball_{st}"][2])

        # Pivot foot
        if u <= u_rp:
            if u <= to:
                g_pv = pivot_foot_eval(float(u), h_sign)
            else:
                s_ = ss((u - to) / float(u_rp - to))
                fp, fr_q, bp, br_q = lerp_foot_group(*g_to, *g_rp, s_)
                arc = arc_swing * math.sin(math.pi * s_)
                g_pv = (vadd(fp, unreal.Vector(0.0, 0.0, arc)), fr_q, vadd(bp, unreal.Vector(0.0, 0.0, arc)), br_q)
            wk[f"foot_{pv}"] = (g_pv[0], g_pv[1], wk[f"foot_{pv}"][2])
            wk[f"ball_{pv}"] = (g_pv[2], g_pv[3], wk[f"ball_{pv}"][2])

        w_all.append(wk)

    # Pin step foot across H .. H + pin_end
    for u in range(h, h + pin_end + 1):
        w_all[u][f"foot_{st}"] = (gh_st[0], gh_st[1], w_all[u][f"foot_{st}"][2])
        w_all[u][f"ball_{st}"] = (gh_st[2], gh_st[3], w_all[u][f"ball_{st}"][2])

    # Pivot ANKLE_MIN=11.0 guard over f0..H+14
    for u in range(1, min(nt, h + 15)):
        w_ank = 1.0 if u <= h + 10 else 1.0 - ss((u - (h + 10)) / 5.0)
        wk = w_all[u]
        lat = vsub(wk["thigh_l"][0], wk["thigh_r"][0])
        lat.z = 0.0
        lat = vnorm(lat)
        gap = vdot(vsub(wk["foot_l"][0], wk["foot_r"][0]), lat)
        push = (11.0 - gap) * w_ank
        if push > 0.01:
            sw = "l" if wk["foot_l"][0].z > wk["foot_r"][0].z else "r"
            dx = vmul(lat, push if sw == "l" else -push)
            wk[f"foot_{sw}"] = (vadd(wk[f"foot_{sw}"][0], dx), wk[f"foot_{sw}"][1], wk[f"foot_{sw}"][2])
            wk[f"ball_{sw}"] = (vadd(wk[f"ball_{sw}"][0], dx), wk[f"ball_{sw}"][1], wk[f"ball_{sw}"][2])

    # Leg 2-bone IK with 3-pass smoothed knee poles
    def smooth_vecs(vs: list[unreal.Vector]) -> list[unreal.Vector]:
        out_v = list(vs)
        for _ in range(3):
            nxt = [out_v[0]]
            for i in range(1, len(out_v) - 1):
                nxt.append(vnorm(vadd(vadd(vmul(out_v[i - 1], 0.25), vmul(out_v[i], 0.5)), vmul(out_v[i + 1], 0.25))))
            nxt.append(out_v[-1])
            out_v = nxt
        return out_v

    for s in ("l", "r"):
        us = [u for u in range(0, nt) if u < h or (s == st and u <= h + pin_end) or (s == pv and u <= u_rp)]
        fws = []
        for u in us:
            wk = w_all[u]
            if u == 0:
                p0 = vsub(wk[f"calf_{s}"][0], vmul(vadd(wk[f"thigh_{s}"][0], wk[f"foot_{s}"][0]), 0.5))
                p0.z = 0.0
                fws.append(vnorm(p0) if vlen(p0) > 1e-3 else rot_z(unreal.Vector(0.0, 1.0, 0.0), yaw_deg[0]))
                continue
            fdir = vsub(wk[f"ball_{s}"][0], wk[f"foot_{s}"][0])
            fdir.z = 0.0
            nf = vlen(fdir)
            base_fw = rot_z(unreal.Vector(0.0, 1.0, 0.0), yaw_deg[u])
            if nf > 1e-3:
                base_fw = vadd(base_fw, vmul(vnorm(fdir), 2.0 * min(nf / 10.0, 1.0)))
            fws.append(vnorm(base_fw))
        fws = smooth_vecs(fws)
        for u, fw in zip(us, fws):
            if u == 0:
                continue
            wk = w_all[u]
            apply_2bone_leg_ik(
                wk, s, wk[f"foot_{s}"][0],
                lengths[f"lt_{s}"], lengths[f"lc_{s}"], fw,
                keep_foot_rot=wk[f"foot_{s}"][1],
            )

    # Uniform 2-bone arm IK over f1..H+6 (soft=0.85, no discontinuity at H)
    for u in range(1, min(nt, h + 7)):
        wk = w_all[u]
        for s in ("l", "r"):
            apply_2bone_arm_ik(
                wk, s, wk[f"hand_{s}"][0], wk[f"lowerarm_{s}"][0],
                lengths[f"lu_{s}"], lengths[f"lf_{s}"], soft=0.85,
            )

    # Exact START_L f0 pose at u=0 + finger nlerp at f0..1
    locals_all: list[dict[str, tuple[unreal.Vector, unreal.Quat, unreal.Vector]]] = []
    for u in range(nt):
        loc_u = world_to_locals(w_all[u], order)
        if u == 0:
            for b in order:
                if b == "root":
                    loc_u[b] = (unreal.Vector(0.0, 0.0, 0.0), q_z(yaw_deg[0]), unreal.Vector(1.0, 1.0, 1.0))
                else:
                    loc_u[b] = s0_local[b]
        else:
            for b in order:
                if b.startswith(FINGER_PREFIXES):
                    if u < h:
                        wf = ss(u / 1.0)
                        p0, r0, s0 = s0_local[b]
                        pr, rr, _ = loop_locals[int(round(c_list[0])) % len(loop_locals)][b]
                        loc_u[b] = (vlerp(p0, pr, wf), qnlerp(r0, rr, wf), s0)
                    else:
                        a = int(math.floor(c_list[u - h]))
                        wf = c_list[u - h] - a
                        ia, ib = a % len(loop_locals), (a + 1) % len(loop_locals)
                        pa, ra, sa = loop_locals[ia][b]
                        pb, rb, _ = loop_locals[ib][b]
                        loc_u[b] = (vlerp(pa, pb, wf), qnlerp(ra, rb, wf), sa)
        locals_all.append(loc_u)

    # 6-pass qnlerp arm smoothing over f1..H+8
    arm_smooth_bones = [
        b for b in order
        if any(b.startswith(p) for p in ("clavicle_", "upperarm_", "lowerarm_", "hand_"))
    ]
    for _ in range(6):
        nxt_rots = [{b: locals_all[u][b][1] for b in arm_smooth_bones} for u in range(nt)]
        for u in range(1, min(nt - 1, h + 9)):
            w_sm = 0.6 * ss(u / 2.0) * (1.0 - ss((u - (h + 3)) / 5.0))
            if w_sm <= 1e-4:
                continue
            for b in arm_smooth_bones:
                avg = qnlerp(locals_all[u - 1][b][1], locals_all[u + 1][b][1], 0.5)
                nxt_rots[u][b] = qnlerp(locals_all[u][b][1], avg, w_sm)
        for u in range(1, min(nt - 1, h + 9)):
            for b in arm_smooth_bones:
                p, _, s = locals_all[u][b]
                locals_all[u][b] = (p, nxt_rots[u][b], s)

    # Recompute final world & sync ik_foot_* / ik_hand_*
    foot_z = {"l": [], "r": []}
    for u in range(nt):
        wu = fk_from_locals(locals_all[u], order)
        for s in ("l", "r"):
            foot_z[s].append(wu[f"foot_{s}"][0].z)
        if "ik_foot_root" in wu:
            wu["ik_foot_root"] = (wu["root"][0], wu["root"][1], unreal.Vector(1.0, 1.0, 1.0))
            for s in ("l", "r"):
                if f"ik_foot_{s}" in wu:
                    wu[f"ik_foot_{s}"] = wu[f"foot_{s}"]
        if "ik_hand_root" in wu:
            wu["ik_hand_root"] = (wu["root"][0], wu["root"][1], unreal.Vector(1.0, 1.0, 1.0))
            if "ik_hand_gun" in wu:
                wu["ik_hand_gun"] = wu["hand_r"]
            if "ik_hand_l" in wu:
                wu["ik_hand_l"] = wu["hand_l"]
            if "ik_hand_r" in wu:
                wu["ik_hand_r"] = wu["hand_r"]
        locals_all[u] = world_to_locals(wu, order)

    # Sample LOOP_F curves
    loop_curve_names = ("contact_l", "contact_r", "phase")
    loop_cv: dict[str, list[float]] = {}
    nl = len(loop_locals)
    for cname in loop_curve_names:
        t_arr, v_arr = AL.get_float_keys(loop_seq, cname)
        t_list, v_list = list(t_arr), list(v_arr)
        sampled = []
        for f in range(nl):
            tf = f / 30.0
            idx = min(range(len(t_list)), key=lambda i: abs(t_list[i] - tf))
            sampled.append(float(v_list[idx]))
        loop_cv[cname] = sampled

    curves_out: dict[str, list[float]] = {}
    for cname in loop_curve_names:
        arr = loop_cv[cname]
        vals = []
        for u in range(nt):
            c = c_list[0] if u < h else c_list[u - h]
            a = int(math.floor(c))
            wf = c - a
            vals.append(round(arr[a % nl] * (1.0 - wf) + arr[(a + 1) % nl] * wf, 5))
        curves_out[cname] = vals

    spd_out = [0.0] + [
        math.hypot(roots[u].x - roots[u - 1].x, roots[u].y - roots[u - 1].y) * 30.0
        for u in range(1, nt)
    ]
    spd_out[0] = spd_out[1]
    curves_out["movedata_speed"] = [round(v, 2) for v in spd_out]
    curves_out["enable_warping"] = [round(ss((u - h - 2) / 4.0), 4) for u in range(nt)]
    curves_out["steeringtargettime"] = [round(1.0 - ss((u - h - 2) / 6.0), 4) for u in range(nt)]

    dst = f"{FOLDER}/AS_GSTN_RunStart_Generic_w2t{turn}p"
    saved = False
    if SAVE:
        if EAL.does_asset_exist(dst):
            EAL.delete_asset(dst)
        if not EAL.duplicate_asset(tmpl_path, dst):
            raise RuntimeError(f"Failed to duplicate {tmpl_path} -> {dst}")
        seq = unreal.load_asset(dst)
        seq.set_editor_property("enable_root_motion", True)
        seq.set_editor_property("force_root_lock", True)
        seq.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)

        ctrl = seq.get_editor_property("controller")
        ctrl.open_bracket(unreal.Text(f"build base27 {turn}"), False)
        try:
            ctrl.set_number_of_frames(unreal.FrameNumber(nt - 1), False)
            for b in order:
                pos = [locals_all[u][b][0] for u in range(nt)]
                rot = [locals_all[u][b][1] for u in range(nt)]
                scl = [unreal.Vector(1.0, 1.0, 1.0) for _ in range(nt)]
                if not ctrl.set_bone_track_keys(b, pos, rot, scl, False):
                    raise RuntimeError(f"set_bone_track_keys failed for {b}")
        finally:
            ctrl.close_bracket(False)

        times = [u / 30.0 for u in range(nt)]
        for cname, vals in curves_out.items():
            if AL.does_curve_exist(seq, cname, unreal.RawCurveTrackTypes.RCT_FLOAT):
                AL.remove_curve(seq, cname, False)
            AL.add_curve(seq, cname, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
            AL.add_float_curve_keys(seq, cname, times, [float(v) for v in vals])

        if "PoseSearch" in [str(x) for x in AL.get_animation_notify_track_names(seq)]:
            AL.remove_animation_notify_track(seq, "PoseSearch")
        AL.add_animation_notify_track(seq, "PoseSearch", unreal.LinearColor(0.2, 0.6, 1.0, 1.0))
        AL.add_animation_notify_state_event(
            seq, "PoseSearch", 0.05, max(0.1, seq.get_play_length() - 0.05),
            unreal.AnimNotifyState_PoseSearchBlockTransition,
        )
        foley_rep = add_foley(seq, curves_out, foot_z)
        saved = bool(EAL.save_asset(dst, only_if_is_dirty=False))
    else:
        foley_rep = {}

    out_dir = Path(PROJ) / "Saved" / "AnimPipeline" / "runstart"
    out_dir.mkdir(parents=True, exist_ok=True)
    rep_path = out_dir / f"AS_GSTN_RunStart_Generic_w2t{turn}p_build.json"
    rep_path.write_text(
        json.dumps(
            {
                "turn": turn, "dst": dst, "frames": nt, "h": h, "u_rp": u_rp,
                "pin_end": pin_end, "saved": saved, "foley": foley_rep,
                "root_H": [round(root_h.x, 2), round(root_h.y, 2), round(root_h.z, 2)],
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    print(f"ESITO: OK {turn} dst={dst.rsplit('/', 1)[-1]} frames={nt} u_rp={u_rp} saved={saved}")


def main() -> None:
    tmap_path = resolve_tmap_path()
    tmap = json.loads(tmap_path.read_text(encoding="utf-8"))
    start_seq = unreal.load_asset(START_L)
    loop_seq = unreal.load_asset(LOOP_F)
    if not start_seq or not loop_seq:
        raise RuntimeError("Missing M_Relaxed_Run_Start_F_Lfoot or M_Relaxed_Run_Loop_F")
    tmpl_path = REF_TEMPLATE if EAL.does_asset_exist(REF_TEMPLATE) else REF_FALLBACK
    if not EAL.does_asset_exist(tmpl_path):
        raise RuntimeError(f"Missing template asset {tmpl_path}")
    for turn in TURNS:
        build_turn(turn, start_seq, loop_seq, tmpl_path, tmap)


if __name__ == "__main__":
    main()
