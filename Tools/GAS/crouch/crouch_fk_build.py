"""Fast crouch clips (ground + air) from GASP source poses + offsets in CHARACTER axes, timed by the curve sheet. Thread crouch-veloce-2.

Run headless (editor closed on this checkout, _ue59 lock held):
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file> <plan.json>" -unattended -nullrhi
Prints max 5 lines; full log + numeric checks in Saved/Crouch/crouch_fk_build.log.

plan.json (all angles deg, distances cm, frames at 60 fps counted from 0):
  "sources": {"stand": {"hints": [["idle","stand"]], "frame": 0}, ...}   first AnimSequence whose name has all words of a hint
  "poses": {"crouch": {"base": "<source or pose>", "pelvis": [fwd, right, up], "rot": {"<bone>": [pitch, roll, twist]},
                       "hands": "Fist", "feet": {"foot_l": [fwd, right, up]}}}
      rot is a delta about CHARACTER axes applied to the bone's component rotation (children follow), parents first:
      pitch + = bone tips forward, roll + = tilts to the character's right, twist + = turns toward the right.
      feet: moves the pinned foot target of that pose (ground clips).
  "clips": {"AS_GSTN_Crouch_Enter": {"folder": "/Game/...", "frames": 13, "pin_feet": true,
            "keys": [{"frame": 0, "pose": "stand"}, {"frame": 4, "pose": "crouch_over", "ease": "in"}, ...],
            "lag": {"<bone>": frames}}}       lag: the bone (and only it, as a local rotation) reads the timeline that many frames late
      ease of a key = the curve INTO that key: in (t^2, accelerating), out (1-(1-t)^2, explosive start), inout, linear, hold.
Between keys every bone's LOCAL rotation is slerped and translations lerped; then, with pin_feet, both legs are re-solved by
two-bone IK in their own knee plane so the feet stay on their (interpolated) targets, ik_foot_* follow the feet.
"""
import json, math, os, sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
FPS = 60
LOG = os.path.join(unreal.Paths.project_saved_dir(), "Crouch", "crouch_fk_build.log")
_lines = []


def log(s):
    _lines.append(str(s))


# ---------- quaternion math, plain Python, FQuat semantics: qmul(a, b) applies b first ----------
def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    r = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qinv(q)); return (r[0], r[1], r[2])


def qnorm(q):
    n = math.sqrt(sum(c * c for c in q)) or 1.0; return tuple(c / n for c in q)


def qaxis(axis, deg):
    a = math.radians(deg) * 0.5; s = math.sin(a)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(a))


def slerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0:
        b = tuple(-c for c in b); d = -d
    if d > 0.9995:
        return qnorm(tuple(x + (y - x) * t for x, y in zip(a, b)))
    th = math.acos(min(1.0, d)); s = math.sin(th)
    return tuple((math.sin((1 - t) * th) * x + math.sin(t * th) * y) / s for x, y in zip(a, b))


def from_to(u, v):
    u = vn(u); v = vn(v); c = vdot(u, v)
    if c < -0.99999:
        ax = vn(vcross(u, (1, 0, 0) if abs(u[0]) < 0.9 else (0, 1, 0))); return (ax[0], ax[1], ax[2], 0.0)
    x = vcross(u, v); return qnorm((x[0], x[1], x[2], 1.0 + c))


def vadd(a, b): return tuple(x + y for x, y in zip(a, b))
def vsub(a, b): return tuple(x - y for x, y in zip(a, b))
def vmul(a, s): return tuple(x * s for x in a)
def vdot(a, b): return sum(x * y for x, y in zip(a, b))
def vlen(a): return math.sqrt(vdot(a, a))
def vn(a): n = vlen(a) or 1.0; return vmul(a, 1.0 / n)
def vcross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


EASE = {"inout": lambda t: t * t * (3 - 2 * t), "in": lambda t: t * t, "out": lambda t: 1 - (1 - t) ** 2,
        "linear": lambda t: t, "hold": lambda t: 0.0}


