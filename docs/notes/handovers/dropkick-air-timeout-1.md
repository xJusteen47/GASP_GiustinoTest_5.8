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

## 3. Ground rules + Landing track (BUILT in code, owner 2026-11-16 "parti"; not compiled)
- `FGSTN_CombatTimelineData::GroundRule` (Any default = today / Ground / Air), `TakeoffFromFrame` / `TakeoffUntilFrame` (Ground only).
  Ground move in the air -> CANCELLED (ledge, push, teleport), except: from a take-off inside the window (wizard dropkick) or a coyote start.
  Synced bit `FGSTN_CombatSyncCore::bAirborneOk` (1 bit on the wire, cleared on ground/with the timeline).
- New track `FGSTN_TimelineTrack_Landing` (`GSTN_Landing.h`): window + `Action` AutoCancel / LandingLag (`Lag` = an FGSTN_RicochetResponse:
  frames, montage, cue, movement; played by the RECOIL player via `StrikeTrack` = landing track index) / ContinueWithMove (`ContinueMove`
  class, started on the touchdown frame like a FollowUp; registered by `AppendFollowUps`). Outside windows = plays on. Ground moves land only
  after a take-off. Rules pure in `FGSTN_LandingRules::Resolve`, called in `AdvanceClock` before Hold Until Landed.
- Coyote per move: `Input.Context.Coyote` (combat host adds it in the air while `Jump.CoyoteFramesLeft > 0`); `FGSTN_TagRequirements::
  bGroundedInCoyote` (not edited: `UGSTN_CombatTableSet::ApplyCoyoteFlags` sets it for moves with `AllowsTakeoff()`); a coyote match ranks
  +1 so it beats the aerial of the same command.
- Applied choices (rule 6): default Any so no current move changes until data is set; landing lag reuses the recoil (no new anim system);
  ContinueWithMove needs its own ability (own hitboxes/frame data). Not done: data on the real moves (jab/ftilt = Ground, dair = Air +
  windows) -> next desktop/anim thread; a validator hint for Landing windows past TotalFrames.
- Tests: `GSTNCombat.Core.Landing.GroundRule`, `.Windows`, `GSTNInput.Core.Input.Bindings.Coyote`. Desktop: build, these + all
  `GSTNCombat`/`GSTNInput` tests (bindings selection changed), PIE: walk off a ledge during a jab with GroundRule Ground -> air idle.

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
- B) owner: coyote only for ground moves that go airborne -> done as above (section 3).
