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

`animpipe check` adds `stop_and_go`: blocking on spline cards, a measure on the others. Startup window: no step with the
whole body (pelvis, plus hands, forearms, head and feet relative to the pelvis) under 3.5 cm, largest step <= 4x the
median. Whole clip: no run of more than 3 still steps (take-off, landing). Declared holds and `exemptions` of check
`stop_and_go` are skipped; thresholds in the card's `stop_and_go` block. Warnings (measure): `near_equal_keys`
(consecutive keys under 3.5 cm apart) and `same_frame_arrivals` (arms, head and legs all arriving on one frame).
Thresholds come from the K7/K8 diagnosis, not yet calibrated on approved clips. Tests: `python tests/test_curves.py`.