# ---------- skeleton ----------
class Skel:
    def __init__(self, seq):
        self.skel = seq.get_editor_property("skeleton")
        ref = unreal.AnimPoseExtensions.get_reference_pose(self.skel)
        self.names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(ref)]
        LS, WS = unreal.AnimPoseSpaces.LOCAL, unreal.AnimPoseSpaces.WORLD
        self.parent = {}
        has = hasattr(unreal.AnimPoseExtensions, "get_parent_bone_name")
        wpos = {b: unreal.AnimPoseExtensions.get_bone_pose(ref, b, WS) for b in self.names}
        for i, b in enumerate(self.names):
            if has:
                p = str(unreal.AnimPoseExtensions.get_parent_bone_name(ref, b))
                self.parent[b] = p if p in wpos else None
                continue
            if i == 0:
                self.parent[b] = None; continue
            lt = unreal.AnimPoseExtensions.get_bone_pose(ref, b, LS)
            want = wpos[b].translation
            best = None
            for c in reversed(self.names[:i]):  # parent index < child index; nearest match wins
                pw = wpos[c]
                q = qt(pw.rotation); p = vt(pw.translation)
                guess = vadd(p, qrot(q, vt(lt.translation)))
                if vlen(vsub(guess, vt(want))) < 0.05:
                    best = c; break
            self.parent[b] = best or self.names[0]
        self.depth = {}
        for b in self.names:
            d, p = 0, self.parent[b]
            while p:
                d += 1; p = self.parent[p]
            self.depth[b] = d
        self.order = sorted(self.names, key=lambda b: self.depth[b])
        self.ref = {}
        for b in self.names:
            t = unreal.AnimPoseExtensions.get_bone_pose(ref, b, LS)
            self.ref[b] = (qnorm(qt(t.rotation)), vt(t.translation))


def qt(uq): return (uq.x, uq.y, uq.z, uq.w)
def vt(uv): return (uv.x, uv.y, uv.z)


def read_pose(sk, seq, frame):
    opts = unreal.AnimPoseEvaluationOptions()
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(seq, frame, opts)
    out = {}
    for b in sk.names:
        t = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL)
        out[b] = (qnorm(qt(t.rotation)), vt(t.translation))
    return out


def fk(sk, loc):
    w = {}
    for b in sk.order:
        q, t = loc[b]; p = sk.parent[b]
        if p is None:
            w[b] = (q, t)
        else:
            pq, pt = w[p]; w[b] = (qnorm(qmul(pq, q)), vadd(pt, qrot(pq, t)))
    return w


def axes(sk, loc):
    """Character axes from a standing pose: fwd = heel->ball, right = thigh_l->thigh_r, up = +Z."""
    w = fk(sk, loc)
    up = (0.0, 0.0, 1.0)
    f = vsub(vadd(w["ball_l"][1], w["ball_r"][1]), vadd(w["foot_l"][1], w["foot_r"][1]))
    fwd = vn((f[0], f[1], 0.0))
    r = vsub(w["thigh_r"][1], w["thigh_l"][1]); r = vsub(r, vmul(fwd, vdot(r, fwd)))
    right = vn((r[0], r[1], 0.0))
    return {"fwd": fwd, "right": right, "up": up,
            "pitch": vcross(up, fwd), "roll": vcross(up, right), "twist": vcross(fwd, right)}


def world_delta(ax, prt):
    p, r, t = prt
    return qmul(qaxis(ax["twist"], t), qmul(qaxis(ax["roll"], r), qaxis(ax["pitch"], p)))


TIP_RELATIVE = False  # plan "tip_relative": true (v2+): pitch/roll say where the bone's TIP goes, also on hanging bones


def tip_dir(sk, w, b):
    """Direction from the bone to its farthest child (its tip), component space; None for leaves."""
    kids = [c for c in sk.names if sk.parent.get(c) == b]
    if not kids:
        return None
    c = max(kids, key=lambda k: vlen(vsub(w[k][1], w[b][1])))
    return vn(vsub(w[c][1], w[b][1]))


