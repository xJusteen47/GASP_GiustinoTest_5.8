"""Two captures of the same run, frame by frame (C5 2026-11-24): trajectory relative to the first frame and pose relative to the actor,
by frame range. Answers "does a capture in the live editor session give the same motion as one in a fresh editor?" against the
baseline "two fresh editors". No editor.

    python Tools/AnimPipeline/capture_pair_diff.py <capA>/<run> <capB>/<run> <sequence> [--ranges=0-60,60-138,138-160,160-230]"""
import json
import math
import sys

KEYS = ("pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r")


def load(p):
    return {r["f"]: r for r in (json.loads(x) for x in open(p, encoding="utf-8") if x.strip())}


def diff(a_dir, b_dir, seq, ranges):
    a, b = load(f"{a_dir}/{seq}/frames.jsonl"), load(f"{b_dir}/{seq}/frames.jsonl")
    a0, b0 = a[min(a)]["loc"], b[min(b)]["loc"]
    out = []
    for lo, hi in ranges:
        wt = wp = 0.0
        where = None
        for f in range(lo, hi):
            if f in a and f in b and "body" in a[f] and "body" in b[f]:
                wt = max(wt, math.dist([a[f]["loc"][i] - a0[i] for i in range(3)], [b[f]["loc"][i] - b0[i] for i in range(3)]))
                for k in KEYS:
                    d = math.dist(a[f]["body"][k], b[f]["body"][k])
                    if d > wp:
                        wp, where = d, f"f{f} {k}"
        out.append({"frames": [lo, hi], "traj_cm": round(wt, 2), "pose_cm": round(wp, 2), "where": where})
    return {"start_a": a0, "start_b": b0, "ranges": out}


if __name__ == "__main__":
    rg = next((x.split("=", 1)[1] for x in sys.argv if x.startswith("--ranges=")), "0-60,60-138,138-160,160-200,200-230")
    ranges = [tuple(int(v) for v in r.split("-")) for r in rg.split(",")]
    print(json.dumps(diff(sys.argv[1], sys.argv[2], sys.argv[3], ranges)))
