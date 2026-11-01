"""Write a solved take (pose_solver.py output: component-space bones per frame) into Unreal, no Cascadeur.

    python ue_write_take.py <take>_bones.json <AssetName> [--template=AS_Pilot_GanonFTilt_V2] [--no-write]

1. samples the template take's frame 0 in local and component space (cached in sheets/raw/<Template>_frame0.json) and finds
   each bone's parent from the data (comp_b = comp_parent * local_b), so no hierarchy is hard-coded;
2. converts the solved bones to local tracks, parents first. ROTATIONS ONLY: every bone but root and pelvis keeps the
   template's local translation (the skeleton's bone lengths). The solver places each bone at its own rig point, and the
   model's points do not keep the skeleton's distances (Ganondorf v3: head 1.4-22.7 cm from neck_02, reference 5.4), so
   writing the solved positions detached the head from the neck. The solved positions now only aim the rotations;
3. gates on bone_lengths.py (any bone off its reference length by > 0.1 cm stops the write) and reports how far the hands,
   feet and head moved from the solve once the lengths are fixed (the model's own stretch);
4. writes them into /Game/AnimPipeline/Pilot/<AssetName> (a copy of the template; ue_take_writer.py, saved);
5. samples the result back: joint error against the expected FK, and the bone-length check again on the sampled bones.
--no-write stops after step 3 (needs the template cache, or Unreal for the sampling)."""
import json, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import legibility as L
import bone_lengths as BL

PILOT = "/Game/AnimPipeline/Pilot/"
ENDS = ("head", "hand_l", "hand_r", "foot_l", "foot_r", "ball_l", "ball_r", "spine_05")


def qconj(q):
    return np.array([-q[0], -q[1], -q[2], q[3]])


def sample(asset, bones, frames, space):
    import ue
    out = {}
    for s0 in range(0, len(frames), 8):
        r = ue.call("sample_pose", {"assetPath": asset, "boneNames": bones, "frames": frames[s0:s0 + 8], "space": space})
        for s in r["samples"]:
            out[s["frame"]] = {b["name"]: [b["location"][k] for k in "xyz"] + [b["rotation"][k] for k in ("pitch", "yaw", "roll")] for b in s["bones"]}
    return out


def template_frame0(tmpl, bones):
    """{"local"|"component": {bone: [x, y, z, qx, qy, qz, qw]}} of the template's frame 0, cached."""
    path = os.path.join(HERE, "sheets", "raw", f"{tmpl.rsplit('/', 1)[-1]}_frame0.json")
    if os.path.exists(path):
        d = json.load(open(path))
        if set(bones) <= set(d["local"]):
            return d
    q = lambda fr: {b: [*v[:3], *L.rot_to_quat(*v[3:6]).tolist()] for b, v in fr.items()}
    d = {"template": tmpl, "local": q(sample(tmpl, bones, [0], "local")[0]),
         "component": q(sample(tmpl, bones, [0], "component")[0])}
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(d, open(path, "w"))
    return d


def find_parents(loc, comp, solved, bones):
    T = lambda fr, b: (np.array(fr[b][:3]), np.array(fr[b][3:7]))
    parent = {}
    for b in solved:
        pb, qb = T(comp, b); lb, lq = T(loc, b)
        best = ("root", 1e9)
        for p in bones:
            if p == b:
                continue
            pp, pq = T(comp, p)
            err = np.linalg.norm(pp + L.qrot(pq, lb) - pb) + 10 * L.qangle(L.qmul(pq, lq), qb) / 57.3
            if err < best[1]:
                best = (p, err)
        parent[b] = best[0]
    return parent


def to_tracks(S, parent, ref_t, root_p, root_q):
    """Local tracks: rotation from the solve, translation = reference except root/pelvis (step 2)."""
    n = len(S); tracks = {}
    for b in S["0"]:
        keys = []
        for f in range(n):
            pb, qb = np.array(S[str(f)][b][0]), np.array(S[str(f)][b][1])
            p = parent[b]
            pp, pq = (np.array(S[str(f)][p][0]), np.array(S[str(f)][p][1])) if p in S[str(f)] else (root_p, root_q)
            lq = L.qmul(qconj(pq), qb); lq = lq / np.linalg.norm(lq)
            lp = L.qrot(qconj(pq), pb - pp) if b in BL.FREE else ref_t[b]
            keys.append([*np.asarray(lp, float).tolist(), *lq.tolist()])
        tracks[b] = keys
    # the root: held where the conversion assumed it (a template from Cascadeur moves its root under the pelvis; keeping that
    # track would shift every bone by the template's root motion)
    tracks["root"] = [[*root_p.tolist(), *root_q.tolist()] for _ in range(n)]
    return tracks


