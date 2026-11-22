"""Approved poses: lock per frame and per part, approve, diff (plan 02/11 §4). Pure Python, no editor.

A card's `approved` block lists poses the owner accepted:
  approved  [{frames: [13, 14], parts: ["leg_l", "leg_r"], ref: "overlord/overlord_dair_v4.json" (path under cards/),
              by, date, why}]
Guard 1 (card.load): the card's `author` poses at the locked frames, for the locked parts, must equal those of the `ref`
card within LOCK_CM / LOCK_DEG; when `ref` is the card itself the snapshot cards/<kit>/approved/<move>.json written by
`animpipe approve` is the reference. A difference is a ValueError naming frame, part and field, so build/check/review
(which all load the card) stop by themselves. `--unlock part@frame` skips one pair and prints a line; the entry must still
be removed from the card with a `why`. A lock does not expire with the version: v5 is born with the `approved` of v4.

Parts map onto the `author.poses` fields (author.py) and, for the build, onto rig control fragments (ue_rig.PART_CTRLS):
  arm_x -> arm_x{hand,pole}, leg_x -> leg_x, trunk -> pelvis + spine, head -> head, hand_x -> arm_x.hand_rot + fingers +
  wrist_out (fingers and wrist_out are shared by both hands).
Tolerances are the plan's 0.5 cm / 1 deg; fingers are a 0-1 blend, 0.01.
`capture` keeps the owner's hand-keyed "Manuale" layer in git (card block `manual`), and `diff` warns when a changed pose sits under it.
"""
import glob, json, math, os, re

from animpipe import card as C

PARTS = ("arm_l", "arm_r", "leg_l", "leg_r", "trunk", "head", "hand_l", "hand_r")
LOCK_CM, LOCK_DEG, LOCK_BLEND = 0.5, 1.0, 0.01
CM_KEYS = {"loc", "hand", "pole", "foot"}            # leaf keys in cm; everything else is degrees (fingers: a blend)
PART_BONES = {"arm_l": ("upperarm_l", "lowerarm_l", "hand_l"), "arm_r": ("upperarm_r", "lowerarm_r", "hand_r"),
              "leg_l": ("thigh_l", "calf_l", "foot_l", "ball_l"), "leg_r": ("thigh_r", "calf_r", "foot_r", "ball_r"),
              "trunk": ("pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05"), "head": ("neck_01", "neck_02", "head"),
              "hand_l": ("index_01_l", "middle_01_l", "ring_01_l", "pinky_01_l"), "hand_r": ("index_01_r", "middle_01_r", "ring_01_r", "pinky_01_r")}
CARDS = C._cfg.CARDS
# control name fragments -> part; copy of ue_rig.PART_CTRLS (that module imports unreal). The fk hand/forearm controls belong to arm_x.
CTRL_PARTS = (("arm_l", ("upperarm_l", "lowerarm_l", "hand_l")), ("arm_r", ("upperarm_r", "lowerarm_r", "hand_r")),
              ("leg_l", ("thigh_l", "calf_l", "foot_l")), ("leg_r", ("thigh_r", "calf_r", "foot_r")),
              ("trunk", ("pelvis", "spine", "chest")), ("head", ("head", "neck")))


def _flat(v, path):
    if isinstance(v, dict):
        for k, x in v.items():
            yield from _flat(x, f"{path}.{k}")
    elif isinstance(v, (list, tuple)):
        for i, x in enumerate(v):
            yield f"{path}[{i}]", x
    else:
        yield path, v


def part_leaves(pose, part):
    """{field path: number} of the pose fields a part owns (missing field = absent, not zero: author.py reads it as idle)."""
    pose = pose or {}
    if part in ("arm_l", "arm_r"):
        src = {k: v for k, v in pose.get(part, {}).items() if k != "hand_rot"}
        return dict(_flat(src, part))
    if part in ("leg_l", "leg_r", "head"):
        return dict(_flat(pose[part], part)) if part in pose else {}
    if part == "trunk":
        return {k: v for f in ("pelvis", "spine") if f in pose for k, v in _flat(pose[f], f)}
    if part in ("hand_l", "hand_r"):
        out = {k: v for f in ("fingers", "wrist_out") if f in pose for k, v in _flat(pose[f], f)}
        if "hand_rot" in pose.get("arm_" + part[-1], {}):
            out.update(_flat(pose["arm_" + part[-1]]["hand_rot"], f"{part}.hand_rot"))
        return out
    raise ValueError(f"unknown part {part!r} (known: {', '.join(PARTS)})")


