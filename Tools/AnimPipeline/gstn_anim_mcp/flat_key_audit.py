"""Headless, no editor: which keys of a move card the OLD "tangents" build (animpipe/ue_rig.build, curve != "spline")
writes with FLAT tangents, i.e. every rig channel at zero speed on that key (the K7/K8 stop-and-go cause, diagnosis
dropkick-k8-3). Audit stop-and-go 2026-11-24.

This is a RISK measure from the card, not the stop measure: part_delays shift a part's interior keys, so the whole body
may not be still on a flat key. The verdict on a clip is only no_stop_gate.py on its joints dump.
Flat key = key bordering a segment whose spacing profile has a weight on that side (ue_rig.PROFILE_W, copied below:
ue_rig imports unreal). Declared holds (hold/impact/holds) are flat on purpose and listed apart.
CLI: python flat_key_audit.py <card.json> [...]   -> one TSV line per card to stdout."""
import json, os, sys
TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)
from animpipe import card as _card  # noqa: E402
from animpipe.card import key_frames, hold_keys, segments  # noqa: E402
import glob, shutil, tempfile  # noqa: E402

# timing CSVs live in Saved/ (not versioned); the cards/<kit>/timing/ copies in git stand in when Saved has none
if not os.path.isdir(os.path.join(_card.REFS, "refs", "ufd")):
    _tmp = tempfile.mkdtemp(prefix="flatkeys_refs_")
    os.makedirs(os.path.join(_tmp, "refs", "ufd"))
    for f in sorted(glob.glob(os.path.join(TOOLS, "cards", "*", "**", "timing", "*_timing.csv"), recursive=True)):
        shutil.copy(f, os.path.join(_tmp, "refs", "ufd", os.path.basename(f)))
    _card.REFS = _tmp

# keep in sync with animpipe/ue_rig.PROFILE_W
PROFILE_W = {"charge_long": (0.6, 0.12), "arrive_1_2": (0.12, 0.6), "hold_recoil": (1 / 3, 1 / 3), "limb_led_recovery": None,
             "ease_from": (0.6, None), "ease_into": (None, 0.5)}


def _prof(segs, f):
    for a, b, p in segs:
        if a <= f < b:
            return p
    return None


def audit(path):
    c = json.load(open(path))
    if c.get("curve", "tangents") == "spline":
        return {"card": path, "curve": "spline", "keys": len(c["poses"]), "flat": [], "holds": hold_keys(c), "undeclared": [], "risk": []}
    fr = key_frames(c)
    src = "csv"
    try:
        segs = segments(c)
    except FileNotFoundError:
        # timing CSV only in Saved/: the active window from the card (anim_active) or from move_sheets.json, which
        # animpipe check generates from the same card (card.to_move_sheet), so the phases are the build's
        ms = json.load(open(os.path.join(TOOLS, "move_sheets.json")))
        # the ftilt v3.1 card's move has no sheet entry of its own; its builds ran as ganon_ftilt_v31 (same anim, same CSV)
        alias = {"overlord_ftilt_v31": "ganon_ftilt_v31"}.get(c["move"], c["move"])
        act = c.get("anim_active") or ms.get(alias, {}).get("contact")
        if not act:
            raise
        src = "anim_active" if c.get("anim_active") else "move_sheets.json"
        sp = c.get("spacing", {})
        own = {p["frame"]: p["spacing"] for p in c["poses"] if p.get("spacing")}
        ph = lambda f: "startup" if f < act[0] else ("active" if f <= act[1] else "recovery")
        segs = [[a, b, own.get(a) or sp.get(ph(a), "limb_led_recovery")] for a, b in zip(fr, fr[1:])]
    flat, eased = [], []
    for i, f in enumerate(fr):
        out_w = PROFILE_W.get(_prof(segs, f)) if i + 1 < len(fr) else None
        in_w = PROFILE_W.get(_prof(segs, fr[i - 1])) if i else None
        out_w = out_w if out_w and out_w[0] is not None else None
        in_w = in_w if in_w and in_w[1] is not None else None
        if out_w is not None or in_w is not None:
            flat.append(f)
            # sampled once per frame, a flat tangent only slows the in-betweens: a key with a key on the next and previous
            # frame on its flat sides is sampled at the key values, no ease, no stop from the tangent
            if (out_w is not None and fr[i + 1] - f >= 2) or (in_w is not None and f - fr[i - 1] >= 2):
                eased.append(f)
    holds = hold_keys(c)
    und = [f for f in flat if f not in holds]
    # interior keys only: frame 0 and the last key are clip ends, a stop there is a start/end pose
    inner = [f for f in und if f not in (fr[0], fr[-1])]
    risk = [f for f in inner if f in eased]
    gaps = [b - a for a, b in zip(fr, fr[1:])]
    return {"card": path, "curve": "tangents", "keys": len(fr), "flat": flat, "holds": holds, "undeclared": und,
            "inner_undeclared": inner, "risk": risk, "min_gap": min(gaps) if gaps else 0, "part_delays": bool(c.get("part_delays")), "phase_src": src}


if __name__ == "__main__":
    print("card\tcurve\tkeys\tflat_keys\tdeclared_holds\tundeclared_inner_flat\tRISK_eased_into_gap>=2\tmin_key_gap\tpart_delays\tphase_src")
    for p in sys.argv[1:]:
        try:
            r = audit(p)
            print(f"{p}\t{r['curve']}\t{r['keys']}\t{len(r['flat'])}\t{r['holds']}\t{r.get('inner_undeclared', [])}\t{r.get('risk', [])}\t{r.get('min_gap', '')}\t{r.get('part_delays', '')}\t{r.get('phase_src', '')}")
        except Exception as e:  # a card without timing csv or poses: say so, never guess
            print(f"{p}\tERROR {type(e).__name__}: {str(e)[:120]}")
