"""Measure UFD hitbox GIFs (side view, 1 GIF frame = 1 game frame): striking-limb extension per frame, impact spacing,
hold and recoil. The limb extent is the silhouette's reach toward the attack (character and grey hurtbox capsules),
with the coloured hitbox bubbles masked out; the attack side comes from where the hitboxes appear.
Usage: python ufd_measure.py   (reads refs/ufd/ufd_framedata.csv and the GIFs next to it; writes refs/ufd/<move>_timing.csv
and refs/ufd/calibration_metrics.csv)"""
import os, csv, json
import numpy as np
from PIL import Image, ImageSequence

HERE = os.path.dirname(os.path.abspath(__file__))
UFD = os.path.join(HERE, "refs", "ufd")
MOVES = {"Mario Jab 1": ("MarioJab1.gif", 2, 3, "light"), "Little Mac Forward Smash": ("LittleMacFSmash.gif", 14, 15, "strong"),
         "Terry Forward Tilt": ("TerryFTilt.gif", 8, 13, "strong"), "Ryu Forward Smash": ("RyuFSmash.gif", 15, 17, "strong")}

def masks(rgb):
    a = rgb.astype(int)
    bg = np.array([64, 64, 64])
    body = np.abs(a - bg).sum(axis=2) > 36
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    sat = a.max(axis=2) - a.min(axis=2)
    hit = (sat > 90) & (((r > 120) & (g < 90)) | ((b > 120) & (g < 90)))   # red / magenta / purple bubbles
    return body & ~hit, hit

def measure(name, gif, a0, a1, strength):
    frames = [np.array(f.convert("RGB")) for f in ImageSequence.Iterator(Image.open(os.path.join(UFD, gif)))]
    n = len(frames)
    M = [masks(f) for f in frames]
    # attack side: hitbox centroid against the body centroid during the active frames
    hx = [np.nonzero(M[k][1])[1].mean() for k in range(a0 - 1, a1) if M[k][1].any()]
    bx = [np.nonzero(M[k][0])[1].mean() for k in range(a0 - 1, a1)]
    side = 1 if (np.mean(hx) if hx else 0) >= np.mean(bx) else -1
    h0 = np.nonzero(M[0][0])[0]; height_px = int(h0.max() - h0.min())
    ext = []
    for body, _ in M:
        xs = np.nonzero(body)[1]
        ext.append(float(xs.max() if side > 0 else -xs.min()))
    ext = np.array(ext)
    c = a0 - 1                                                     # first active frame, 0-based
    k0 = int(np.argmin(ext[: c + 1]))                              # anticipation key: furthest back before contact
    full = float(ext[a0 - 1: a1].max())
    pct = (ext - ext[0]) / max(full - ext[0], 1e-6) * 100
    rel = (ext - ext[k0]) / max(full - ext[k0], 1e-6)
    d = np.abs(np.diff(ext[k0: c + 1]))
    tot = d.sum() or 1e-6
    # side-view silhouettes cannot tell the limb pulling back from the body still leaning in, so two plain readings:
    # strict hold = frames after contact at >= 90% of full extension; plateau = longest frozen run (< 1% of reach per frame)
    # starting within 3 frames after contact (Little Mac's arm settles back to ~72% and freezes)
    hold = 0
    for k in range(c + 1, n):
        if rel[k] >= 0.9:
            hold += 1
        else:
            break
    seg = rel[c + 1: c + hold + 1] * 100
    dv = np.diff(seg)
    nz = [x for x in dv if abs(x) >= 0.5]
    rev = int(sum(1 for i in range(len(nz) - 1) if nz[i] * nz[i + 1] < 0))
    amp = float(seg.max() - seg.min()) if len(seg) else 0.0
    best = (0, None)
    for st in range(c + 1, min(n, c + 4)):
        run = 0
        for k in range(st + 1, n):
            if abs(rel[k] - rel[k - 1]) < 0.01:
                run += 1
            else:
                break
        if run > best[0]:
            best = (run + 1, round(float(rel[st] * 100)))
    rows = [{"frame": k + 1, "extension_pct": round(float(pct[k]), 1), "reach_px": round(float(ext[k] - ext[0]), 1),
             "active": a0 <= k + 1 <= a1} for k in range(n)]
    with open(os.path.join(UFD, gif.replace(".gif", "_timing.csv")), "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    return {"move": name, "frames": n, "attack_side": "right" if side > 0 else "left", "height_px": height_px,
            "anticipation_key_frame": k0 + 1, "contact_frame": a0, "build_frames": c - k0,
            "share_last1_pct": round(100 * d[-1] / tot, 1) if len(d) else 100.0,
            "share_last2_pct": round(100 * d[-2:].sum() / tot, 1) if len(d) else 100.0,
            "peak_on_contact_frame": bool(len(d)) and int(np.argmax(d)) == len(d) - 1,
            "hold_frames_after_first_contact": hold, "frozen_plateau_frames": best[0], "plateau_extension_pct": best[1],
            "recoil_reversals_in_hold": rev, "recoil_amplitude_pct_reach": round(amp, 1),
            "strength": strength,
            "extension_curve_pct": [round(float(x)) for x in pct]}

if __name__ == "__main__":
    out = [measure(k, *v) for k, v in MOVES.items()]
    keys = [k for k in out[0] if k != "extension_curve_pct"]
    with open(os.path.join(UFD, "calibration_metrics.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=keys + ["extension_curve_pct"])
        w.writeheader()
        for r in out:
            w.writerow({**{k: r[k] for k in keys}, "extension_curve_pct": " ".join(map(str, r["extension_curve_pct"]))})
    for r in out:
        print(json.dumps(r))