def _unit(field):
    if field.startswith("fingers"):
        return "blend", LOCK_BLEND
    keys = set(re.findall(r"[A-Za-z_]+", field))
    return ("cm", LOCK_CM) if keys & CM_KEYS else ("deg", LOCK_DEG)


def compare(a, b):
    """[(field, delta, unit, tol)] over the fields of two leaf dicts; a field present on one side only has delta inf."""
    out = []
    for f in sorted(set(a) | set(b)):
        unit, tol = _unit(f)
        d = abs(a[f] - b[f]) if f in a and f in b else math.inf
        out.append((f, d, unit, tol))
    return out


def _pose(c, frame):
    return c.get("author", {}).get("poses", {}).get(str(frame))


def card_file(c_or_path):
    p = c_or_path if os.path.isabs(c_or_path) else os.path.join(C.TOOLS, c_or_path)
    return os.path.abspath(p)


def rel_to_cards(path):
    return os.path.relpath(card_file(path), CARDS).replace(os.sep, "/")


def snapshot_path(path, move):
    return os.path.join(os.path.dirname(card_file(path)), "approved", f"{move}.json")


def _check_entry(e, i, where):
    miss = [k for k in ("frames", "parts", "ref", "why") if k not in e]
    if miss:
        raise ValueError(f"card {where}: approved[{i}] missing {miss}")
    bad = [p for p in e["parts"] if p not in PARTS]
    if bad:
        raise ValueError(f"card {where}: approved[{i}] unknown parts {bad} (known: {', '.join(PARTS)})")


def guard(c, path, unlock=()):
    """Guard 1. Raises ValueError listing every locked (frame, part, field) that moved; prints one line per unlock."""
    errs = [m for _, _, m in violations(c, path, unlock)]
    if errs:
        raise ValueError(f"card {path}: approved poses changed ({len(errs)}); fix the card or --unlock part@frame:\n  " + "\n  ".join(errs[:8]))


def violations(c, path, unlock=()):
    """[(frame, part, message)] of locked poses that differ from the lock's ref (or snapshot when ref is the card itself)."""
    errs = []
    for i, e in enumerate(c.get("approved", [])):
        _check_entry(e, i, path)
        ref_path = os.path.join(CARDS, e["ref"])
        if not os.path.exists(ref_path):
            raise ValueError(f"card {path}: approved[{i}] ref {e['ref']} not found under cards/")
        self_ref = os.path.samefile(ref_path, card_file(path))
        snap = None
        if self_ref:
            sp = snapshot_path(path, c["move"])
            snap = json.load(open(sp)) if os.path.exists(sp) else None
            ref = None
        else:
            ref = json.load(open(ref_path))
        for f in e["frames"]:
            for p in e["parts"]:
                if (p, f) in unlock:
                    print(f"UNLOCK {p}@{f}: approved guard skipped (remove the entry from the card with a why)")
                    continue
                if ref is not None:
                    want = part_leaves(_pose(ref, f), p)
                elif snap is not None:
                    want = snap["frames"].get(str(f), {}).get(p)
                    if want is None:
                        continue
                else:
                    continue                      # self-reference without a snapshot: nothing to compare against
                for field, d, unit, tol in compare(part_leaves(_pose(c, f), p), want):
                    if d > tol:
                        errs.append((f, p, f"frame {f} part {p} field {field}: moved {'(one side missing)' if d == math.inf else f'{d:.2f} {unit}'} (tol {tol} {unit}, ref {e['ref']})"))
    return errs


def parse_unlock(s):
    out = set()
    for t in filter(None, (s or "").split(",")):
        part, _, fr = t.partition("@")
        out.add((part, int(fr)))
    return out


def out_dir():
    """Saved/.../animpipe when the project's Saved folder exists, else Tools/AnimPipeline/sheets."""
    d = os.path.join(C.REFS, "animpipe")
    return d if os.path.isdir(C.REFS) else os.path.join(C.TOOLS, "sheets")


