"""Write a solved take (pose_solver.py output: component-space bones per frame) into Unreal, no Cascadeur.

    python ue_write_take.py <take>_bones.json <AssetName> [--template=AS_Pilot_GanonFTilt_V2]

1. samples the template take's frame 0 in local and component space and finds each bone's parent from the data
   (comp_b = comp_parent * local_b), so no hierarchy is hard-coded;
2. converts the solved bones to local tracks (parents first; a parent we do not solve uses the template's own track);
3. writes them into /Game/AnimPipeline/Pilot/<AssetName> (a copy of the template; ue_take_writer.py, saved);
4. samples the result back and reports the worst joint error against the solve."""
import json, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue, legibility as L
from pose_solver import q2m, m2q

PILOT = "/Game/AnimPipeline/Pilot/"


def qconj(q):
    return np.array([-q[0], -q[1], -q[2], q[3]])


def sample(asset, bones, frames, space):
    out = {}
    for s0 in range(0, len(frames), 8):
        r = ue.call("sample_pose", {"assetPath": asset, "boneNames": bones, "frames": frames[s0:s0 + 8], "space": space})
        for s in r["samples"]:
            out[s["frame"]] = {b["name"]: [b["location"][k] for k in "xyz"] + [b["rotation"][k] for k in ("pitch", "yaw", "roll")] for b in s["bones"]}
    return out


def main():
    src, name = sys.argv[1], sys.argv[2]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[3:] if a.startswith("--"))
    tmpl = PILOT + (opt.get("template") or "AS_Pilot_GanonFTilt_V2")
    dest = PILOT + name
    S = json.load(open(os.path.join(HERE, src)))["frames"]
    n = len(S); solved = sorted(S["0"])
    extra = ["root"]
    bones = solved + extra
    loc = sample(tmpl, bones, [0], "local")[0]; comp = sample(tmpl, bones, [0], "component")[0]
    T = lambda fr, b: (np.array(fr[b][:3]), L.rot_to_quat(*fr[b][3:6]))
    # parents from the data
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
    bad = {b: p for b, p in parent.items() if p == "root" and b != "pelvis"}
    print("parents:", {b: parent[b] for b in ("pelvis", "spine_01", "head", "hand_l", "upperarm_twist_01_l", "lowerarm_twist_02_r", "foot_l", "ball_l")})
    if bad:
        print("WARNING bones parented to root (check):", bad)
    root_p, root_q = T(comp, "root")
    tracks = {}
    for b in solved:
        keys = []
        for f in range(n):
            pb, qb = np.array(S[str(f)][b][0]), np.array(S[str(f)][b][1])
            p = parent[b]
            if p in S[str(f)]:
                pp, pq = np.array(S[str(f)][p][0]), np.array(S[str(f)][p][1])
            else:
                pp, pq = root_p, root_q
            lq = L.qmul(qconj(pq), qb); lq = lq / np.linalg.norm(lq)
            lp = L.qrot(qconj(pq), pb - pp)
            keys.append([*lp.tolist(), *lq.tolist()])
        tracks[b] = keys
    # the root: held where the conversion assumed it (a template from Cascadeur moves its root under the pelvis; keeping that
    # track would shift every bone by the template's root motion)
    tracks["root"] = [[*root_p.tolist(), *root_q.tolist()] for _ in range(n)]
    tmp =os.path.join(HERE, "sheets", "raw", f"{name}_tracks.json")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    json.dump({"keys": n, "bones": tracks}, open(tmp, "w"))
    r = ue.call("run_python_file", {"filePath": (HERE + "/ue_take_writer.py").replace("\\", "/"), "entryPoint": "write",
                                    "kwargs": {"src": tmp.replace("\\", "/"), "template": tmpl, "dest": dest}})
    print("write:", r.get("result"), ("errors: " + str(r.get("log_output"))[:600]) if r.get("logErrorCount") else "")
    info = ue.call("read_anim_sequence", {"assetPath": dest})
    print("asset:", info.get("numFrames"), info.get("frameRate"), round(info.get("sequenceLength", 0), 4))
    back = sample(dest, solved, list(range(n)), "component")
    worst = (0, None)
    for f in range(n):
        for b in ("pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r", "ball_l", "calf_r"):
            d = float(np.linalg.norm(np.array(back[f][b][:3]) - np.array(S[str(f)][b][0])))
            if d > worst[0]:
                worst = (d, (f, b))
    print(f"read-back vs solve: worst joint {worst[0]:.2f} cm at {worst[1]}")


if __name__ == "__main__":
    main()
