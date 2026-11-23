# Brief: Cascadeur curves that match the Unreal build, then the physics panel switches (owner go 2026-11-03)

Read only this brief, `Tools/AnimPipeline/cards/README.md` (section "Editing in Cascadeur"), the last section of
`anim-pipeline.md` ("Giro scheda ↔ Cascadeur") and the files you touch.

## Goal (owner: "assolutamente si")
1. **Spacing**: in the dair scene the in-betweens Cascadeur shows must match what `animpipe build` makes in Unreal
   (`AS_OverlordDAirV4_Build`), not be Cascadeur's own clamped-bezier preview. Keys stay sparse (card key frames only).
2. **Physics switches**: set the AutoPhysics panel toggles (Secondary / Compensation / Separation / Smooth rotation /
   Smooth trajectory) from our script.

## What exists
- `animpipe/cascadeur.py` (`casc-export/import/revert/diff`), `casc_prepare.py` (Cascadeur side: import onto the
  selected joints with up axis Z, thin keys, clamped bezier, save, export joints), `casc_ue.py`, 8 tests in
  `tests/test_casc_roundtrip.py`. The import compares with the `--ref` readback (all frames, `casc/<move>_ref.json`).
- Scene: `Saved/AnimPipeline/casc/overlord_dair_v4.casc` (from card 89966ed), keys 0/2/3/5/12/13/14/15/17/31/37/42.
- Cascadeur keys API, verified (`cascadeur_api_verified.md` §3, `sparse_apply.py`, `tangent_write_test.py`,
  `jab_read_keys.py`): `layer.key(f).left_weight_velocity / right_weight_velocity`, `key.tangents`
  (`Tangents.UserDefined/Continuous`), `layer.interval(f).interpolation`, written in `layers_editor().change_section`
  inside `modify_update`. Per-layer lag (overlap) as in `sparse_apply.py` `layers: {name: {keys, lag}}`.
- The build's spacing: card `spacing` profiles + `part_delays` (head/hand lag) + Sequencer tangents, in
  `animpipe/ue_rig.py` (`build`, `_seg_profile`). Read its header before changing anything.

## Suggested way (decide yourself, note it in the handover)
- Ground truth = sample the build per frame (`cascadeur.sample_take`, all 43 frames; editor via the hold/free protocol:
  ask the coordinator, the dair thread uses the editor too).
- Per segment and per layer, fit interpolation + tangent weights so Cascadeur's readback in-betweens match the build
  (metric: worst bone cm per in-between frame, target < 1 cm); write it in `casc_prepare.py` (or a `casc_spacing.py`
  it calls) from data the export plan carries. `part_delays` may need lagged keys on the head/hand layers: that adds keys
  off the card frames, so keep them out of the import (import must still treat only card key frames as poses).
- After applying: redo `--ref`; the round-trip guarantees must hold (no edit = identical card, locks, snapshot/revert).
- Physics switches: `cascadeur_api_verified.md` §6c (per scene, no setter; one PySide attempt hit the wrong panel). Max 2
  attempts; if it fails, report and stop.

## Rules
Do not change the dair card poses (locked owner f15/f23). Max 2 attempts per problem. Push to feature at green.
≤30-line handover appended to `anim-pipeline.md`. PR #24 (locks, not merged) also edits `__main__.py`.
