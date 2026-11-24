# Animation pilot pipeline (Cascadeur -> Unreal)

Tooling for the combat-move animation pilot lives in [`Tools/AnimPipeline/`](../../Tools/AnimPipeline/README.md): motion
models, pre-flight and quality checks (motion, legibility, tangency, balance, clearance), the Cascadeur and Unreal
round-trip scripts and the light-model fix loop. The README there explains the purpose, how to run a pass and how to
extend it. It is a content/animation workflow, not game code; nothing in the C++ modules depends on it.

Data (scenes, renders, references, logs) stays out of git, in the working folder `Saved/AnimPipeline/`
(`ANIMPIPELINE_DIR`); `Tools/AnimPipeline/sync.sh` moves the tracked files between the two.

## Curve "spline": keys on every frame (2026-11-24)

The tangent build (card `curve` absent or `"tangents"`) gives every key that borders a weighted spacing segment a flat
tangent, so a card with many keys plays stop-and-go (diagnosis `docs/notes/handovers/dropkick-k8-3-diagnosi.md`, branch
`claude/project-thread-6blrlz`). With `"curve": "spline"` the build keeps the same key edits (drop, edits, hitstop_blend,
planted pins, part delays) and then writes a key on every frame from one cubic per channel through the keys
(`animpipe/curves.py`: three-point slope, monotone so no bone passes a pose; Euler channels the short way). Only keys
marked `hold: true` / `impact: true` (or listed in `holds`) are flat. `animpipe variant new` sets `"spline"` on new
variants; approved cards keep their old build (rule 13b).

`animpipe check` adds `stop_and_go` and the `life_*` lines from `animpipe/life.py`, the one source of the stop and life
metrics (the OG skill's life metrics back as numbers; `no_stop_gate.py` and the capture gate call it too). **Owner 2A
2026-11-24:** a metric BLOCKS only once `python life_calibrate.py --write-gate` proved it on a complete run (every stock and
approved clip passes it, at least one known-bad clip fails it) and listed it in `Tools/AnimPipeline/life_gate.json`; until
then it is a measure printed as `WOULD FAIL ...` for the reviewer. `hold_reason` is a declaration rule and always blocks.
- `stop_and_go` = life `stops` + `still_runs`. Startup: no GO-STOP-GO dip (a step < 3.5 cm with the whole body, under half
  the travel of the 3 steps before AND after, the motion continuing the same way: the flat-tangent key), at most 2 reversal
  beats (anticipation turns), no isolated pop (> 4x the median and the smaller neighbour; the 2 steps into contact are the
  wanted impact snap). Whole clip: no run of more than 3 DEAD steps (< 0.4 cm/frame: take-off, landing, settle, loop).
  The first version (every step >= 3.5 cm, max <= 4x median, still = 3.5 cm on the whole clip) failed a textbook OG strike
  and every stock GASP landing (tests/test_life.py): that is why it changed.
- Holds (owner 1A 2026-11-24: a LIVE hold is a normal tool, the defects are dead holds and stop-and-go): declared in the card
  (`life_holds` [{frames, why}], a `hold`/`impact` pose with `why` or `hold_reason`, card `holds` + `hold_reasons`).
  `hold_reason` = every hold has its reason; `life_hold_alive` = no dead core (pelvis < 1 cm over the hold), no frozen
  smash recoil (< 1 cm), no "hold" moving more than 6% of the reach per frame; `life_hold_guides` (measure only) = more
  than 2 holds or longer than 6 frames (12 on a smash), numbers for the eye.
- `life_stop_sync`: share of speed minima reached together by most parts <= 0.5 (stock mocap 0.05-0.18, flat-tangent jab 0.84).
- `life_cascade` (pelvis > forearm > hand peaks, spread >= 1 frame, lags >= 0.5), `life_impact` (>= 40% of the effector
  path in the last 2 frames, peak on contact), `life_anticipation` (effector or pelvis first goes >= 2 cm against the attack).
`python -m animpipe life <joints.json|frames.jsonl|frames.json> [--card=] [--window=a-b] [--contact=] [--holds=a-b:why] [--json]`
runs them on any dump or capture (the level-2 review pack reads `--json`). Calibration: `python life_calibrate.py` on
`life_calibration.json` (stock mocap must PASS, approved moves PASS, K7/K8/K6 settle FAIL); dumps of the approved clips
come from `gstn_anim_mcp/clip_joints_dump_ue.py`. Tests: `python tests/test_life.py`, `python tests/test_curves.py`.