def fk(tracks, parent, f):
    """Component (position, quat) at frame f of the local tracks."""
    out = {"root": (np.array(tracks["root"][f][:3]), np.array(tracks["root"][f][3:7]))}

    def get(b):
        if b not in out:
            pp, pq = get(parent[b])
            k = tracks[b][f]
            out[b] = (pp + L.qrot(pq, np.array(k[:3])), L.qmul(pq, np.array(k[3:7])))
        return out[b]
    for b in tracks:
        get(b)
    return out


def main():
    src, name = sys.argv[1], sys.argv[2]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[3:] if a.startswith("--"))
    tmpl = PILOT + (opt.get("template") or "AS_Pilot_GanonFTilt_V2")
    dest = PILOT + name
    S = json.load(open(os.path.join(HERE, src)))["frames"]
    n = len(S); solved = sorted(S["0"])
    bones = solved + ["root"]
    t0 = template_frame0(tmpl, bones)
    loc, comp = t0["local"], t0["component"]
    parent = find_parents(loc, comp, solved, bones)
    bad = {b: p for b, p in parent.items() if p == "root" and b != "pelvis"}
    print("parents:", {b: parent[b] for b in ("pelvis", "spine_01", "neck_02", "head", "hand_l", "upperarm_twist_01_l", "lowerarm_twist_02_r", "foot_l", "ball_l")})
    if bad:
        print("WARNING bones parented to root (check):", bad)
    ref_t = {b: np.array(loc[b][:3]) for b in solved}
    tracks = to_tracks(S, parent, ref_t, np.array(comp["root"][:3]), np.array(comp["root"][3:7]))
    tmp = os.path.join(HERE, "sheets", "raw", f"{name}_tracks.json")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    json.dump({"keys": n, "template": tmpl, "bones": tracks}, open(tmp, "w"))
    ref_len = {b: float(np.linalg.norm(v)) for b, v in ref_t.items()}
    gate = BL.check(BL.lengths(tmp), ref_len)
    print(BL.report(gate, len(tracks)))
    if gate:
        print("write stopped: bone lengths changed"); sys.exit(1)
    drift = {b: (0.0, 0) for b in ENDS}
    for f in range(n):
        F = fk(tracks, parent, f)
        for b in ENDS:
            d = float(np.linalg.norm(F[b][0] - np.array(S[str(f)][b][0])))
            if d > drift[b][0]:
                drift[b] = (d, f)
    print("moved from the solve by the fixed lengths (model stretch), worst cm @frame:",
          {b: f"{d:.1f}@{f}" for b, (d, f) in drift.items()})
    if "no-write" in opt:
        return
    import ue
    r = ue.call("run_python_file", {"filePath": (HERE + "/ue_take_writer.py").replace("\\", "/"), "entryPoint": "write",
                                    "kwargs": {"src": tmp.replace("\\", "/"), "template": tmpl, "dest": dest}})
    print("write:", r.get("result"), ("errors: " + str(r.get("log_output"))[:600]) if r.get("logErrorCount") else "")
    info = ue.call("read_anim_sequence", {"assetPath": dest})
    print("asset:", info.get("numFrames"), info.get("frameRate"), round(info.get("sequenceLength", 0), 4))
    back = sample(dest, solved, list(range(n)), "component")
    worst = (0, None)
    for f in range(n):
        F = fk(tracks, parent, f)
        for b in ENDS + ("pelvis", "calf_r"):
            d = float(np.linalg.norm(np.array(back[f][b][:3]) - F[b][0]))
            if d > worst[0]:
                worst = (d, (f, b))
    print(f"read-back vs written FK: worst joint {worst[0]:.2f} cm at {worst[1]}")
    bk = os.path.join(HERE, "sheets", "raw", f"{name}_readback.json")
    json.dump({"frames": {str(f): back[f] for f in range(n)}}, open(bk, "w"))
    after = BL.check(BL.lengths(bk), ref_len)
    print("read-back " + BL.report(after, len(solved)))
    if after:
        sys.exit(1)


if __name__ == "__main__":
    main()
