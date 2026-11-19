# Handover smoke A2 r4 (2026-11-19, laptop, Opus high) - STOPPED after 2 attempts, diagnosed, not reviewed
Branch `claude/project-thread-inmtkm` after e84efc16 (r3 Fable PASS narrow). Owner on r3: "a few puffs generated independently, not one
blow from the centre" -> one radial shockwave. Owner design note: the dust look is reused for dash/skid etc., only shape/direction change.
Colour decisions 1 and 2 of REVIEW-A2r3 still open: colours unchanged from r3.
## Done
- `setup_presentation_vfx.py`: shared dust layer. `DUST_SHAPES` presets (generator: shape ring/cone, ring or cone axis, push origin, count,
  life, size, speed, drag, curl) + `_dust(system, name, preset, look)`; the look stays the MIs from setup_presentation_vfx_puff.py.
  land_ring uses presets `land_shockwave` (14 puffs 24-40 cm, ring 8 cm, speed 380, drag 5) and `land_haze`; `dash_back` cone = example, not wired.
  Next step for designers (not done): expose the preset as Niagara user parameters.
- `_puffs` never applied `ring_width` (r2/r3 "filled disc" was the plain ring); removed from the call.
- `pie_capture_vfx.py` PIE_PROBE now probes LandRing emitters Skirt and Wisps.
## Result: FAIL before review (looked at once: docs/notes/reviews/vfx-v3/a2/r4/consec_*.jpg, pass2_dark_check.jpg)
Pass 1 (origin z -4) and pass 2 (origin z +6): the puffs rise as a column between the legs to knee/hip, no outward spread.
## Diagnosis (probe, docs/notes/reviews/vfx-v3/a2/r4/probe_landring.txt, from Saved/VfxV3/a2r4/probe/capture.log, `grep LandRing`)
- Skirt f1: pos ~(10,2,13), vel ~(-161,36,309): vertical 309 vs horizontal 165 = 62 deg up. Wisps f1: vel (-50,-49,191) = 70 deg up.
- `AddVelocity` "From Point" ignores our `Velocity Origin` (same angles with -4 and +6). Both emitters fit one hidden origin ~7 cm under the
  system pivot (skirt (8-O)/8 = 1.87, wisps (4-O)/4 = 2.7 -> O = -7). With r3's 36 cm ring the same bug gave only ~23 deg (its "late rise").
## Next (new thread; ask the Unreal Assistant first: Tools/UE59/ask_unreal_assistant.py, needs an open 5.9 editor)
1. Why AddVelocity From Point ignores Velocity Origin in 5.9 (other input name, space, or the template's own velocity module?).
2. If no quick answer: velocity without From Point, e.g. ShapeLocation ring + AddVelocity linear velocity from a dynamic input
   (normalize(Particles.Position.xy) * speed), or Ring's own "velocity along normal" if exposed. Probe again (PIE_PROBE=1), target vz < 0.2 vh.
3. Then capture dark+light (~4 min each, `vfx_v3_capture.ps1`), Fable review vs REVIEW-A2r3, owner mp4 only on PASS.
Cost r4 ~5 $ (r3 ~10 $).
