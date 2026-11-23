# Playtest fixes 2026-11-17 (desktop, _ue59, Opus). Branch `claude/playtest-fixes-1117` from `claude/kit-playtest-1116` 9658b271
Merged: `claude/project-thread-ipt5wf` (ground/tech bounce, handover ground-bounce-1.md), `claude/project-thread-hewki6` (kit tilts, launch-light-kit-1.md).
## Done
- R1a dead zone: ONE per-player option `UGSTN_InputOptionsSubsystem` (GSTNInputCore, LocalPlayer subsystem, SaveGame slot per player), radial + remap
  in `UGSTN_PlayerInputSource`. IMC_GSTN_Locomotion dead zone 0.12 -> 0.02 floor (setup_player_input_contexts.py). A player-controlled pawn inside
  its dead zone gets a zero Mover intent (ApplyAnalogMoveIntent, else GASP's IMC intent would creep). AnalogWalk defaults InnerDeadzone 0, MinSpeedFraction 0, StartAccel 1.
- R1b ramp: `FGSTN_RunStage::AccelRampFrames/RampStartFraction` + state `RampFrames/RampStage` (compact net form, ==); applied in ApplyStageValues.
  DA_Locomotion_Default: run 6 f from 30 %, sprints none.
- R1c thresholds: `FGSTN_StickThresholds` (Neutral 0.2, Aim 0.5, Flick 3) removed from UGSTN_InputSettings/ini. Travel in the loadout
  (`FGSTN_LoadoutInputFeelExt`, wire id GSTN.InputFeel, stays on kit swap) -> table set per revision -> input sim. Owning client sends them with
  `ServerSetInputFeel` (new revision); a request without the entry keeps the applied one. Console `gstn.Input.Options [DeadZone|Neutral|Aim|Flick v]`.
  Hardcoded 0.25 (StartupSteer, MomentumSlide) NOT moved to data yet (budget).
- Mario 3D World preset: ini (JumpVelocity 1240, rise grav 0, cut/fall 4.1, MaxFall 1800, FastFall 6.0/2600, hold 2..11, AirJump 900, AirControl 0.85),
  kits 1450/1270/1100 jump, fall 3.6/4.1/5.3, air 0.95/0.85/0.65; stages 470/610/850 etc. via Tools/GAS/setup_playtest_fixes_1117.py;
  PlatformJump C++ defaults Coyote 6, CutRamp 3 (assets that override keep theirs, not checked). Squat stays 3 (owner 1A), full jump 2.3 heights (2A).
- Dropkick: Self Landing Bounce track 25 cm (new `UGSTN_DefenceContentLibrary::SetSelfLandingBounceTrack`); TL was guard-"edited", added with force, backup Saved/Backups/SetupGuard/20261117-012932.
- Overlord kit (owner 00:23Z): WPN_Test_Overlord + DAir V9 (CrouchNeutral, airborne) + Dropkick (Directional, airborne). FTilt: no ability/timeline exists (missing entirely).
  A rerun of setup_overlord_jab_kit.py drops them: rerun setup_playtest_fixes_1117.py after it.
- Side flip later: `Movement.State.Skid.JumpWindow` is already in the input context, a kit binding can require it (no code needed).
## Not done (next thread): R0 measurement, R2 run-start clip, R3/R4 skid + wall slide clips/IK rig import and ABP review from GaspMoverProject; 0.25 constants to data.
## Proofs
Build: `Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\pf1117-build2.log` ESITO OK (build1 FAIL: static GetOwner, fixed). Tests: `-Preset Test -Tests GSTN`
-> Saved\Logs\ci\pf1117-test1.log, 1719 Success / 0 other (`Select-String "Test Completed. Result=\{(\w+)\}"` grouped). Scripts: Saved\PlaytestFixes\*.txt.
Not committed: AM_OverlordDAirV9.uasset (dirty before this thread), _ue59/, Tools/AnimPipeline/cards/overlord/jab/director/.
Cost estimate ~24 $ (Opus, ~140k context, 2 builds, 1 test run).
