"""source_fidelity: the card's owner poses must be the owner's poses (plan 03/11 CP1, PR #24 §10). Blocking.

A pose in the card points at the owner's pose it comes from:
  poses[i].source = {file: "cards/<kit>/refs/<name>.json", frame: <owner frame>, tool, keep?: {field prefix: why}}
    tool "casc_to_author..." (default): file = Cascadeur control points ({frame: {joint: xyz}}); the expected author pose
         is casc_to_author.convert of that frame (rescale on, owner lengths averaged over the file's frames).
    tool "casc-import": file = bones read from Cascadeur ({frames: {frame: bones}}, cascadeur.load_bones_json); the
         card pose solved by the Poser must match those bones part by part (cascadeur.part_delta).
    derived: "<why>"  the pose is built from an owner pose on purpose (a held charge): listed, not compared.
  poses[i].origin = "owner" (CP4 `casc-import --owner-keys`): source is then mandatory.
Never compared (the pipeline's on every move, README): fingers, wrist_out, hand_rot, pelvis.loc (height). Any other
deviation must be declared in source.keep with a why; an undeclared one over 0.5 cm / 1 deg fails.

Blocking: a FAIL in `check` (block source_fidelity), `approve` refuses, and card.load() stops when the card has an
`origin: owner` pose (new-format cards). Without the rig data (sheets/raw, desktop only) the comparison is n/a, said so.
CLI: animpipe fidelity <card>  -> <= 5 lines, full result in <OUT>/<move>_fidelity.json
"""
import json, os

from animpipe import card as C
from animpipe.locks import LOCK_CM, LOCK_DEG, _flat, _unit

SKIP = ("fingers", "wrist_out", "pelvis.loc")
CONVERT_PARTS = ("pelvis", "spine", "head", "arm_l", "arm_r", "leg_l", "leg_r")


def _leaves(pose):
    out = {}
    for k in CONVERT_PARTS:
        if k in pose:
            for f, v in _flat({x: y for x, y in pose[k].items() if x != "hand_rot"} if isinstance(pose[k], dict) else pose[k], k):
                out[f] = v
    return {f: v for f, v in out.items() if not f.startswith(SKIP)}


def _rig_ok():
    import casc_to_author as A
    return os.path.exists(A.IDLE)


def _expected_points(data, frame):
    import casc_to_author as A
    frames = [k for k in data if k.lstrip("-").isdigit()]
    rig = A.ue_rig(); L_o = A.owner_lengths(data, frames)
    ref_z = A.ue(data[sorted(frames, key=int)[0]]["pelvis"])[2]
    pose, _, _ = A.convert(data[str(frame)], ref_z, True, rig, L_o)
    return pose


def compare_pose(card_pose, want, keep=()):
    """[(field, delta, unit, tol)] over the expected fields that are off (keep = declared field prefixes)."""
    a, b = _leaves(card_pose), _leaves(want)
    bad = []
    for f in sorted(b):
        if f.startswith(tuple(keep)):
            continue
        unit, tol = _unit(f)
        d = abs(a[f] - b[f]) if f in a else float("inf")
        if d > tol + 1e-6:
            bad.append((f, round(d, 2), unit, tol))
    return bad


def _bones_errors(c, frame, path, sframe, keep):
    from animpipe.cascadeur import load_bones_json, part_delta, PARTS, PART_CM, PART_DEG
    from animpipe.author import Poser
    want = load_bones_json(path).get(int(sframe))
    if want is None:
        return [(f"frame {sframe}", float("inf"), "missing in file", 0)]
    P, R = Poser().solve(c["author"]["poses"][str(frame)])
    out = []
    for part in PARTS:
        if part in keep:
            continue
        cm, dg = part_delta(P, R, *want, part)
        if cm > PART_CM or dg > PART_DEG:
            out.append((part, round(max(cm, dg), 2), "cm" if cm > PART_CM else "deg", PART_CM if cm > PART_CM else PART_DEG))
    return out


def check(c, path=None):
    """(ok | None (n/a), line, detail). detail = {frame: {...}} for the json."""
    errs, detail, compared, derived = [], {}, 0, []
    rig = None
    for p in c.get("poses", []):
        f, src = p["frame"], p.get("source")
        if not src:
            if p.get("origin") == "owner":
                errs.append(f"frame {f}: owner pose without source"); detail[f] = {"error": "no source"}
            continue
        if src.get("derived"):
            derived.append(f); detail[f] = {"derived": src["derived"]}; continue
        fp = src.get("file", "")
        if not (fp.startswith("cards/") and "/refs/" in fp):
            errs.append(f"frame {f}: source file {fp!r} not under cards/<kit>/refs/ (owner poses live in git)")
            detail[f] = {"error": "file not in refs"}; continue
        full = os.path.join(C.TOOLS, fp)
        if not os.path.exists(full):
            errs.append(f"frame {f}: source file {fp} missing"); detail[f] = {"error": "file missing"}; continue
        if str(f) not in c.get("author", {}).get("poses", {}):
            errs.append(f"frame {f}: no author pose"); continue
        keep = tuple(src.get("keep", {}))
        if rig is None:
            rig = _rig_ok()
        if not rig:
            continue
        if str(src.get("tool", "casc_to_author")).startswith("casc-import"):
            bad = _bones_errors(c, f, full, src["frame"], keep)
        else:
            data = json.load(open(full, encoding="utf-8"))
            if str(src["frame"]) not in data:
                errs.append(f"frame {f}: owner frame {src['frame']} not in {fp}"); continue
            bad = compare_pose(c["author"]["poses"][str(f)], _expected_points(data, src["frame"]), keep)
        compared += 1
        detail[f] = {"owner_frame": src["frame"], "off": [list(b) for b in bad]}
        if bad:
            w = max(bad, key=lambda b: b[1] / max(b[3], 1e-6))
            errs.append(f"frame {f} (owner {src['frame']}): {len(bad)} fields off, worst {w[0]} {w[1]} {w[2]}")
    n_src = sum(1 for p in c.get("poses", []) if p.get("source"))
    if errs:
        return False, "; ".join(errs[:3]) + (f" (+{len(errs) - 3})" if len(errs) > 3 else ""), detail
    if n_src and rig is False:
        return None, f"n/a: {n_src} sourced poses, rig data (sheets/raw) missing on this machine", detail
    return True, f"{compared} owner poses match (0.5 cm / 1 deg)" + (f", derived {derived}" if derived else ""), detail


def guard(c, path):
    """card.load(): new-format cards (an `origin: owner` pose) stop on a fidelity error."""
    if not any(p.get("origin") == "owner" for p in c.get("poses", [])):
        return
    ok, line, _ = check(c, path)
    if ok is False:
        raise ValueError(f"card {path}: source_fidelity FAIL: {line} (owner poses must stay his; declare a deviation in source.keep with a why)")


def cmd(path):
    from animpipe.locks import out_dir
    c = C.load(path)
    ok, line, detail = check(c, path)
    jp = os.path.join(out_dir(), f"{c['move']}_fidelity.json")
    os.makedirs(os.path.dirname(jp), exist_ok=True)
    json.dump({"move": c["move"], "ok": ok, "line": line, "frames": detail}, open(jp, "w"), indent=1, default=str)
    print(f"source_fidelity {c['move']}: {'PASS' if ok else ('n/a' if ok is None else 'FAIL')} | {line}")
    print(jp)
    return ok
