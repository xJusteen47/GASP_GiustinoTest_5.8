"""Anti-stop gate on a joints dump, for EVERY clip of a move: startup, loop, settle, landing (skill audit 2026-11-23, option A).

Why: K7/K8 read stop-and-go and nobody saw it, because no gate measured stops (diagnosis dropkick-k8-3) and the inherited
settle of K6 (still ~6 frames on the ground, v158-163) was never judged again. `animpipe check` measures the card it builds
(stop_and_go in animpipe/checks.py); this tool measures any clip from its joints dump, cards or not, so the capture gate can
check the clips a variant only INHERITS too. Since the unified skill (2026-11-24) the verdict is animpipe/life.py (one
definition, no copy; each metric blocks only with its calibration proof, owner 2A). The STOP marks below stay for the sheets:
  still step: every tracked point (pelvis in component space; hands, forearms, head, feet relative to the pelvis) moves
              < stop_cm in one anim frame;
  whole clip: no run of more than still_run consecutive still steps outside the declared holds (settle, loop, landing);
  startup:    (optional window) no still step at all and the largest step <= max_ratio x the median (curves.stop_and_go).
Declared holds (owner-wanted frozen poses) are steps f -> f+1 listed in `holds`; everything is a parameter (rule 15c).

Joints JSON: {"fps", "joints": {bone: [[x, y, z] per frame]}} (clip_joints_dump_ue.py / runstart_clip_joints_ue.py).
CLI: python no_stop_gate.py <joints.json> [--startup=a-b] [--holds=a-b,c-d] [--stop=3.5] [--pop=4] [--run=3]
Prints `NO_STOP: OK|FAIL` and at most 4 more lines; exit 0 OK, 1 FAIL.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

_TOOLS = str(Path(__file__).resolve().parents[1])
if _TOOLS not in sys.path:
    sys.path.insert(0, _TOOLS)

from animpipe import curves as CV  # noqa: E402
from animpipe import life  # noqa: E402

# joints-dump bone -> curves point name (curves.GATE_POINTS + pelvis)
DUMP_POINTS = {"pelvis": "pelvis", "hand_MainPoint_l": "hand_l", "hand_MainPoint_r": "hand_r",
               "lowerarm_MainPoint_l": "lowerarm_l", "lowerarm_MainPoint_r": "lowerarm_r", "head": "head",
               "foot_MainPoint_l": "foot_l", "foot_MainPoint_r": "foot_r"}
DEFAULTS = {"stop_cm": CV.STOP_CM, "max_ratio": CV.JUMP_RATIO, "still_run": CV.STILL_RUN}


def frames_from_joints(joints: dict[str, list]) -> list[dict[str, tuple]]:
    """Per-frame {point: (x, y, z)} in the names animpipe/curves uses. Raises ValueError without a pelvis track."""
    if "pelvis" not in joints:
        raise ValueError("joints dump has no pelvis track: the stop gate measures limbs relative to the pelvis")
    pts = {DUMP_POINTS[b]: v for b, v in joints.items() if b in DUMP_POINTS}
    n = min(len(v) for v in pts.values())
    return [{p: tuple(float(x) for x in v[f]) for p, v in pts.items()} for f in range(n)]


def parse_ranges(spec: Any) -> set[int]:
    """'a-b,c-d' or [[a, b], ...] or [f, ...] -> set of steps (a-b = steps a..b-1)."""
    out: set[int] = set()
    if not spec:
        return out
    items = spec.split(",") if isinstance(spec, str) else spec
    for it in items:
        if isinstance(it, str):
            it = it.strip()
            if not it:
                continue
            a, _, b = it.partition("-")
            a, b = int(a), int(b or int(a) + 1)
        elif isinstance(it, (list, tuple)):
            a, b = int(it[0]), int(it[1])
        else:
            a, b = int(it), int(it) + 1
        out |= set(range(a, b))
    return out


def still_steps(F: list[dict[str, tuple]], holds: set[int] = frozenset(), stop_cm: float = CV.STOP_CM) -> list[int]:
    """Every whole-body still step outside the holds (what the motion sheets mark as a STOP)."""
    return [f for f, (d, _) in enumerate(CV.body_steps(F)) if f not in holds and d < stop_cm]


def _holds(spec: Any) -> list[tuple[int, int, str]]:
    """'a-b:why,c-d:why' / [{frames, why}] -> life holds. A bare range ('a-b', [a, b]) has no reason: hold_reason fails
    (owner 1A 2026-11-24: a hold is declared WITH its reason, then judged on video)."""
    if not spec:
        return []
    if (isinstance(spec, str) and ":" in spec) or (isinstance(spec, list) and spec and isinstance(spec[0], dict)):
        return life.parse_holds(spec)
    steps = sorted(parse_ranges(spec))
    out: list[tuple[int, int, str]] = []
    for f in steps:                                   # contiguous steps -> one hold
        if out and out[-1][1] == f:
            out[-1] = (out[-1][0], f + 1, "")
        else:
            out.append((f, f + 1, ""))
    return out


def check(joints: dict[str, list], holds: Any = None, startup: Any = None, stop_cm: float = CV.STOP_CM,
          max_ratio: float = CV.JUMP_RATIO, still_run: int = CV.STILL_RUN, blocking: Any = None) -> dict[str, Any]:
    """Gate one clip with animpipe/life.py (the one source of the stop metrics, unified skill 2026-11-24): whole clip
    still_runs, startup stops when `startup` is given, declared holds (reason, live core). Each metric blocks only with its
    calibration proof (owner 2A, life_gate.json; `blocking` overrides it in tests); the others come back in `measures`.
    stop_cm / max_ratio / still_run are kept for the STOP marks of the velocity sheets (`stills`), not for the verdict.
    Returns {ok, lines (<= 4), still_runs [(a, b)], stills [steps], steps [cm per step], measures {name: line}}."""
    try:
        F = frames_from_joints(joints)
    except ValueError as exc:
        return {"ok": False, "lines": [str(exc)], "still_runs": [], "stills": [], "steps": [], "measures": {}}
    H = _holds(holds)
    win = [int(x) for x in (startup.split("-") if isinstance(startup, str) else startup)] if startup else None
    r = life.run(F, window=win, holds=H, gate=False)
    r = life.apply_gate(r, blocking)
    held = set()
    for a, b, _ in H:
        held |= set(range(a, b))
    runs = CV.long_stills(F, held, life.thresholds()["life_dead_cm"], int(life.thresholds()["life_still_run"]))
    bad = [k for k, v in r["block"].items() if not v[0]]
    lines = [f"{k}: {r['block'][k][1]}" for k in bad][:3]
    lines.append("measures (no calibration proof yet): " + ", ".join(f"{k} {str(v)[:60]}" for k, v in r["measure"].items()
                                                                 if str(v).startswith("WOULD FAIL")) or "none failing")
    steps = [round(d, 2) for d, _ in CV.body_steps(F)]
    return {"ok": not bad, "lines": lines[:4], "still_runs": runs, "stills": still_steps(F, held, stop_cm), "steps": steps,
            "measures": r["measure"], "blocks": {k: v for k, v in r["block"].items()}}


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__.strip().splitlines()[-3])
        return 2
    opt = dict(a.split("=", 1) for a in argv[1:] if a.startswith("--"))
    J = json.loads(Path(argv[0]).read_text(encoding="utf-8"))["joints"]
    r = check(J, opt.get("--holds", ""), opt.get("--startup", ""), float(opt.get("--stop", CV.STOP_CM)),
              float(opt.get("--pop", CV.JUMP_RATIO)), int(opt.get("--run", CV.STILL_RUN)))
    print(f"NO_STOP: {'OK' if r['ok'] else 'FAIL'}")
    for line in r["lines"][:3]:
        print(line[:400])
    return 0 if r["ok"] else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
