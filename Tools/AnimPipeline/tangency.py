"""Tangency (Solid Drawing; owner's animation-principles list, 31/10). Two silhouette contours that run parallel and touch over
a long stretch flatten the form even when the limb is technically visible: an arm laid along the torso's edge, a forearm in
line with a thigh, both legs or both arms reading as one shape.

Per key pose and view (legibility's capsule parts, cameras and renderer; offline on sheets/raw/<take>_legbones.json):
  - each part's own silhouette mask (occluded or not: a hidden edge that coincides with another one still reads as one line);
  - for every limb part A (upper arm, forearm, hand, thigh, shin, foot) and every other part B except A's joint neighbours
    (shoulder, elbow, wrist, hip, knee, ankle, neck): A's outline pixels within TOL_CM (screen-scaled,
    at least 1.5 px) of B's outline whose outline directions agree within ANG_DEG;
  - coverage = the share of A's length (its mask's extent along its main axis) that those pixels cover;
  - a tangency = coverage > COVER_MIN.
Views: the six orthographic views (front, right, back, left, top, bottom) plus 3/4s, and the game rings (Follow, rail, PvP).
Key poses: frame 0, the anticipation key, contact, mid-hold, hold end, mid-recovery, FAF (or the frames given).

Usage: python tangency.py <take> [f1,f2,...]   -> sheets/<take>_tangency.json, prints the worst views and frames."""
import sys, os, json, math, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility as L

TOL_CM = 1.5
ANG_DEG = 15.0
COVER_MIN = 0.25
LIMBS = ["upperarm_l", "lowerarm_l", "hand_l", "upperarm_r", "lowerarm_r", "hand_r", "thigh_l", "shin_l", "foot_l", "thigh_r", "shin_r", "foot_r"]
JOINED = {frozenset(p) for p in [("upperarm_l", "lowerarm_l"), ("lowerarm_l", "hand_l"), ("upperarm_r", "lowerarm_r"), ("lowerarm_r", "hand_r"),
                                 ("thigh_l", "shin_l"), ("shin_l", "foot_l"), ("thigh_r", "shin_r"), ("shin_r", "foot_r"),
                                 ("thigh_l", "torso"), ("thigh_r", "torso"), ("head", "torso"),
                                 ("upperarm_l", "torso"), ("upperarm_r", "torso")]}   # the shoulder: the capsule continues the torso's line (v11: 63 of 140 view-frames)
SIX = [{"name": n, "kind": "ortho", "az": az, "el": el} for n, az, el in
       (("front", 0, 0.0), ("right", 270, 0.0), ("back", 180, 0.0), ("left", 90, 0.0), ("top", 0, 89.0), ("bottom", 0, -89.0))]


def views():
    rings = [v for v in L.VIEWS if v["kind"] == "persp" or v["az"] in (45, 135, 225, 315)]   # 3/4s + game rings
    return SIX + rings


def normals(m):
    """Outline direction per pixel: the gradient of a 3x3-blurred mask (no scipy)."""
    f = m.astype(np.float32)
    p = np.pad(f, 1, mode="edge")
    b = sum(p[1 + dy:1 + dy + f.shape[0], 1 + dx:1 + dx + f.shape[1]] for dy in (-1, 0, 1) for dx in (-1, 0, 1)) / 9.0
    gy, gx = np.gradient(b)
    return gx, gy


