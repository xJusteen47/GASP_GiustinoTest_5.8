# Handover VFX v3 r5 (2026-11-19, laptop, Opus; captures r8 + r9)
Branch `claude/project-thread-ii4go3` from f847529a (29srbd). Packs `docs/notes/reviews/vfx-v3/r8/` (12 sequences) and `r9/` (light_ground_bounce
only). Reviews: `r8/REVIEW.md` Fable high **FAIL, very close (11/12)**; `r9/REVIEW.md` targeted recheck: **PASS** (round PASS: cloud 6-10% brighter than the white floor, still lobed).
## Done
- gioco/cattura, sprint lines (r7 FAIL): `pie_capture_vfx.py` parked the dummy "15 m behind the yaw" and placed the backdrop "ahead of the yaw";
  the sprint starts at yaw -180 but runs world +X, so the host hit the dummy 16 frames after StageUp (r7/light2 run.log.ue.log: StageUp 3200,
  StopRelease 3216 mag 830) and ran over the hub floor. Now: dummy 30 m on -X; backdrop yaw 0 for stick sequences. r8: StageUp 3241 ->
  StageDown 3355 (r8/light2 run.log.ue.log). Lines visible on both floors.
- posa/look, impact cloud (r7 FAIL "flat blob"): `setup_presentation_vfx.impact()` = Core 3 x 200-240 cm at t0, Lobes 2 x 140-180 cm at
  +0.04 s, Tops 2 x 100-130 cm at +0.085 s, own MI `SmokeCloud` (ShadeMin 0.62, rim 0.68 / width 0.5, opacity 0.8, lift 0.4). Grains 9-12 cm.
  r8: dark cloud lumpy (PASS); light read grey (the blocker). r9: `M_GSTN_VFX_Smoke` gets `*Light` twins of ShadeMin/RimShade/EmissiveLift,
  blended by surface G 0.4->0.7; SmokeCloud light 0.78 / 0.76 / 0.55; other MIs twins = same values (no change).
- dati, dark-floor dust (r7 FAIL "cream"): capture dark surface 0.30,0.28,0.26 -> 0.22,0.215,0.21 (0.15 tried: read as soot, run r8/dark
  discarded). In game a dark floor needs its own `SurfaceColors` entry (Physical Material) near this value.
- Light dust rim 0.8 -> 0.72; strong-hit FlashStreak ShrinkStart 0.35 -> 0.12 (r8: still little shrink, see below).
## Remaining (r8 notes, none blocking unless r9 says so)
1. posa/look: strong-hit streaks: cap 4 (dark shows 6-8, User.Count from the cue table), 50% length by frame 4; light shows almost no shrink
   through v44: check the Flash life curve vs hitstop (system slowed to 0.15).
2. posa/look optional: sprint lines thin on light (width x1.5-2, 5-6 lines); dark dust slight khaki cast (desaturate 10-15%).
3. posa/look: 6-10 white sparkle dots on strong hits and landings.
4. gioco/cattura: bounce cloud first visible 2 frames after the logged contact; skid sampled once (v142); end sprint sampling at v270.
## Runs (all ESITO OK, Saved/VfxV3/r8-*.out, r9-light.out)
r8/light (with -Pre v3+systems), light2, light3 (sprint after the backdrop fix), dark (0.15, discarded), dark2 (stopped), dark3, dark4;
r9/light (ground_bounce, -Pre v3). Packs merged in Saved/VfxV3/r8pack, r9pack.
Cost: ~45 min editor (8 runs), 2 Fable reviews (~135k + small); ~9 $ estimated.
## Real-speed check (coordinator: project rule, game camera at real speed)
- r9 `r9/REALSPEED.md` (8 sheets of consecutive 60 fps game frames): FAIL on sprint only (no readable line on light, flecks on dark); hits,
  bounce, landing pass (no popping, flicker, soot, cream). Notes: star lives 1 frame, streaks frozen over the legs 8-9 frames, bounce cloud
  first readable 5 frames after contact, a ring at v153-v154 before the logged landing v155.
- Fix: speed lines 16/s per side, life 0.30-0.38 s, 6 x 170 cm. r10 sprint recaptured (Saved/VfxV3/r10, both ESITO OK): `r10/REVIEW.md` PASS.
- Owner video: `r10/vfx_v3_light_dark.webp` (game camera, light | dark, 60 fps, built by `Tools/GASNPPTest/vfx_side_by_side.py`).
## Owner 2026-11-19: sprint lines off for now
"le linee dello sprint per ora levale": StageUp row removed from `setup_presentation_table.py` (kept as a comment to restore),
`DA_GSTN_Presentation_Default` rebuilt (11 rows). NS_GSTN_SpeedLines stays built (r10 PASS version). Check: r11 sprint run, Sprint 2
StageUp mag 3 logs `niagara=-` (Saved/VfxV3/r11/light/run.log.ue.log, grep "_C_0 .*StageUp").