def apply_rot(sk, loc, ax, rot):
    loc = dict(loc)
    w0 = fk(sk, loc)  # hanging is judged on the BASE pose: after a parent's rotation a calf can sit near horizontal and flip sign
    for b in sorted(rot, key=lambda x: sk.depth[x]):
        w = fk(sk, loc); p = sk.parent[b]
        pq = w[p][0] if p else (0, 0, 0, 1)
        prt = tuple(rot[b])
        tip = tip_dir(sk, w0, b) if TIP_RELATIVE else None
        if tip is not None and vdot(tip, ax["up"]) < 0:
            prt = (-prt[0], -prt[1], prt[2])  # hanging bone: v1 moved its tip backward for pitch + (axis up x fwd)
        d = world_delta(ax, prt)
        q, t = loc[b]
        loc[b] = (qnorm(qmul(qinv(pq), qmul(d, qmul(pq, q)))), t)
    return loc


def comp_to_parent(sk, loc, bone, vec):
    p = sk.parent[bone]
    if p is None:
        return vec
    return qrot(qinv(fk(sk, loc)[p][0]), vec)


def ax_vec(ax, v3):
    return vadd(vadd(vmul(ax["fwd"], v3[0]), vmul(ax["right"], v3[1])), vmul(ax["up"], v3[2]))


# ---------- leg IK ----------
def solve_leg(sk, loc, side, target_pos, target_rot):
    th, ca, fo = f"thigh_{side}", f"calf_{side}", f"foot_{side}"
    w = fk(sk, loc)
    H, K0, F0 = w[th][1], w[ca][1], w[fo][1]
    a, b = vlen(vsub(K0, H)), vlen(vsub(F0, K0))
    d_vec = vsub(target_pos, H); d = min(vlen(d_vec), a + b - 0.01); u = vn(d_vec)
    x = (a * a - b * b + d * d) / (2 * d); h = math.sqrt(max(0.0, a * a - x * x))
    pole = vsub(vsub(K0, H), vmul(u, vdot(vsub(K0, H), u)))
    if vlen(pole) < 1e-4:
        pole = (0.0, 0.0, 0.0)
    K = vadd(vadd(H, vmul(u, x)), vmul(vn(pole), h))
    stretch = vlen(d_vec) - (a + b - 0.01)
    # thigh
    tq = qmul(from_to(vsub(K0, H), vsub(K, H)), w[th][0])
    pq = w[sk.parent[th]][0]
    loc[th] = (qnorm(qmul(qinv(pq), tq)), loc[th][1])
    w = fk(sk, loc)
    cq = qmul(from_to(vsub(w[fo][1], w[ca][1]), vsub(vadd(H, vmul(u, d)), w[ca][1])), w[ca][0])
    loc[ca] = (qnorm(qmul(qinv(w[th][0]), cq)), loc[ca][1])
    w = fk(sk, loc)
    loc[fo] = (qnorm(qmul(qinv(w[ca][0]), target_rot)), loc[fo][1])
    return stretch


def sync_ik_bones(sk, loc):
    w = fk(sk, loc)
    for s in ("l", "r"):
        ik = f"ik_foot_{s}"
        if ik in loc and sk.parent.get(ik) in w:
            pq, pt = w[sk.parent[ik]]
            fq, ft = w[f"foot_{s}"]
            loc[ik] = (qnorm(qmul(qinv(pq), fq)), qrot(qinv(pq), vsub(ft, pt)))
    return loc


# ---------- hands ----------
def hand_locals(sk, pose_name):
    """Finger locals for both hands (fingers have no other animation in these clips), from the reference pose (hand_poses.py)."""
    try:
        import hand_poses as hp
    except Exception as ex:
        log(f"hand_poses missing: {ex}"); return {}
    ref_cs = fk(sk, sk.ref)
    ref_local = {b: sk.ref[b][0] for b in sk.names}
    out = {}
    for s in ("l", "r"):
        for b, q in hp.finger_locals(ref_cs, ref_local, s, pose_name).items():
            out[b] = (qnorm(q), sk.ref[b][1])
    return out


# ---------- build ----------
def find_seq(hints):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence')], package_paths=['/Game'],
                          recursive_paths=True)
    names = sorted((str(d.package_name) for d in reg.get_assets(flt)), key=len)
    for hint in hints:
        if isinstance(hint, str) and hint.startswith("/Game/"):
            if unreal.EditorAssetLibrary.does_asset_exist(hint):
                return hint
            continue
        for n in names:
            low = n.rsplit('/', 1)[-1].lower()
            if all(w in low for w in hint):
                return n
    return None


