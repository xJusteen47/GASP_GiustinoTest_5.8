# Handover smoke A2 r5 (2026-11-19, laptop, Opus) - Fable PASS (narrow) after 2 targeted rechecks, to the owner
Branch `claude/project-thread-inmtkm` from ed8333e3. Goal: one burst from a central impact pushed radially along the floor; owner (same day):
dust must never shrink; owner reference video IMG_9543 -> breakdown first.
## Causes found
- Push origin (r4): AddVelocity "Velocity Origin" is a Position input that our set_input never writes; the Vector "Origin Offset" works.
  Probe Saved/VfxV3/a2r5/probe/capture.log (`grep "probe f1 NS_GSTN_LandRing"`): skirt v ~(300,0,52), vz/vh 0.15 (r4: 62-70 deg up).
  `_balls`/`_smoke` have the same dead setting, left as is (their effects passed with it). Unreal Assistant: no usable answer.
- Shrink: (a) smoke flipbook erosion threshold rose over life (-17 % radius, `vfx_size_monotonic.py`, size_before.txt); (b) the burst template's
  own update-stage ScaleSpriteSize curve (log "template size module off" in Saved/VfxV3/a2r5d/dark/run.log.ue.log). (b) was the visible one.
## Changes
- setup_presentation_vfx.py: `_puffs` Origin Offset + all template size modules in ParticleUpdate off; land_shockwave ring 12, origin z 6.5,
  count 18, life 0.42-0.46, speed 950, drag 22; land_haze speed 400, drag 8.
- setup_presentation_vfx_puff.py: new material param FadeCurve (fade = 1 - ramp^FadeCurve, default 1 = as before); SmokeReal RampLo/Hi
  0.05/0.9, OccStrength 0.4, FadeStart 0.40, FadeCurve 2.5; SmokeHaze StartScale 0.7, FadeStart 0.30, OpacityMax 0.45; PuffClump fades, no shrink.
- make_vfx_smoke_flipbook.py: constant threshold 0.24, soft edge th+0.35, normal k 10. Colours = r3 (owner decisions 1-2 open).
- New tools: `Tools/GASNPPTest/vfx_size_monotonic.py` (models the material only; template modules are checked by the build log),
  `Tools/GASNPPTest/vfx_ref_sheet.py` (crop sheets of any frame folder).
## Reference and study
`docs/notes/vfx-studies/fumo-owner-IMG9543/` (Italian study: method, 9-point breakdown, principles, r5 comparison; frames). Dossier
`docs/notes/reviews/vfx-v3/a2/r5/DOSSIER-r5.md`.
## Reviews (docs/notes/reviews/vfx-v3/a2/r5/)
r5b: critic NOT YET, Fable FAIL (shrink, brake, merging, matter). r5c: FAIL (shrink = template curve). r5d: PASS narrow
(REVIEW-A2r5.md last section: shrink gone, ring cover 0.89-0.97, brake 94 % at 0.15 s, width 80 % of A1b early, top 72-79 px vs knee 90).
Owner videos: video/A2r5_atterraggio_{dark,light}[_vicino].mp4 from Saved/VfxV3/a2r5d.
## Open (non-blocking, for the coordinator)
- Other dust emitters (`_smoke`, jump puffs, run-start, skid) may carry the same template size curve: apply the same off() when they move
  to the shared dust look. Make the "template size module" log a failing check.
- Dark-floor dust 1.90-2.04x the floor (r3 1.55-1.81x): owner colour decision; DarkSnow 0.26 -> 0.23 is an unmeasured estimate.
- Not captured: OFF pass, low preset; no blind critic on r5d. Hub umap must be restored after captures (`git checkout` it).
Cost estimate ~12 $ (3 capture pairs + probe, 1 critic, Fable review + 2 rechecks).
