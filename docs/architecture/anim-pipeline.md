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

`animpipe check` adds `stop_and_go` and the `life_*` blocks, **blocking on every card** (2026-11-24: K7 is a tangents card
and has the defect). They come from `animpipe/life.py`, the OG skill's life metrics back as numbers (owner 2026-11-24:
quality equal to the OG; a metric that errs is recalibrated with a proof, never softened):
- `stop_and_go` = life `stops` + `still_runs`. Startup: no GO-STOP-GO dip (a step < 3.5 cm with the whole body, under half
  the travel of the 3 steps before AND after, the motion continuing the same way: the flat-tangent key), at most 2 reversal
  beats (anticipation turns), no isolated pop (> 4x the median and the smaller neighbour; the 2 steps into contact are the
  wanted impact snap). Whole clip: no run of more than 3 DEAD steps (< 0.4 cm/frame: take-off, landing, settle, loop).
  The first version (every step >= 3.5 cm, max <= 4x median, still = 3.5 cm on the whole clip) failed a textbook OG strike
  and every stock GASP landing (tests/test_life.py): that is why it changed.
- `life_holds`: a stop is a defect unless declared (card `life_holds` [{frames, why}] or a `hold`/`impact` pose with `why`).
  A declaration without a reason, more than 2 holds, a hold longer than 6 frames (12 on a smash), a frozen hold on a smash
  (< 1 cm of recoil) or a "hold" moving more than 6% of the reach per frame FAIL: declarations cannot cover a defect.
- `life_stop_sync`: share of speed minima reached together by most parts <= 0.5 (stock mocap 0.05-0.18, flat-tangent jab 0.84).
- `life_cascade` (pelvis > forearm > hand peaks, spread >= 1 frame, lags >= 0.5), `life_impact` (>= 40% of the effector
  path in the last 2 frames, peak on contact), `life_anticipation` (effector or pelvis first goes >= 2 cm against the attack).
`python -m animpipe life <joints.json|frames.jsonl|frames.json> [--card=] [--window=a-b] [--contact=] [--holds=a-b:why] [--json]`
runs them on any dump or capture (the level-2 review pack reads `--json`). Calibration: `python life_calibrate.py` on
`life_calibration.json` (stock mocap must PASS, approved moves PASS, K7/K8/K6 settle FAIL); dumps of the approved clips
come from `gstn_anim_mcp/clip_joints_dump_ue.py`. Tests: `python tests/test_life.py`, `python tests/test_curves.py`.
