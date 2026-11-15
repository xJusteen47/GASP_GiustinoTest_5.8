# MONTAGE-HALF-RATE handover: cp5 review game-side bugs 1-3 (cloud, NOT BUILT, 2026-11-15)
Branch `claude/project-thread-lc9e27` from c9tzis `bfbf50b7`. Cloud thread (Opus): nothing compiled or run. A desktop thread builds and verifies.
- **Bug 1, half-rate montage: cause found and fixed** (c787bdbb, fork `NetMontageSimulator`). The engine's montage instance adds every update's
  delta to its sub-stepper and spends it only when the play rate is not 0, all at once (Epic says so in `EngineOverrides/Mover/.../MoverComponent.cpp`
  ~3090). LandOnSimTime holds (rate 0) on a start, a loop wrap or when on target, so the next frame moved 2 ticks, landed on the next target, held
  again. Model reproduces the captures exactly: jab 0 2 2 4 4 6 then 6.04 (cycle ends on a double tick), dropkick wrap 15.98 18.97 18.02 20.10
  (3 kept deltas). Rows: `docs/notes/reviews/cp5-data/jab/jab1_jab2_faf.jsonl` f21-f43, `dropkick/dropkick_whiff.jsonl` f68-f90 (`historyFrame` = 1 tick
  a frame). Fix: `ComputeLandOnSimTimeStep(..., CarriedSeconds)` divides the gap by kept + delta, returns `CarriedSecondsAfter`; the simulator keeps
  the estimate per `FAnimMontageInstance::GetInstanceID()`. Presentation only, sim untouched. Old tests used a wrong instance model (pos += rate*dt):
  now an engine-like model; new test `GSTNCombat.NP.MontageSync.StepperCarry` (reproduces 0 2 2 with the old path, 0 1 2 3 with the fix).
- **Bug 2, victim wrong pose for one frame: fixed** (Mover fork `FMoverDefaultSyncState::Interpolate`). It is the back-hit turn: the dummy faces away
  (az0 v25 shows its front, v27 its back), the sim turns it 180 deg in one tick, smoothing drew v26 half-way (side-on). An orientation change above
  cvar `Mover.Interpolation.OrientationSnapDegrees` (60, 0 = off) now snaps. Test `GSTNCombat.NP.Hits.Facing.InterpolateSnap`.
  NOT done: the example kit timing (hit at frame 4-6, clip push ~20 frames later). No field starts a montage mid-clip; options for the owner below.
- **Bug 3, shake ~1 px: no code fault found.** The shake runs along the PLAYER camera's screen-right (by design, CP4); the review measured the side
  view (az90), which sees it edge-on. az0 crops v27-v31 move a few px side to side (crude, inline). Order is fine (NP finalizes pre-actor tick).
  Amplitude is already a setting (`HitstopShake.VictimAmplitudeCm` 3 = 6 cm p-p); judge it in playtest/game view.
**Desktop verify, in order**: 1) merge this branch into the work branch, timed build of _ue59 (editor closed). Likely compile spots: `GetInstanceID()`,
  `AngularDistance` double/float, `Yaw` cast in the new test. 2) headless `Automation RunTests GSTNCombat.NP.MontageSync` (5 green) +
  `GSTNCombat.NP.Hits.Facing` + full `GSTN`. 3) PIE harness `abilities`, `forced_rollback`, `combat_spike`: all `CMP OK`. 4) recapture with
  `pie_capture_move.py`: jab1_jab2_faf (KIT_Test_Overlord), dropkick_whiff (dropkick kit + hold script), cue_hit (KIT_Example), dair_whiff (V9).
  5) `cd docs/notes/reviews/cp5-data && python tools/count_repeats.py` on the new rows: expect 0 repeats outside hitstop/hold; first rows of a move
  0, 1, 2, 3 (or 1.04 steps); `tools/rv_diff.py <mp4> <first_v> side`: no 0 px frame outside hitstop. 6) cue_hit az0/az45 on the hit frame: the
  victim already faces the attacker. 7) shake: look at the game view and az0, not az90.
**Applied choices (rule 6)**: snap threshold 60 deg as a cvar; carry estimated (engine field is private), assumes one anim update per world tick.
**Owner decision (example kit)**: A) new timeline field "montage start frame" so a stock clip starts at its windup (~$8-12 + desktop build,
  recommended: the public plugin needs it for any stock clip); B) move the example light's hit to frames ~26-30 (data only, slow light);
  C) swap the clip for one with an early contact. Until then the example light reacts before the push.
**Risks**: a mesh that skips anim updates (offscreen) makes the carry estimate wrong for a frame (self-corrects next frame); ChaosMover also calls
  Interpolate (not used by the game's NP backend). **Cost**: one Opus thread, no subagents, no builds: about $12-16 (estimate, not measured).
