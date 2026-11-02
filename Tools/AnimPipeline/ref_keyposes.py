"""Key poses of a reference as numbers (README, "Replicating a reference: key poses first", step 1).

    python ref_keyposes.py refs/<name> [support side l|r]   -> refs/<name>/keyposes.json and a printed table

Per still (reference.json "stills"): every 2D segment's direction in the side view (deg; 0 = along the attack, 90 = up,
180 = back, -90 = down) and its length in body lengths, plus the joints relative to the mid-hip and to the support ankle
(body lengths, x along the attack, y up). Same normalisation as motion_ref.py (each pose by its own torso + support thigh +
support shin), so the numbers are what the gate compares. A model builds its key poses from these directions; the 2D length
ratio of a segment (vs its longest over the stills) says how far it points out of the side plane.
"""
import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_ref as MR  # noqa: E402


def main(ref_dir, sup="r"):
    ref_dir = ref_dir if os.path.isabs(ref_dir) else os.path.join(HERE, ref_dir)
    R, seq = MR.load_ref(ref_dir)
    scales = [MR._scale(p, sup) for _, _, p in seq if p]
    med = float(np.median([s for s in scales if s > 0]))
    out = {"_doc": __doc__.strip().splitlines()[0], "support": sup, "poses": []}
    seglen_max = {}
    rows = []
    for ph, img, p in seq:
        if not p:
            continue
        sc = MR._scale(p, sup) or med
        h = MR.norm(p, p["mid_hip"], sc)
        g = MR.norm(p, p[f"ankle_{sup}"], sc) if f"ankle_{sup}" in p else None
        segs = {}
        for s, (a, b) in MR.SEGS.items():
            if a in h and b in h:
                d = h[b] - h[a]
                segs[s] = {"deg": round(math.degrees(math.atan2(d[1], d[0])), 1), "len": round(float(np.linalg.norm(d)), 3)}
                seglen_max[s] = max(seglen_max.get(s, 0.0), segs[s]["len"])
        rows.append((ph, img, segs))
        out["poses"].append({"phase": ph, "image": img, "scale_px": round(sc, 1), "segments": segs,
                             "joints_hip": {k: [round(float(v[0]), 3), round(float(v[1]), 3)] for k, v in h.items()},
                             "joints_ground": ({k: [round(float(v[0]), 3), round(float(v[1]), 3)] for k, v in g.items()} if g else None)})
    for pose in out["poses"]:
        for s, v in pose["segments"].items():
            v["len_ratio"] = round(v["len"] / seglen_max[s], 2) if seglen_max[s] > 0 else None
    json.dump(out, open(os.path.join(ref_dir, "keyposes.json"), "w"), indent=1)
    names = list(MR.SEGS)
    print("seg     " + " ".join(f"{(ph or img)[:12]:>14}" for ph, img, _ in rows))
    for s in names:
        print(f"{s:<7} " + " ".join(f"{seg[s]['deg']:>7.0f} ({seg[s]['len']:.2f})" if s in seg else f"{'-':>14}" for _, _, seg in rows))
    print(f"written {os.path.join(ref_dir, 'keyposes.json')}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "r")