def build_poses(sk, plan, ax):
    seqs, poses, feet = {}, {}, {}
    for name, s in plan["sources"].items():
        path = find_seq(s["hints"])
        if not path:
            raise RuntimeError(f"source {name}: no match for {s['hints']}")
        seqs[name] = path
        poses[name] = read_pose(sk, unreal.load_asset(path), s.get("frame", 0))
        log(f"source {name} = {path} f{s.get('frame', 0)}")
    pending = dict(plan["poses"])
    while pending:
        progressed = False
        for name, p in list(pending.items()):
            if p["base"] not in poses:
                continue
            loc = dict(poses[p["base"]])
            if "pelvis" in p:
                q, t = loc["pelvis"]
                loc["pelvis"] = (q, vadd(t, comp_to_parent(sk, loc, "pelvis", ax_vec(ax, p["pelvis"]))))
            if p.get("rot"):
                loc = apply_rot(sk, loc, ax, p["rot"])
            if p.get("hands"):
                loc.update(hand_locals(sk, p["hands"]))
            w = fk(sk, loc)
            fb = feet.get(p["base"]) or {s: (w[f"foot_{s}"][1], w[f"foot_{s}"][0]) for s in ("l", "r")}
            feet[name] = {s: (vadd(fb[s][0], ax_vec(ax, p.get("feet", {}).get(f"foot_{s}", (0, 0, 0)))),
                              qmul(world_delta(ax, p.get("foot_rot", {}).get(f"foot_{s}", (0, 0, 0))), fb[s][1]))
                          for s in ("l", "r")}
            poses[name] = loc
            del pending[name]; progressed = True
        if not progressed:
            raise RuntimeError(f"pose bases missing: {list(pending)}")
    for s in plan["sources"]:
        w = fk(sk, poses[s]); feet.setdefault(s, {x: (w[f"foot_{x}"][1], w[f"foot_{x}"][0]) for x in ("l", "r")})
    return poses, feet


def sample(keys, poses, feet, t, bone):
    if t <= keys[0]["frame"]:
        k = keys[0]; return poses[k["pose"]][bone], feet[k["pose"]]
    for a, b in zip(keys, keys[1:]):
        if t <= b["frame"]:
            u = (t - a["frame"]) / float(b["frame"] - a["frame"])
            e = EASE[b.get("ease", "inout")](u)
            (qa, ta), (qb, tb) = poses[a["pose"]][bone], poses[b["pose"]][bone]
            fa, fb = feet[a["pose"]], feet[b["pose"]]
            ft = {s: (vadd(fa[s][0], vmul(vsub(fb[s][0], fa[s][0]), e)), slerp(fa[s][1], fb[s][1], e)) for s in ("l", "r")}
            return (slerp(qa, qb, e), vadd(ta, vmul(vsub(tb, ta), e))), ft
    k = keys[-1]; return poses[k["pose"]][bone], feet[k["pose"]]


def build_clip(sk, name, c, poses, feet, ax):
    n = c["frames"]
    if (n - 1) % 2:
        n += 1; log(f"{name}: frames raised to {n} (even 60 fps intervals)")
    lag = c.get("lag", {})
    loop = c.get("loop", False)
    frames = []
    max_stretch = 0.0
    for f in range(n):
        loc = {}
        for b in sk.names:
            t = f - lag.get(b, 0)
            if loop:
                t = t % (n - 1)
            loc[b], ftgt = sample(c["keys"], poses, feet, max(0, t), b)
        _, ftgt = sample(c["keys"], poses, feet, f, "pelvis")
        if c.get("pin_feet"):
            for s in ("l", "r"):
                max_stretch = max(max_stretch, solve_leg(sk, loc, s, ftgt[s][0], ftgt[s][1]))
            loc = sync_ik_bones(sk, loc)
        frames.append(loc)
    # numeric checks
    w0 = fk(sk, frames[0])
    pel = [fk(sk, fr)["pelvis"][1][2] - w0["pelvis"][1][2] for fr in frames]
    slide = 0.0
    for fr in frames:
        w = fk(sk, fr)
        for s in ("l", "r"):
            slide = max(slide, vlen(vsub(w[f"ball_{s}"][1], w0[f"ball_{s}"][1])) if c.get("pin_feet") and c.get("check_slide", True) else 0)
    log(f"{name}: {n} keys, pelvis dz per frame " + " ".join(f"{v:.1f}" for v in pel))
    log(f"{name}: max ball drift {slide:.1f} cm, leg overreach {max(0.0, max_stretch):.1f} cm")
    return frames


