# QA movimento 1117 (laptop, Opus, UE 5.9). Branch `claude/qa-movement-1117` from `claude/playtest-fixes-1117` c2a2c1d6
## Done
- 0.25 constants -> data, same value: `FGSTN_SteerWindow::StickDeadZone` (per move: start snap, reverse floor, steering; choice: move data, not rules)
  and `FGSTN_MomentumSlide::StickDeadZone` (latched + serialized in the slide layered move). Unit tests added (AttackAim.Snap, MomentumSlide.StickRetreat).
- In-game QA: `Tools/GASNPPTest/movement_scenarios.py` (7 scenarios, listen host+2, judged on authority AND owner, numbers in `Saved/Logs/qam/<name>.json`),
  launcher `Tools/GASNPPTest/run_movement_qa.ps1 [-Names ..] [-Tag ..]` (rendering editor, DefaultLevel, kit `KIT_Test_Playtest`, env QAM_KIT).
  Probes: test component sim history now has run stage / run frame / skid phase + skid Mover tags; `GetPresentationSnapshot()` (foot slide, lean,
  montage, override layer, bone heights, ABP variables, shared TurningRate/MaxSpeed, run airborne).
- Clips: `pie_capture_move.py` MOVE_SEQUENCES (mv_run_stop, mv_skid, mv_walk_low, mv_jump_full, mv_jump_tap, mv_air_reverse, mv_fast_fall) +
  PIE_CAPTURE_VIEWS; grids az90 in `docs/notes/reviews/qa-movement-1117/` (frames in Saved/QAM/capture1, not committed).
## Defects found
1. FIXED: KIT_Test_Playtest had JumpVelocity 700 (pre-preset): apex 1.10 heights. `Tools/GAS/setup_qa_movement_1117.py` sets the kit's jump stats
   to the preset (a stat cannot be removed: the init effect writes 0, seen in run3 = no jump). Now apex 419 cm = 2.44 heights, air 0.96 s.
   The uasset is committed; on a merge conflict take the other side and rerun the script.
2. OPEN (owner, input design): the playtest kit has NO Command.Movement.FastFall binding and no kit has one; Crouch in the air is the air crouch.
   Fast fall is unreachable in the playtest (scenario move_fast_fall FAIL by design until a binding exists).
3. OPEN (2 attempts used): air turn is 300 deg/s constant (yaw trace in move_air_control.json), not the profile's 3600. Attempt 1: shared
   TurningRate raised to the air turn rate while airborne (GSTN_RunStageSyncState.cpp ApplyAirValues; kept: it was a real 2000 cap). Attempt 2
   (diag): TurningRate reads 3600 in the air, still 300 deg/s. Next suspects: StepAirYaw step (LastStepMs), another orientation writer after the
   air turn intent (GASP smoothing / RunFacingTime), the falling mode's own smoothing. Grid: air_reverse_az90.png (turns over ~30 frames).
4. Note: analog walk tilt 0.2 -> 32 cm/s (7 % of run), 0.12 -> ~0: by design (player dead zone 0.08 + exponent 1.4), smooth, no step. Feel call for owner.
## Results (Saved/Logs/qam/batch-run4.log + run5/run6; count: `Select-String "PASS|FAIL" Saved/Logs/qam/batch-run*.log`)
run_stages PASS 6 (start ramp 90 % at frame ~17, stages 0/1/2 = 472/612/853 cm/s, stage 1 lasts 99 f), skid PASS 5, analog_walk PASS 3 (run3),
jump_full PASS 4, jump_tap PASS 2 (tap 0.47 of full, cut 4018 cm/s2), air_control FAIL (3), fast_fall FAIL (2).
Unit: `Run-Timed -Preset Test -Tests "GSTNCombat.Core.AttackAim+...+GSTNCombat.NP"` -> Saved/Logs/ci/qam-test1.log 221 Success / 0.
Builds: qam-build1 FAIL (lean field), build2..4 OK. Full GSTN suite NOT rerun (budget).
Harness note: the authority's ticks sit ~0.3 s later on the wall clock than the owner's; judge per copy from its own events, not wall windows.
Pillow installed with pip --user on the laptop (grid tool). Cost estimate ~16 $ (Opus, 4 builds, 6 PIE batches, 1 capture).
