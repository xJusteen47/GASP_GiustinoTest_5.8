# getup-momentum-2 handover: stick cuts the get-up tail, one exit speed, directional roll (cloud, 2026-11-23). CODE DONE, NOT COMPILED
Branch `claude/project-thread-qjfnc0` (from `claude/project-thread-rqxsn8` 5c3f3470). Owner answers 2026-11-23: 1A, 2A (FAF stays at 85 %),
"i roll di getup rendili tutti della stessa velocitÃ ", "qualche animazione multidirezionale warpata verso l'angolo giusto?".
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
Decision 2026-11-23 (coordinator, recommended default): roll travel left as is (80 / 65 cm, clip-bound), only the exit speed is shared.
SUBAGENTI: 0
COSTO: ~6 $ | 0 min desktop | 0 token review

## Laptop run (2026-11-23 evening, getup-momentum-2 thread, worktree GASP_Giustino2_qjfnc0, content junctions to the main folder)
- Build OK 1424 s: `Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build_m2.log` (no fix needed). Tests: `Run-Timed.ps1 -Preset Test -Tests "GSTNCombat+GSTNDefence"
  -Log Saved\Logs\ci\test_m2.log` + `Check-TestLog.ps1 -UeLog Saved\Logs\ci\test_m2.log.ue.log` -> PASS ok=799 fail=0; the 4 new tests Success
  (data/new_tests.txt, data/build_test_esito.txt). Data step (setup_reaction_anims.py) skipped: knob already 160, BP save on the laptop needs an owner OK.
- Capture `getup_capture_timed.ps1 -Runs Tools\GASNPPTest\getup_runs_momentum2.json -Out Saved\Getup\m2 -MaxMinutes 15`: 501 s, ASSERT: OK
  (data/m2.assert.txt). Media: `getup_review_media.py docs\notes\reviews\getup-momentum-2 Saved\Getup\m2:m2` -> m2/<run>.mp4 + _az180/_az90.png.
- Speeds `python Tools/GASNPPTest/getup_speeds.py Saved\Getup\m2 21 60` (data/speeds.txt; cm/s, heading deg; method checked = getup-momentum-1 new3):
  stand_run 96 / 160 | roll_run 6 / 119 | rollback_run 10 / 118 | roll_idle 0 / 0 | roll_a45_run 132 / 386 heading -138 (axis 180: 42 deg off)
  rollback_a45_run 100 / 368 heading 41 (axis 0: 41 off) | roll_a80_max90_run 99 / 369 heading -101 (79 off) | roll_a45_warpoff_run 94 / 294 heading -171 (straight).
- Straight roll_run/rollback_run ~0 in FAF..+6 = CAPTURE DRIVER, not the cut: for runs without "angle" the driver lets the stick go when the get-up starts
  and presses it again on the FAF video frame (pie_capture_react_ingame.py:365, :373); the roll tail is 3 sim frames, the montage plays to its natural end
  and the push never comes (data/speed_trace.txt: 0 until +24, then the walk ramp). The held-stick runs (all *_a45*, a80) cut at FAF with the push; stand_run
  (6-frame tail) cuts 2 frames after the press (156 cm/s at +23). Open question for the owner/coordinator: a stick PRESSED on the FAF of a roll arrives after
  its 3-frame tail; widen the roll tail or accept it.
- Extra try (rule 5, 2nd capture): straight rolls with the stick held (`angle` 0.01, data/runs_m2held.json, -Out Saved\Getup\m2held) -> ASSERT: FAIL
  "no get-up" on both runs (data/m2held.assert.txt), not investigated. Frames (2 grids): a45 stands up on the turned facing and runs diagonally;
  a80/max90 stands facing the side camera, no snap seen in +12..+37.
SUBAGENTI: 0
COSTO: ~4 $ | 55 min laptop (build 24, test 3, captures 16, media) | 0 token review

## Retry 2026-11-23 22:19 (portatile, coordinator brief, 1 capture)
- `getup_capture_timed.ps1 -Runs Tools\GASNPPTest\getup_runs_momentum2_held.json -Out Saved\Getup\m2held2 -MaxMinutes 15` at b0d557d5 -> ESITO: OK (123 s), ASSERT: FAIL "no get-up" on all 3 runs (data/m2held2_assert.txt).
- Cause in data/m2held2_capture.log at 29.7 s: `_apply_loadout` -> `_srv` KeyError 'BP_GASNPP_TestCharacter_C_0' (gasnpp_test.py:1802), so the sequences never ran; no speeds, no media. Not investigated (brief).
- Note: an editor of another thread (cp1verify, animpipe editor_session.py, pid 4956) started at 22:19:52, the same second as this capture; possible interference, not checked.
- No Blueprint saved, setup_reaction_anims.py not run. Lock freed.
COSTO: ~1 $ | 6 min laptop (capture 2) | 0 token review

## Round 2 (cloud, 2026-11-23 late, after the cloud review FAIL on "same speed")
- Cause of the roll ramp 8 -> 140 (speed_trace.txt, roll_a45_run +21..+26): with the stick held the cut lands ON the first tail frame; the cancel
  reaches the ability in the ASC pre-tick before its own tick moves EvaluatedFrame there (20 < tail 21) -> end reason Interrupted -> NO exit push,
  only the walk accelerating from 0. The stand-up was cut a frame later (stick re-pressed), so it got its 156. Fix: `FGSTN_MoveEventRules::EndReasonFrame`
  (a cancel takes the combat clock's frame when further on), used in `UGSTN_GA_BaseCombat::FinishTimelinePlay`; test lines in GSTNCombat.NP.MoveEvents.EndReason.
  Generic (all moves): a cancel exactly on a tail's first frame now reports TailCancelled instead of Interrupted. Tail/FAF timing untouched.
- Spike 140 -> 378 -> 234 at roll_a45 +27 (one frame, not in warpoff): NOT explained, not fixed. Guess: the pushbox against the attacker (the
  diagonal run passes it) or the run-start layer; check on the next capture whether it survives (frames.jsonl attacker position at +27).
- Laptop retry m2held2 FAILED before any sequence (KeyError in _apply_loadout, another thread's editor started at the same second): capture still owed.
  Next: build + MoveEvents/Defence tests + `getup_capture_timed.ps1 -Runs Tools\GASNPPTest\getup_runs_momentum2_held.json` plus roll_a45_run, laptop ALONE.
