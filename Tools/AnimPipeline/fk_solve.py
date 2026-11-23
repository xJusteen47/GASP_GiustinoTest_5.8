"""Solve FK rotation deltas for wanted joint moves (card `edits`), from the rig's own Jacobian at each key.
    python fk_solve.py <card> <base_take> '[[ctrl, bone, frame, [dx,dy,dz]], ...]'
Two probe builds (+10 deg on Rotation Y, then Z, at every target key), sampled with legibility.sample; least squares per
key; prints the edits as JSON (cm in component space: X left, Y forward, Z up). Targets must lie on the joint's sphere
around its parent, or the solve compromises. Needs the editor; probe assets AS_Jac1/2_Build are scratch."""
import json, subprocess, sys, numpy as np, legibility as L, os
card, base, targets = sys.argv[1], sys.argv[2], json.loads(sys.argv[3])   # targets: [[ctrl, bone, frame, [dx,dy,dz]]]
c0 = json.load(open(card)); J = {}
for ax in (1, 2):
    c = dict(c0); c["move"] = f"_jac{ax}"; c["anim"] = f"/Game/AnimPipeline/Rig/AS_Jac{ax}_Build"
    d = [0, 0, 0]; d[ax] = 10
    c["edits"] = list(c0.get("edits", [])) + [{"control": t[0], "attr": "Rotation", "frames": [t[2]], "delta": d} for t in targets]
    json.dump(c, open(f"cards/_jac{ax}.json", "w"))
    subprocess.run([sys.executable, "-m", "animpipe", "build", f"cards/_jac{ax}.json"], capture_output=True)
    p = f"sheets/raw/Jac{ax}_Build_legbones.json"
    if os.path.exists(p): os.remove(p)
    L.sample(f"/Game/AnimPipeline/Rig/AS_Jac{ax}_Build", f"Jac{ax}_Build")
    J[ax] = json.load(open(p))["frames"]
A = json.load(open(f"sheets/raw/{base}_legbones.json"))["frames"]
out = []
for ctrl, bone, f, want in targets:
    a = np.array(A[str(f)][bone][:3])
    M = np.stack([(np.array(J[ax][str(f)][bone][:3]) - a) / 10 for ax in (1, 2)], 1)   # cm per degree
    x, *_ = np.linalg.lstsq(M, np.array(want, float), rcond=None)
    out.append({"control": ctrl, "attr": "Rotation", "frames": [f], "delta": [0, round(float(x[0]), 1), round(float(x[1]), 1)],
                "_got": np.round(M @ x, 1).tolist()})
print(json.dumps(out))
