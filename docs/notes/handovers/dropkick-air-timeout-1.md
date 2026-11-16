# Dropkick air timeout + landing lag / coyote report (2026-11-16)

Branch `claude/project-thread-pcq7jx` from `claude/integ-night-1116` 194ad7e3. Cloud thread: NOTHING compiled or run. Opus, ~5 $.

## 1. Air timeout (done, not built)
- `FGSTN_TimelineTrack_HoldUntilLanded::MaxAirFrames` (frames, data per move, 0 = off). Only moves with a Hold Until Landed track can
  stay in the air forever, so every other move is "off" by construction.
- Rule in `FGSTN_HoldUntilLanded::ResolveClock(..., ElapsedFrames)`: in the air, still in the air part (`CurrentFrame < EndFrame`),
  `Frame - TimelineStartFrame >= MaxAirFrames` (hitstop frames included) -> `Timeout`. Ground wins: a landing in time is unchanged.
- `FGSTN_CombatTimelineLogic::AdvanceClock`: Timeout = `ClearTimeline` + return false with an empty output -> hitbox closed this frame,
  `bFinished` false so NO follow-up and no montage release; the ability ends like an interrupt, montage blends out with its own blend,
  locomotion = air idle. Air control untouched. Rollback-safe (pure function of synced state).
- Dropkick default **125 frames = 2 s** in `Tools/GAS/setup_dropkick_hold_until_landed.py` (env `DROPKICK_MAX_AIR_FRAMES`, 0 = off).
  Choice applied (rule 6): 2 s is longer than any normal jump+dropkick, short enough not to feel stuck.
- Test: `GSTNCombat.Core.HoldUntilLanded.AirTimeout` (new). Helper `SetHoldUntilLandedTrack(..., MaxAirFrames = 0)` new last param.

## Desktop build steps
1. Merge this branch into the playtest work branch (it sits on integ-night-1116). Close the editor; build _ue59 (45 min script).
2. Tests: `GSTNCombat.Core.HoldUntilLanded` (Clock, Montage, AirTimeout) + `GSTNCombatNP` HoldUntilLanded tests.
3. Editor: rerun kit script -> `setup_dropkick_hold_until_landed.py` -> body capsule script (same order as before). Log line must say `air timeout 125 frames`.
4. PIE check: dropkick off a high ledge -> after 2 s the character falls in air idle, no hitbox (`show collision`); normal dropkick lands as before.

## 2. Landing lag today (report)
- Attacks (`UGSTN_GA_BaseCombat`, dair, aerials) have NO landing logic: landing mid-move, the timeline plays on to its end on the ground
  (air clip, hitbox and all). No autocancel window, no landing lag. Only exception: Hold Until Landed (dropkick) jumps to its land clip.
- Landing lag exists only for movement moves (`UGSTN_GA_MoveTimeline`: dash, air dodge): movement track `bCancelOnLand` + `LandingLagFrames`,
  checked in the ability (`GSTN_GA_Dash.cpp` OnSimulationTick), not in the combat clock. Spin Drill has its own.

## 3. Proposal: per-move landing rule (not built) — "da confermare" to owner
- New track `Landing` (generalises the hold's Land jump): windows `[StartFrame, EndFrame)` with `Action`: `AutoCancel` (end, optional
  `LagFrames`), `LandingLag` (jump to a lag part of the timeline/montage section, own actionable frame), `Continue` (jump to a ground
  continuation part: the owner's "attack continues with a ground clip if landing before the startup ends"), `None` (play on = today).
- Same mechanics as Hold Until Landed (clock jump + montage section jump, already written and rollback-safe); Hold becomes `Landing` +
  `bHoldInAir`. Default for aerials with no track = today's behaviour, so nothing breaks.
- Cost: ~10-14 $ Opus (core rule + clock + montage reuse + tests + timeline editor colour); clips are anim work apart.

## 4. Coyote time today (owner question)
- Jump only: `FGSTN_JumpLogic` opens the window on ground (`CoyoteFramesLeft = CoyoteFrames`), counts down in the air, a jump zeroes it.
  Default 4 frames (64 ms) from `UGSTN_CombatSettings::JumpCoyoteFrames` (a project setting: against the "no project settings for
  gameplay" rule; the kit override `UGSTN_GA_PlatformJump::bOverrideCoyoteFrames` exists). Launch pads: `bResetCoyote` / kit rule Clear.
- No coyote for attacks: walking off a ledge and pressing attack 1 frame later gives the aerial. Ground read is one frame late (Mover end
  of previous frame), so the effective window is ~5 frames.
- A) DONE (coordinator, rule 6): `JumpCoyoteFrames` removed from `UGSTN_CombatSettings` and its ini; `bOverrideCoyoteFrames` removed;
  `UGSTN_GA_PlatformJump::CoyoteFrames` (default 4) is always read from the kit's jump binding (`GSTN_CombatSimJump.cpp`). Same value
  for every kit today. Caveat: a BP child with override OFF but a non-4 CoyoteFrames would now use its value (none known). Build check:
  `GSTNCombat.Core.Jump.*` + a ledge jump in PIE.
- B) "attack coyote" (ground attacks allowed N frames after leaving the ground, ~4-6 $): waits for the owner, like the Landing track.