def write_clip(sk, folder, name, frames):
    path = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        seq = unreal.load_asset(path)
    else:
        fac = unreal.AnimSequenceFactory(); fac.set_editor_property("target_skeleton", sk.skel)
        seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimSequence, fac)
    ctrl = seq.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("fast crouch"), False)
    ctrl.set_frame_rate(unreal.FrameRate(FPS, 1), False)
    ctrl.set_number_of_frames(unreal.FrameNumber(len(frames) - 1), False)
    model = None
    try:
        have = {str(n) for n in unreal.AnimationLibrary.get_animation_track_names(seq)}
    except Exception:
        have = set()
    for b in sk.names:
        if b not in have:  # set_bone_track_keys does nothing on a missing track (round 1 wrote 0 bone tracks)
            fn = "add_bone_track" if hasattr(ctrl, "add_bone_track") else "add_bone_curve"
            getattr(ctrl, fn)(b, False)
        rots = [unreal.Quat(*fr[b][0]) for fr in frames]
        pos = [unreal.Vector(*fr[b][1]) for fr in frames]
        ctrl.set_bone_track_keys(b, pos, rots, [unreal.Vector(1, 1, 1)] * len(frames), False)
    ctrl.close_bracket(False)
    try:
        log(f"{name}: bone tracks = {len(unreal.AnimationLibrary.get_animation_track_names(seq))}")
    except Exception as ex:
        log(f"{name}: bone tracks unreadable ({ex})")
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    return path


def main():
    plan_path = os.environ.get("CROUCH_PLAN") or sys.argv[-1]  # env var: the project path has spaces
    plan = json.load(open(plan_path, encoding="utf-8"))
    aset = unreal.get_default_object(unreal.AnimationSettings)
    try:
        aset.set_editor_property("default_frame_rate", unreal.FrameRate(FPS, 1))
    except Exception as ex:
        log(f"anim settings rate: {ex}")
    first = find_seq(next(iter(plan["sources"].values()))["hints"])
    sk = Skel(unreal.load_asset(first))
    stand = read_pose(sk, unreal.load_asset(find_seq(plan["sources"][plan.get("axes_from", "stand")]["hints"])), 0)
    ax = axes(sk, stand)
    log("axes " + " ".join(f"{k}={tuple(round(v, 2) for v in ax[k])}" for k in ("fwd", "right", "up")))
    global TIP_RELATIVE
    TIP_RELATIVE = plan.get("tip_relative", False)
    log(f"tip_relative={TIP_RELATIVE}")
    poses, feet = build_poses(sk, plan, ax)
    only = plan.get("only")
    made = []
    for name, c in plan["clips"].items():
        if only and name not in only:
            continue
        frames = build_clip(sk, name, c, poses, feet, ax)
        if not plan.get("dry_run"):
            made.append(write_clip(sk, c["folder"], name, frames))
    log(f"made {len(made)}: " + ", ".join(m.rsplit('/', 1)[-1] for m in made))


try:
    main()
    status = "ESITO: OK"
except Exception as ex:
    import traceback
    log(traceback.format_exc()); status = f"ESITO: ERRORE {ex}"
os.makedirs(os.path.dirname(LOG), exist_ok=True)
with open(LOG, "w", encoding="utf-8") as fh:
    fh.write("\n".join(_lines + [status]) + "\n")
for line in ([l for l in _lines if l.startswith(("made", "axes"))][:3] + [status, LOG])[:5]:
    print(line)
