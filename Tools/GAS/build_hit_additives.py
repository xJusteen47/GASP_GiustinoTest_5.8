"""Build the 8 directional hit additive clips (ground/air x front/back/left/right) for UGSTN_ReactionAnimSet::DirectionalHits.

Run inside the editor (or the pythonscript commandlet). Every clip is a local-space additive on the reference pose, made bone by
bone from numbers (no rig bake), so it can be rebuilt and tuned from this file:
  frame 0 = no offset; snap to the peak in 3 frames; held to frame 4 (the component holds Layer.Time at 0.07 s during hitstop);
  then a damped spring back with one overshoot and a settle (Sakurai hold + recoil). Each bone has its own delay so the chest
  leads and the neck/head and the arms follow 1-3 frames later (whiplash, overlap).
Rotations are authored about the character's axes (forward / right / up measured from the reference pose) and turned into each
bone's local delta, so the clip does not depend on the mannequin's local bone axes. The node masks it to spine_01 and children.
Writes Saved/Reaction/build_hit_additives.log (ends with DONE_MARK)."""
import math
import os
import unreal

OUT_DIR = "/Game/GSTN/Animation/Reaction/HitAdditive"
SKEL_FROM = "/Game/GASNPPTest/Combat/Reaction/AS_React_HitStand"
FPS = 60
FRAMES = 41           # 40 intervals = 0.667 s (v3, Fable: room for the second wobble); even, so the 30 fps resample lands on whole frames
PEAK = 3              # frame of the peak
HOLD_END = 4          # peak held to here
LOG = os.path.join(unreal.Paths.project_saved_dir(), "Reaction", "build_hit_additives.log")
L = []

# Character-axis angles in degrees at the peak, cumulative from the pelvis (each bone's own share = its value minus its parent's).
#   back: + = upper body thrown backward (head moves to -forward)
#   side: + = upper body bends to its right (head moves to +right)
#   twist: + = chest turns toward its right
# delay in frames, k = damping (smaller = more wobble).
TORSO = [
    # bone,      delay, k
    ("spine_01", 0.0, 1.0),
    ("spine_02", 0.15, 1.0),
    ("spine_03", 0.3, 1.0),
    ("spine_04", 0.5, 0.95),
    ("spine_05", 0.6, 0.9),
    ("neck_01", 0.9, 0.75),
    ("neck_02", 1.0, 0.7),
    ("head", 1.2, 0.6),   # v3: every torso bone peaks before the hitstop hold (0.07 s = frame 4.2); the lag shows in the recoil
]
SHARE = {"spine_01": 0.10, "spine_02": 0.30, "spine_03": 0.52, "spine_04": 0.75, "spine_05": 1.0}

# Peak pose per side (chest total at spine_05, head total, arms). Asymmetric on purpose: no twinning.
SIDES = {
    # attacker in front -> chest knocked back, head whips back after it, arms fling forward/up, the left one first
    "Front": {"chest": (22.0, 4.0, -7.0), "head": (34.0, 6.0, -9.0),
              "clav_l": (-24.0, 0.0, 0.0), "clav_r": (-14.0, 0.0, 0.0), "elbow_l": 30.0, "elbow_r": 14.0, "arm_delay": (2.0, 3.5)},
    # attacker behind -> chest thrown forward (hips pushed from behind), head lags back then nods, arms fling back
    "Back": {"chest": (-20.0, -3.0, 6.0), "head": (-12.0, -4.0, 8.0),
             "clav_l": (12.0, 0.0, 0.0), "clav_r": (16.0, 0.0, 0.0), "elbow_l": 8.0, "elbow_r": 14.0, "arm_delay": (2.5, 1.5)},
    # attacker on the left -> struck side caves: chest bends away (to its right), left shoulder knocked back, head lags toward the hit
    "Left": {"chest": (4.0, 14.0, -14.0), "head": (8.0, -12.0, -4.0),   # v4 (Fable r2)
             "clav_l": (16.0, -4.0, 0.0), "clav_r": (-5.0, 10.0, 0.0), "elbow_l": 28.0, "elbow_r": 6.0, "arm_delay": (1.0, 2.5)},
    # mirror of Left with its own small differences
    "Right": {"chest": (7.0, -17.0, 13.0), "head": (9.0, 6.0, 4.0),
              "clav_l": (-6.0, -11.0, 0.0), "clav_r": (16.0, 4.0, 0.0), "elbow_l": 5.0, "elbow_r": 28.0, "arm_delay": (2.5, 1.0)},
}
VARIANTS = {"Ground": {"amp": 1.0, "k": 0.11}, "Air": {"amp": 1.35, "k": 0.09}}   # v3 (Fable): slower damping, readable overshoot