def tangencies(depth, view):
    tol = max(TOL_CM * L.px_per_cm(view), 1.5)
    cosmax = math.cos(math.radians(ANG_DEG))
    masks = {p: np.isfinite(depth[k]) for k, p in enumerate(L.PNAMES)}
    bnd, nrm = {}, {}
    for p, m in masks.items():
        if m.sum() < 6:
            continue
        ys, xs = np.nonzero(L.boundary(m))
        gx, gy = normals(m)
        n = np.stack([gx[ys, xs], gy[ys, xs]], axis=1); n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)
        bnd[p] = np.stack([xs, ys], axis=1).astype(np.float32); nrm[p] = n
    out = []
    for a in LIMBS:
        if a not in bnd:
            continue
        ys, xs = np.nonzero(masks[a]); P = np.stack([xs, ys], axis=1).astype(np.float32)
        c = P.mean(axis=0); _, _, vt = np.linalg.svd(P - c, full_matrices=False); ax = vt[0]
        proj = (P - c) @ ax; lo, hi = float(proj.min()), float(proj.max()); length = hi - lo + 1
        if length < 4:
            continue
        for b in bnd:
            if b == a or frozenset((a, b)) in JOINED:
                continue
            A, B = bnd[a], bnd[b]
            d2 = ((A[:, None, :] - B[None, :, :]) ** 2).sum(axis=2)
            j = d2.argmin(axis=1); near = d2[np.arange(len(A)), j] <= tol * tol
            if not near.any():
                continue
            par = np.abs((nrm[a][near] * nrm[b][j[near]]).sum(axis=1)) >= cosmax
            pts = A[near][par]
            if not len(pts):
                continue
            bins = np.unique(np.floor((pts - c) @ ax - lo).astype(int))
            cover = len(bins) / length
            if cover > COVER_MIN:
                out.append({"limb": a, "other": b, "cover": round(float(cover), 2), "px": int(len(pts))})
    return out


def frame_rows(f, fr):
    prims = L.part_prims(fr)
    return [{"frame": f, "view": v["name"], **t} for v in views() for t in tangencies(L.render(prims, v), v)]


def key_frames(take, n):
    sh = json.load(open(os.path.join(L.HERE, "move_sheets.json"))).get(take, {})
    c0, c1 = sh.get("contact", [1, 2]); faf = min(sh.get("faf", n - 1), n - 1); hw = sh.get("hold_window", [c0, c1])
    ks = {0, max(c0 - 2, 0), c0, (hw[0] + hw[1]) // 2, hw[1], (hw[1] + faf) // 2, faf}
    return sorted(k for k in ks if 0 <= k < n)


def run(take, frames_sel=None):
    t0 = time.time()
    L.configure(take)
    frames = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))["frames"]
    ks = frames_sel or key_frames(take, len(frames))
    res = []
    n = min(L.workers(), len(ks))
    if n <= 1:
        for f in ks:
            res += frame_rows(f, frames[str(f)])
    else:                                    # one process per key frame, 6 cores left free (legibility.workers)
        import multiprocessing as mp
        with mp.get_context("spawn").Pool(n, initializer=L._init_worker, initargs=(take,)) as pool:
            for rows in pool.starmap(frame_rows, [(f, frames[str(f)]) for f in ks], chunksize=1):
                res += rows
    res.sort(key=lambda r: -r["cover"])
    # one row per unordered pair, frame and view (A along B and B along A are the same tangency)
    best = {}
    for r in res:
        k = (frozenset((r["limb"], r["other"])), r["frame"], r["view"])
        if k not in best or r["cover"] > best[k]["cover"]:
            best[k] = r
    res = sorted(best.values(), key=lambda r: -r["cover"])
    by_pair = {}
    for r in res:
        by_pair.setdefault(tuple(sorted((r["limb"], r["other"]))), []).append(r)
    summary = [{"pair": f"{a} / {b}", "views": len(v), "frames": sorted({x["frame"] for x in v}), "worst": max(v, key=lambda x: x["cover"])}
               for (a, b), v in sorted(by_pair.items(), key=lambda kv: -max(x["cover"] for x in kv[1]))]
    out = {"take": take, "frames": ks, "views": [v["name"] for v in views()], "thresholds": {"tol_cm": TOL_CM, "ang_deg": ANG_DEG, "cover_min": COVER_MIN},
           "summary": summary, "rows": res, "seconds": round(time.time() - t0, 1)}
    json.dump(out, open(os.path.join(L.SHEETS, f"{take}_tangency.json"), "w"), indent=1)
    return out


if __name__ == "__main__":
    take = sys.argv[1]
    sel = [int(x) for x in sys.argv[2].split(",")] if len(sys.argv) > 2 else None
    o = run(take, sel)
    print(f"tangency {take}: key frames {o['frames']}, {len(o['views'])} views, {len(o['rows'])} tangencies, {o['seconds']} s")
    for s in o["summary"][:12]:
        w = s["worst"]
        print(f"  {s['pair']:24s} {s['views']:3d} view-frames, frames {s['frames']}; worst {w['cover']:.0%} at f{w['frame']} {w['view']}")
