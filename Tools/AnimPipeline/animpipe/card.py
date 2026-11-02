"""Move card: the one data file per move (brief §4.1). Versions are cards, not scripts.

Schema (cards/<kit>/<kit>_<move>_vN.json, see cards/README.md):
  move, class (jab|tilt|smash -> move_sheets _classes), anim (UE asset the build writes), rig, fps (60 or 62.5)
  timing_csv   frame data CSV (refs/ufd/*_timing.csv: frame,extension_pct,reach_px,active; 1-based game frames).
               The ONLY source of the windows: startup/active/hold/FAF are derived here, never typed.
  faf_game     0-based game frame of FAF when the CSV ends before it (owner rule: "fai 19 se parti da 0")
  poses        [{name, frame (anim), source: {file, frame} (owner's .casc/FBX step pose) | null, spacing?}]: the only
               keyed frames; `spacing` overrides the phase profile for the segment that starts at this pose
               ease_out (0-1, opt-in): a flat pose key followed by an unweighted segment eases out with this leave weight
  spacing      {phase: profile}: charge_long | arrive_1_2 | hold_recoil | limb_led_recovery
  part_delays  {chest, head, forearm, hand}: frames each part lags the pelvis
  exemptions   [{check, frames, why}]: declared, owner can override
  sheet_extra  extra move_sheets fields the checks read (hold_window, strike_limb, strike_side): overrides the derived ones
  edits        [{control, attr: Location|Rotation, frames, delta [x,y,z], why}]: targeted pose changes, added to the keys
  drop         [{control, attr, frames, why}]: keys removed on one control so it travels through (no stop) on those frames
  hitstop_blend [{frame, toward, amount 0-1, match: [control name fragments] | [] = all, why}]: the key at `frame` moves
               `amount` of the way to the key at `toward` on the matched controls (after `edits`). Fixes a pop out of a
               hitbox-active frame (check hitstop_snap): the travel moves before the hitbox opens or later in the recovery.
  hitstop_max_cm  per-bone limit of the hitstop_snap check (default 10 cm per anim frame); the check is on for every move
"""
import csv, json, os

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REFS = os.path.join(os.path.expanduser("~"), "Documents", "Unreal Projects", "GASP_Giustino2", "Saved", "AnimPipeline")
GAME_HZ = 62.5
PROFILES = {"charge_long", "arrive_1_2", "hold_recoil", "limb_led_recovery"}
REQUIRED = ("move", "class", "anim", "fps", "timing_csv", "poses")


def load(path):
    c = json.load(open(path if os.path.isabs(path) else os.path.join(TOOLS, path)))
    miss = [k for k in REQUIRED if k not in c]
    if miss:
        raise ValueError(f"card {path}: missing {miss}")
    bad = {p: v for p, v in c.get("spacing", {}).items() if v not in PROFILES}
    if bad:
        raise ValueError(f"card {path}: unknown spacing profiles {bad}")
    frames = [p["frame"] for p in c["poses"]]
    if frames != sorted(set(frames)):
        raise ValueError(f"card {path}: pose frames must be unique and ascending: {frames}")
    return c


def key_frames(c):
    return [p["frame"] for p in c["poses"]]


def g2k(c, g):
    """0-based game frame -> anim frame (the clip at c.fps samples the 62.5 Hz game; at 62.5 the map is identity)."""
    return int(round(g * float(c["fps"]) / GAME_HZ))


def windows(c):
    """{startup, active, faf} in 0-based game frames and anim frames, from the CSV."""
    rows = list(csv.DictReader(open(os.path.join(REFS, c["timing_csv"]))))
    act = [int(r["frame"]) - 1 for r in rows if r["active"] == "True"]
    faf = int(c.get("faf_game", len(rows)))
    g = {"startup": [0, act[0] - 1], "active": [act[0], act[-1]], "faf": faf}
    return {"game": g, "anim": {"startup": [g2k(c, x) for x in g["startup"]], "active": [g2k(c, x) for x in g["active"]],
                                "faf": g2k(c, faf)}}


def to_move_sheet(c):
    """The move_sheets.json entry the checks read (generated, never hand-written)."""
    w = windows(c)["anim"]
    e = {"move": c["class"], "anim": c["anim"], "contact": w["active"], "faf": w["faf"], "hold_window": w["active"],
         "effector": c.get("effector", "middle_01_l"), "reference": c.get("reference", "")}
    e.update(c.get("sheet_extra", {}))
    for ex in c.get("exemptions", []):
        e[f"{ex['check']}_exempt_frames"] = ex["frames"]
    return e


def phase_of(c, f):
    """startup | active | recovery of an anim frame, from the CSV windows."""
    a = windows(c)["anim"]["active"]
    return "startup" if f < a[0] else ("active" if f <= a[1] else "recovery")


def segments(c):
    """[[f0, f1, profile]] for each pair of consecutive poses: the spacing profile of the phase the segment starts in."""
    fr = key_frames(c)
    sp = c.get("spacing", {})
    own = {p["frame"]: p["spacing"] for p in c["poses"] if p.get("spacing")}   # a pose may set the profile of the segment it starts
    return [[a, b, own.get(a) or sp.get(phase_of(c, a), "limb_led_recovery")] for a, b in zip(fr, fr[1:])]