def env(t, k):
    """0 at t<=0, eases out to 1 at PEAK, holds to HOLD_END, damped spring after (one overshoot near frame 14), 0 at the end."""
    if t <= 0.0:
        return 0.0
    if t < PEAK:
        return math.sin(0.5 * math.pi * t / PEAK)
    if t <= HOLD_END:
        return 1.0
    u = t - HOLD_END
    v = math.exp(-k * u) * math.cos(u * math.pi / 10.0)
    taper = 1.0
    end = FRAMES - 1
    if t > end - 5:
        taper = max(0.0, (end - t) / 5.0)
    return v * taper


def log(s):
    L.append(str(s))


def flush():
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    open(LOG, "w").write("\n".join(L) + "\nDONE_MARK\n")


# Quaternion math in plain Python (x, y, z, w), FQuat semantics: qmul(a, b) applies b first, then a. The Python binding's '*' on
# unreal.Quat gave a reversed composition (hitadd-v1: the front hit folded the chest forward), so nothing here relies on it.
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    r = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qinv(q))
    return (r[0], r[1], r[2])


def qt(uq):
    return (uq.x, uq.y, uq.z, uq.w)


def quat_axis(axis, deg):
    a = math.radians(deg) * 0.5
    s = math.sin(a)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(a))


def build():
    src = unreal.load_asset(SKEL_FROM)
    skel = src.get_editor_property("skeleton")
    ref = unreal.AnimPoseExtensions.get_reference_pose(skel)
    names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(ref)]
    WS = unreal.AnimPoseSpaces.WORLD
    LS = unreal.AnimPoseSpaces.LOCAL

    def cs(b):
        return unreal.AnimPoseExtensions.get_bone_pose(ref, b, WS)

    def loc(b):
        return unreal.AnimPoseExtensions.get_bone_pose(ref, b, LS)

    # character axes from the reference pose
    upv = unreal.Vector(0, 0, 1)
    fwdv = (cs("ball_l").translation - cs("foot_l").translation) + (cs("ball_r").translation - cs("foot_r").translation)
    fwdv = unreal.Vector(fwdv.x, fwdv.y, 0).normal()
    rightv = upv.cross(fwdv).normal()
    if (cs("thigh_l").translation - cs("thigh_r").translation).dot(rightv) > 0:
        rightv = rightv * -1.0
    up, fwd, right = (0.0, 0.0, 1.0), (fwdv.x, fwdv.y, 0.0), (rightv.x, rightv.y, rightv.z)
    log(f"axes fwd={fwd} right={right}")

    # sign calibration: rotating the probe about the axis must move it the named way
    def calib(axis, probe, want):
        moved = qrot(quat_axis(axis, 10.0), probe)
        return 1.0 if sum((moved[i] - probe[i]) * want[i] for i in range(3)) > 0 else -1.0

    s_back = calib(right, up, tuple(-x for x in fwd))       # pitch back
    s_side = calib(fwd, up, right)              # bend to its right
    s_twist = calib(up, fwd, right)             # chest turns right
    log(f"signs back={s_back} side={s_side} twist={s_twist}")

    def world_q(back, side, twist):
        return qmul(quat_axis(up, s_twist * twist), qmul(quat_axis(fwd, s_side * side), quat_axis(right, s_back * back)))

    parent = {}
    for b in names:
        try:
            parent[b] = str(unreal.AnimPoseExtensions.get_parent_bone_name(ref, b)) if hasattr(unreal.AnimPoseExtensions, "get_parent_bone_name") else None
        except Exception:
            parent[b] = None
    # mannequin hierarchy (fallback when the pose API has no parent query)
    hier = {"spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "spine_04": "spine_03", "spine_05": "spine_04",
            "neck_01": "spine_05", "neck_02": "neck_01", "head": "neck_02", "clavicle_l": "spine_05", "clavicle_r": "spine_05",
            "upperarm_l": "clavicle_l", "upperarm_r": "clavicle_r", "lowerarm_l": "upperarm_l", "lowerarm_r": "upperarm_r"}
    for b, p in hier.items():
        if not parent.get(b) or parent[b] == "None":
            parent[b] = p

    def local_delta(bone, wq):
        """World-axis rotation about the bone's pivot -> local pre-multiplied delta: Q = inv(P) * W * P."""
        p = qt(cs(parent[bone]).rotation)
        return qmul(qinv(p), qmul(wq, p))

    # New sequences take their sample rate from the animation settings (30 here), which would halve the 60 fps snap. The read-only
    # PlatformTargetFrameRate is set from it at creation, so raise it in memory for this run only (the ini is not written).
    aset = unreal.get_default_object(unreal.AnimationSettings)
    try:
        log(f"anim default rate was {aset.get_editor_property('default_frame_rate')}")
        aset.set_editor_property("default_frame_rate", unreal.FrameRate(FPS, 1))
    except Exception as ex:
        log(f"anim settings rate: {ex}")
    tracks_src = [str(x) for x in unreal.AnimationLibrary.get_animation_track_names(src)]
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    made = []
    for var, vp in VARIANTS.items():
        for side, sp in SIDES.items():
            name = f"AS_GSTN_HitAdd_{var}_{side}"
            path = f"{OUT_DIR}/{name}"
            if unreal.EditorAssetLibrary.does_asset_exist(path):
                unreal.EditorAssetLibrary.delete_asset(path)
            fac = unreal.AnimSequenceFactory()
            fac.set_editor_property("target_skeleton", skel)
            seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, OUT_DIR, unreal.AnimSequence, fac)
            ctrl = seq.get_editor_property("controller")
            ctrl.open_bracket(unreal.Text("hit additive"), False)
            ctrl.set_frame_rate(unreal.FrameRate(FPS, 1), False)
            ctrl.set_number_of_frames(unreal.FrameNumber(FRAMES - 1), False)
            amp, k = vp["amp"], vp["k"]
            cb, csd, ct = sp["chest"]
            hb, hsd, ht = sp["head"]
            # cumulative world angles per torso bone at the peak
            cum = {}
            for b, _, _ in TORSO:
                if b in SHARE:
                    cum[b] = (cb * SHARE[b], csd * SHARE[b], ct * SHARE[b])
            cum["neck_01"] = (cb + (hb - cb) * 0.35, csd + (hsd - csd) * 0.35, ct + (ht - ct) * 0.35)
            cum["neck_02"] = (cb + (hb - cb) * 0.7, csd + (hsd - csd) * 0.7, ct + (ht - ct) * 0.7)
            cum["head"] = (hb, hsd, ht)
            delays = {b: d for b, d, _ in TORSO}
            kscale = {b: s for b, _, s in TORSO}
            keys = {}
            for f in range(FRAMES):
                # total world angle of each bone at this frame (each follows its own delayed envelope)
                tot = {"pelvis": (0.0, 0.0, 0.0)}
                for b, _, _ in TORSO:
                    e = env(f - delays[b], k * kscale[b]) * amp
                    tot[b] = tuple(v * e for v in cum[b])
                for b, _, _ in TORSO:
                    pb = parent[b] if parent[b] in tot else "pelvis"
                    own = tuple(tot[b][i] - tot[pb][i] for i in range(3))
                    keys.setdefault(b, []).append(local_delta(b, world_q(*own)))
                # arms: clavicle swing (back, side) and elbow flex, own delay, lagging the chest
                for s, i in (("l", 0), ("r", 1)):
                    d = sp["arm_delay"][i]
                    e = env(f - d, k * 0.8) * amp
                    cl = sp[f"clav_{s}"]
                    keys.setdefault(f"clavicle_{s}", []).append(local_delta(f"clavicle_{s}", world_q(cl[0] * e * 0.45, cl[1] * e * 0.45, 0.0)))
                    keys.setdefault(f"upperarm_{s}", []).append(local_delta(f"upperarm_{s}", world_q(cl[0] * e * 0.55, cl[1] * e * 0.55, 0.0)))
                    el = env(f - d - 1.0, k * 1.0) * amp * sp[f"elbow_{s}"]
                    # elbow flex: about the upper arm's own bend axis = rotate the forearm toward the chest (character back axis on the arm)
                    keys.setdefault(f"lowerarm_{s}", []).append(local_delta(f"lowerarm_{s}", world_q(-el if s == "l" else -el, 0.0, 0.0)))
            # write every track: reference local transform, with the delta on the animated bones
            for b in names:
                lt = loc(b)
                if b in keys:
                    rots = [unreal.Quat(*qmul(q, qt(lt.rotation))) for q in keys[b]]
                else:
                    rots = [lt.rotation] * FRAMES
                ctrl.add_bone_curve(b, False) if hasattr(ctrl, "add_bone_curve") else None
                ctrl.set_bone_track_keys(b, [lt.translation] * FRAMES, rots, [unreal.Vector(1, 1, 1)] * FRAMES, False)
            ctrl.close_bracket(False)
            seq.set_editor_property("additive_anim_type", unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE)
            seq.set_editor_property("ref_pose_type", unreal.AdditiveBasePoseType.ABPT_REF_POSE)
            unreal.EditorAssetLibrary.save_asset(path)
            seq = unreal.load_asset(path)
            # direction self-check: head position in component space at frame 0 and at the peak, from the stored tracks
            def head_pos(fr):
                chain = ["head"]
                while chain[-1] in parent and parent[chain[-1]] and parent[chain[-1]] != "None" and chain[-1] != "root":
                    chain.append(parent[chain[-1]])
                    if chain[-1] == "pelvis":
                        break
                rot, pos = qt(cs("pelvis").rotation), cs("pelvis").translation
                pos = (pos.x, pos.y, pos.z)
                for b in reversed(chain[:-1]):
                    lt = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, b, fr, False)
                    tr = qrot(rot, (lt.translation.x, lt.translation.y, lt.translation.z))
                    pos = (pos[0] + tr[0], pos[1] + tr[1], pos[2] + tr[2])
                    rot = qmul(rot, qt(lt.rotation))
                return pos
            h0, h1 = head_pos(0), head_pos(PEAK + 2)
            dv = [h1[i] - h0[i] for i in range(3)]
            dfwd = sum(dv[i] * fwd[i] for i in range(3))
            drt = sum(dv[i] * right[i] for i in range(3))
            log(f"{name}: frames={seq.get_editor_property('number_of_sampled_keys')} len={seq.get_play_length():.3f} "
                f"head moves fwd {dfwd:+.1f} cm, right {drt:+.1f} cm, up {dv[2]:+.1f} cm at frame {PEAK + 2}")
            made.append(path)
    log(f"made {len(made)}")


try:
    build()
except Exception as ex:
    import traceback
    log("ERROR " + repr(ex))
    log(traceback.format_exc())
flush()
