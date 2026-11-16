"""Cascadeur in-betweens that follow the Unreal build's spacing (owner go 2026-11-03).

    python -m animpipe casc-spacing <card.json> --fit [--build=<bones.json>] [--passes=2]
    python -m animpipe casc-spacing <card.json> --apply [--settings=<fit result.json>]
    python -m animpipe casc-spacing <card.json> --score [--ref=<readback.json>]

The build (ue_rig.build) shapes each card segment with weighted flat tangents per spacing profile; Cascadeur shows its
own bezier between the same sparse keys. This module makes Cascadeur's in-betweens match the build with the key API only
(per segment and per layer: interval interpolation and the easing weights of the segment's two keys). No key is added,
moved or re-posed, so casc-import, the owner locks, casc-revert and casc-diff are untouched by construction; after
--apply the scene's readback must be redone (casc-import --ref) because the in-betweens changed.

Ground truth: the build's bones on every frame (`sheets/raw/<take>_legbones.json`, written by legibility after each build:
component space, [x, y, z, pitch, yaw, roll]). Cascadeur and the build do not hold the same KEY poses (the build's
Control Rig moves the knees up to ~7 cm and lock_lengths the neck; Cascadeur's rig its own few mm), so the metric removes
the key-pose gap: target(f) = build(f) + lerp over the segment of (cascadeur - build) at its two keys. What is left is
spacing and path shape between the keys. Worst bone gap (cm) per in-between frame; goal < 1 cm, reported per segment.

Cascadeur space: Y-up cm, the same numbers as the FBX readback with y and z swapped (measured 0.001 cm on the dair).
part_delays are not handled: they shift keys off the card frames (sparse_apply.py's lagged layers) and the import treats
only card key frames as poses; a card with a non-zero delay is refused until that is built.
"""
import json, os
import numpy as np

from animpipe import card as C
from animpipe.cascadeur import CASC_DIR, RESID_BONES, bones_from_json, load_ref, _card_path

TOOLS = C.TOOLS
SCRIPT = os.path.join(TOOLS, "casc_spacing.py").replace("\\", "/")
GRID = (0.1, 1 / 3, 0.6)
SKIP_LAYERS = ("Fingers_L", "Fingers_R")     # no compared bone on them; their keys keep Cascadeur's default


def to_casc(p):
    return [float(p[0]), float(p[2]), float(p[1])]


def from_casc(p):
    return np.array([p[0], p[2], p[1]], float)


def candidates():
    c = [{"interp": "LINEAR"}]
    for i in ("CLAMPED_BEZIER", "BEZIER", "LOW_AMPLITUDE_BEZIER"):
        c += [{"interp": i, "rw": round(rw, 4), "lw": round(lw, 4)} for rw in GRID for lw in GRID]
    return c


def load_build(path):
    d = json.load(open(path))
    return {int(f): bones_from_json(v)[0] for f, v in d["frames"].items()}


def target(build, casc_keys, keys):
    """{frame: {bone: xyz}} in UE space for every in-between frame: the build plus the key-pose gap blended across the segment."""
    out = {}
    for a, b in zip(keys, keys[1:]):
        for f in range(a + 1, b):
            t = (f - a) / (b - a)
            out[f] = {bn: build[f][bn] + (1 - t) * (casc_keys[a][bn] - build[a][bn]) + t * (casc_keys[b][bn] - build[b][bn])
                      for bn in RESID_BONES}
    return out


def score(casc, tgt):
    """Per in-between frame: (worst cm, bone)."""
    res = {}
    for f, tb in tgt.items():
        d = {bn: float(np.linalg.norm(casc[f][bn] - p)) for bn, p in tb.items()}
        w = max(d, key=d.get)
        res[f] = (round(d[w], 2), w)
    return res


def segment_report(sc, keys):
    return {f"{a}-{b}": max((sc[f][0] for f in range(a + 1, b)), default=0.0) for a, b in zip(keys, keys[1:]) if b - a > 1}


def _paths(c, opt):
    move = c["move"]
    take = c["anim"].rsplit("/", 1)[1][3:]
    build = opt.get("build") or os.path.join(TOOLS, "sheets", "raw", f"{take}_legbones.json")
    return move, build, os.path.join(CASC_DIR, f"{move}_ref.json")


def plan_cmd(card_path, opt):
    c = C.load(_card_path(card_path))
    if any(int(d) for d in (c.get("part_delays") or {}).values()):
        raise SystemExit("casc-spacing: part_delays != 0 need lagged keys off the card frames: not built yet")
    move, build_p, ref_p = _paths(c, opt)
    keys = sorted(int(k) for k in c["author"]["poses"])
    ref = load_ref(opt.get("ref") or ref_p)
    if ref is None:
        raise SystemExit(f"no Cascadeur readback {ref_p}: run casc-import --ref first")
    build = load_build(build_p)
    casc_keys = {k: ref[k][0] for k in keys}
    tgt = target(build, casc_keys, keys)
    if "score" in opt:
        sc = score({f: ref[f][0] for f in ref}, tgt)
        rep = segment_report(sc, keys)
        out = os.path.join(CASC_DIR, f"{move}_spacing_score.json")
        json.dump({"segments_cm": rep, "frames": {str(f): v for f, v in sc.items()}}, open(out, "w"), indent=1)
        print(f"casc-spacing score {move} (readback vs build, key gap removed): " + ", ".join(f"{s} {v}" for s, v in rep.items()))
        print(f"  worst {max(rep.values())} cm; {out}")
        return rep
    if "fit" in opt:
        plan = {"tab": f"{move}_spacelab.casc", "mode": "fit", "bones": list(RESID_BONES),
                "target": {str(f): {bn: to_casc(p) for bn, p in tb.items()} for f, tb in tgt.items()},
                "candidates": candidates(), "fit": [a for a, b in zip(keys, keys[1:]) if b - a > 1],
                "passes": int(opt.get("passes", 2)), "skip_layers": list(SKIP_LAYERS),
                "out": os.path.join(CASC_DIR, f"{move}_spacing_fit_result.json").replace("\\", "/")}
        out = os.path.join(CASC_DIR, f"{move}_spacing_fit.json")
        json.dump(plan, open(out, "w"))
        print(f"casc-spacing fit plan {move}: {len(plan['fit'])} segments, {len(plan['candidates'])} candidates per layer")
        print(f"next: copy {move}.casc to {plan['tab']}, open it in Cascadeur, run casc_spacing.py with SPACING={out}")
        return plan
    settings = json.load(open(opt.get("settings") or os.path.join(CASC_DIR, f"{move}_spacing_fit_result.json")))["settings"]
    plan = {"tab": f"{move}.casc", "mode": "apply", "bones": list(RESID_BONES), "settings": settings,
            "fbx": os.path.join(CASC_DIR, f"{move}_casc.fbx").replace("\\", "/"),
            "out": os.path.join(CASC_DIR, f"{move}_spacing_applied.json").replace("\\", "/")}
    out = os.path.join(CASC_DIR, f"{move}_spacing_apply.json")
    json.dump(plan, open(out, "w"), indent=1)
    print(f"casc-spacing apply plan {move}: {len(settings)} segments -> {plan['tab']}")
    print(f"next: run casc_spacing.py in Cascadeur with SPACING={out}, then casc-import --ref and casc-spacing --score")
    return plan
