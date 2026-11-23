# getup-momentum-2 handover: stick cuts the get-up tail, one exit speed, directional roll (cloud, 2026-11-23). CODE DONE, NOT COMPILED
Branch `claude/project-thread-qjfnc0` (from `claude/project-thread-rqxsn8` 5c3f3470). Owner answers 2026-11-23: 1A, 2A (FAF stays at 85 %),
"i roll di getup rendili tutti della stessa velocità", "qualche animazione multidirezionale warpata verso l'angolo giusto?".
Reused (rule 1): FGSTN_TimelineTail, FGSTN_SteerWindow + FGSTN_StartupSteer::ResolveFrame + StepStartupSteer (SetFacing requests, the input
producer's hold already reads them), CancelForOtherCommand, FGSTN_GetupExit. Nothing new in Mover, no new sync state.
## Cause of "the stick does not cut the tail" (getup-momentum-1)
Not bIgnoreMoveInput: NO code ever cut a tail on the stick (only commands, IsCommandAllowed); the struct comment promised it. Fixed opt-in:
- `FGSTN_TimelineTail::bStickCuts` (default off: attack tails unchanged). Combat tick step 8b (`GSTN_CombatSimComponent.cpp`, after step 8):
  no command this frame + tail + flag + stick zone not Neutral -> `CancelForOtherCommand` -> ability ends `TailCancelled` -> FGSTN_GetupExit push.
  Pure check `FGSTN_CombatTimelineLogic::ShouldStickCutTail`. A stick HELD from the roll cuts on the FAF frame (wanted: run away).
- Get-ups set it: class bool `bStickCutsTail` (true), knob `gstn.Getup.StickCutsTail` (-1 data / 0 off / 1 on).
## Same speed (owner): `gstn.Getup.ExitSpeed` = 160 cm/s for all three (default 160; -1 = per-ability data; per-move knobs still win when >= 0).
`setup_reaction_anims.py` writes 160 in all three test abilities too (GETUP_EXIT_SPEED; not run, editor needed). Note: the 99/122/100 of the
review were jab-follow numbers (the jab does not walk); the data was 220/160/120. Roll TRAVEL speeds (80 cm vs 65 cm) untouched: clip-bound.
## Directional roll (owner question): YES, cheap version, no new clips
- Roll classes: `WarpMaxDegrees` 45 (0-90), `WarpFrames` -1 = until the first travelling frame (forward 6, back 5). Knobs `gstn.Getup.WarpMaxDeg`,
  `gstn.Getup.WarpFrames`. While still curled on the back, the facing turns toward the stick (roll back: its back toward it, new
  `FGSTN_SteerWindow::bAwayFromStick`) at the rate that covers the cap in the window (never a snap); the travel and the exit then follow the new facing.
  With the forward/back split at 90 deg, 90 covers 360 deg. 0 = off (also removes the default 20 deg reticle steer from get-up rolls).
- How it will look (guess, unverified): a pivot on the back during the rock/legs-up frames, readable up to ~45, odd past ~60 (a body
  spinning on its back). Needs new clips: side rolls (true lateral) and a stand-up that turns. Neutral stand-up not warped.
- Knob changes rebuild every get-up class default (cvar callback), so they work live in the GSTN Debug window (gstn.Getup.* group).
## Tests (not run: no UE here): GSTNCombat.Core.StartupSteer.Window.AwayFromStick, GSTNCombat.NP.Timeline.StickCutsTail,
GSTNDefence.NP.Getup.SharedExitSpeed, GSTNDefence.NP.Getup.DirectionalWarp.
## Laptop (after dropkick round 3): build, tests (GSTNCombat + GSTNDefence), then capture
`Tools/GASNPPTest/getup_capture_timed.ps1 -Runs Tools\GASNPPTest\getup_runs_momentum2.json -Out Saved\Getup\m2` (8 runs; new `angle` key:
stick turned off the axis, held 10 frames). Check: run variants move > 100 cm/s in FAF..+6 (was ~0); roll_idle 0; a45 travels diagonal.
Index entry (docs/INDICE-PROGETTO.md, steer section): add "get-up tail stick cut (bStickCuts, step 8b) and directional roll (bAwayFromStick)".
SUBAGENTI: 0
COSTO: ~6 $ | 0 min desktop | 0 token review