def approve(path, frames, parts, why, by="owner", date=None):
    """Add the entry (ref = the card itself), write the snapshot, and the rig keys when a build dump has them."""
    import datetime
    c = json.load(open(card_file(path), encoding="utf-8"))
    from animpipe import fidelity
    fok, fline, _ = fidelity.check(c, path)
    if fok is False:
        raise ValueError(f"approve refused: source_fidelity FAIL: {fline}")
    from animpipe import visual
    vb = visual.blocked(c["move"])
    if vb and vb[0]:
        raise ValueError(f"approve refused: visual review {vb[1]} (animpipe visual-gate)")
    frames = [int(x) for x in frames]
    _check_entry({"frames": frames, "parts": parts, "ref": "-", "why": why}, 0, path)
    entry = {"frames": frames, "parts": list(parts), "ref": rel_to_cards(path), "by": by,
             "date": date or datetime.date.today().isoformat(), "why": why}
    c.setdefault("approved", []).append(entry)
    sp = snapshot_path(path, c["move"])
    snap = json.load(open(sp)) if os.path.exists(sp) else {"move": c["move"], "frames": {}}
    for f in frames:
        for p in parts:
            snap["frames"].setdefault(str(f), {})[p] = part_leaves(_pose(c, f), p)
    bj = os.path.join(out_dir(), f"{c['move']}_build_ue.json")
    if os.path.exists(bj) and "keys" in (b := json.load(open(bj))):
        snap["keys"] = b["keys"]              # optional: rig control keys of the last build (guard 2, desktop)
    os.makedirs(os.path.dirname(sp), exist_ok=True)
    json.dump(snap, open(sp, "w", encoding="utf-8"), indent=1); open(sp, "a").write("\n")
    json.dump(c, open(card_file(path), "w", encoding="utf-8"), indent=1, ensure_ascii=False); open(card_file(path), "a").write("\n")
    print(f"approve {c['move']}: frames {frames} parts {','.join(parts)} locked (ref = itself)")
    print(sp)


# --- capture: the Manuale layer into git ---
def ctrl_part(channel):
    """Part of a rig channel name (e.g. hand_r_fk_ctrl.Rotation.X) by control fragment, or None."""
    for part, frags in CTRL_PARTS:
        if any(f in channel for f in frags):
            return part


