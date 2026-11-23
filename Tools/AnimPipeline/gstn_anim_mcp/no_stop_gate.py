"""Anti-stop gate on a joints dump, for EVERY clip of a move: startup, loop, settle, landing (skill audit 2026-11-23, option A).

Why: K7/K8 read stop-and-go and nobody saw it, because no gate measured stops (diagnosis dropkick-k8-3) and the inherited
settle of K6 (still ~6 frames on the ground, v158-163) was never judged again. `animpipe check` measures the card it builds
(stop_and_go in animpipe/checks.py); this tool measures any clip from its joints dump, cards or not, so the capture gate can
check the clips a variant only INHERITS too. The thresholds and the math are animpipe/curves.py (one definition, no copy):
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


def check(joints: dict[str, list], holds: Any = None, startup: Any = None, stop_cm: float = CV.STOP_CM,
          max_ratio: float = CV.JUMP_RATIO, still_run: int = CV.STILL_RUN) -> dict[str, Any]:
    """Gate one clip. Returns {ok, lines (<= 4), still_runs [(a, b)], stills [steps], steps [cm per step]}."""
    try:
        F = frames_from_joints(joints)
    except ValueError as exc:
        return {"ok": False, "lines": [str(exc)], "still_runs": [], "stills": [], "steps": []}
    held = parse_ranges(holds)
    runs = CV.long_stills(F, held, stop_cm, int(still_run))
    ok = not runs
    lines = [f"whole clip ({len(F)} frames): still runs > {still_run} steps (< {stop_cm} cm/frame) outside holds: "
             + (", ".join(f"{a}-{b + 1}" for a, b in runs) if runs else "none")]
    if startup:
        a, b = (int(x) for x in (startup.split("-") if isinstance(startup, str) else startup))
        s_ok, s_line, _ = CV.stop_and_go(F, [a, b], held, stop_cm, max_ratio)
        ok = ok and s_ok
        lines.append("startup " + s_line)
    steps = [round(d, 2) for d, _ in CV.body_steps(F)]
    return {"ok": ok, "lines": lines, "still_runs": runs, "stills": still_steps(F, held, stop_cm), "steps": steps}


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