def capture(path, src=None, by="owner", date=None):
    """Copy the layer backup JSON to cards/<kit>/manual/<move>.json and write the card's `manual` block (parts -> frames)."""
    import datetime
    c = json.load(open(card_file(path), encoding="utf-8"))
    src = src or os.path.join(out_dir(), f"{c['move']}_manual_layer.json")
    if not os.path.exists(src):
        print(f"capture {c['move']}: no layer backup at {src} (build once with the layer keyed, or pass --from=<file>)")
        return
    layer = json.load(open(src, encoding="utf-8"))
    parts, nk = {}, 0
    for ch, ks in layer.items():
        nk += len(ks)
        p = ctrl_part(ch)
        parts.setdefault(p or "?", set()).update(int(k[0]) for k in ks)
    rel = f"manual/{c['move']}.json"
    dst = os.path.join(os.path.dirname(card_file(path)), rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    json.dump(layer, open(dst, "w", encoding="utf-8"), indent=1); open(dst, "a").write("\n")
    c["manual"] = {"file": rel, "by": by, "date": date or datetime.date.today().isoformat(), "channels": len(layer), "keys": nk,
                   "parts": {p: sorted(fr) for p, fr in sorted(parts.items())}}
    json.dump(c, open(card_file(path), "w", encoding="utf-8"), indent=1, ensure_ascii=False); open(card_file(path), "a").write("\n")
    print(f"capture {c['move']}: {len(layer)} channels, {nk} keys; parts " + "; ".join(f"{p} {_ranges(fr)}" for p, fr in c["manual"]["parts"].items()))
    print(dst)


def manual_path(c, path):
    """Absolute path of the card's captured layer, or None."""
    return os.path.join(os.path.dirname(card_file(path)), c["manual"]["file"]) if c.get("manual") else None


# --- diff ---
def _vkey(name):
    m = re.search(r"_v(\d+)(\w*)$", name)
    return (int(m.group(1)), m.group(2)) if m else None


def previous_card(path):
    """The card with the highest version below this one in the same folder, or None."""
    me = _vkey(os.path.splitext(os.path.basename(path))[0])
    stem = re.sub(r"_v\d+\w*$", "", os.path.splitext(os.path.basename(path))[0])
    if not me:
        return None
    cand = []
    for q in glob.glob(os.path.join(os.path.dirname(card_file(path)), f"{stem}_v*.json")):
        k = _vkey(os.path.splitext(os.path.basename(q))[0])
        if k and k < me:
            cand.append((k, q))
    return max(cand)[1] if cand else None


def pose_delta(pa, pb, part):
    """(cm, deg) largest change of a part between two poses; a field present on one side only counts as 360 deg / 999 cm."""
    cm = deg = 0.0
    for _, d, unit, _ in compare(part_leaves(pa, part), part_leaves(pb, part)):
        d = 999.0 if d == math.inf and unit == "cm" else (360.0 if d == math.inf else d)
        if unit == "cm":
            cm = max(cm, d)
        else:
            deg = max(deg, d)
    return round(cm, 2), round(deg, 2)


def _ranges(fr):
    fr = sorted(set(fr)); out = []
    for f in fr:
        if out and f == out[-1][1] + 1:
            out[-1][1] = f
        else:
            out.append([f, f])
    return ",".join(str(a) if a == b else f"{a}-{b}" for a, b in out)


def timing_line(c):
    return "timing: owner" if c.get("anim_active") else "timing: CSV (avviso: i tempi li dà l'owner)"


def _bones(take):
    p = os.path.join(C.TOOLS, "sheets", "raw", f"{take}_legbones.json")
    return json.load(open(p))["frames"] if os.path.exists(p) else None


def bone_table(ca, cb):
    """{frame: {part: cm}} largest bone-position change per part between two built takes, or None without both dumps."""
    ta, tb = (x["anim"].rsplit("/", 1)[1][3:] for x in (ca, cb))
    A, B = _bones(ta), _bones(tb)
    if not A or not B:
        return None
    out = {}
    for f in sorted(set(A) & set(B), key=int):
        row = {}
        for p, bones in PART_BONES.items():
            ds = [math.dist(A[f][b][:3], B[f][b][:3]) for b in bones if b in A[f] and b in B[f]]
            row[p] = round(max(ds), 2) if ds else 0.0
        out[f] = row
    return out


EPS_CM, EPS_DEG = 0.05, 0.1


def diff(path, vs=None):
    """Author-pose displacement per frame x part between this card and `vs` (default: previous version, else the first
    approved ref). Prints at most 5 lines; the full table goes to <out_dir>/<move>_diff.json."""
    c = json.load(open(card_file(path), encoding="utf-8"))
    if vs is None:
        vs = previous_card(path) or (os.path.join(CARDS, c["approved"][0]["ref"]) if c.get("approved") else None)
    if vs is None or os.path.samefile(card_file(vs), card_file(path)):
        print(f"diff {c['move']}: no previous version or foreign ref to compare with (pass --vs=<card>)")
        return
    o = json.load(open(card_file(vs), encoding="utf-8"))
    fa, fb = ({int(f) for f in x.get("author", {}).get("poses", {})} for x in (o, c))
    frames = sorted(fa | fb)
    table = {str(f): {p: dict(zip(("cm", "deg"), pose_delta(_pose(o, f), _pose(c, f), p))) for p in PARTS} for f in frames if f in fa & fb}
    new, gone = sorted(fb - fa), sorted(fa - fb)
    changed, intact = {}, {}
    for f in sorted(fa & fb):
        for p in PARTS:
            t = table[str(f)][p]
            if t["cm"] > EPS_CM or t["deg"] > EPS_DEG:
                changed.setdefault(p, []).append(f)
    broken = {(f, p) for f, p, _ in violations(c, path)}      # locks are judged against their own ref, not against --vs
    for e in c.get("approved", []):
        for p in e["parts"]:
            intact.setdefault(p, set()).update(f for f in e["frames"] if (f, p) not in broken)
    res = {"card": c["move"], "vs": o["move"], "new_keys": new, "removed_keys": gone, "timing": timing_line(c), "poses": table}
    bt = bone_table(o, c)
    if bt:
        res["bones"] = bt
    d = out_dir(); os.makedirs(d, exist_ok=True)
    jp = os.path.join(d, f"{c['move']}_diff.json")
    json.dump(res, open(jp, "w"), indent=1)
    print(f"diff {c['move']} vs {o['move']}; {timing_line(c)}")
    print("cambiato: " + ("; ".join(f"{p} {_ranges(fr)}" for p, fr in changed.items()) or "niente")
          + (f" | chiavi nuove: {_ranges(new)}" if new else "") + (f" | chiavi tolte: {_ranges(gone)}" if gone else ""))
    mline = ""
    if c.get("manual"):
        hit = {p: [f for f in fr if any(abs(f - g) <= 1 for g in changed.get(p, ()))] for p, fr in c["manual"]["parts"].items()}
        hit = {p: fr for p, fr in hit.items() if fr}
        mline = " | manuale sotto una posa cambiata: " + "; ".join(f"{p} {_ranges(fr)}" for p, fr in hit.items()) + " (ricontrollare)" if hit else " | manuale: intatto"
    print("intatto (lock): " + ("; ".join(f"{p} {_ranges(fr)}" for p, fr in intact.items() if fr) or "nessun lock") + mline)
    if bt:
        bc = {}
        for f, row in bt.items():
            for p, v in row.items():
                if v > EPS_CM:
                    bc.setdefault(p, []).append(int(f))
        print("ossa (build): " + ("; ".join(f"{p} {_ranges(fr)}" for p, fr in bc.items()) or "niente"))
    print(jp)
