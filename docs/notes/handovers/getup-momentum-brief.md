# Brief: knockdown get-ups keep momentum, tail after FAF (getup-momentum-1, 2026-11-21)
Owner ask "rialzate" (via coordinator): both get-up kinds (rolls AND the neutral stand-up) keep a bit of momentum into the next move,
and each plays its end-of-clip tail after FAF; the tail is cut when the player acts (FAF TAIL RULE: report the frame range cut).
Machine: laptop, folder `GASP_Giustino2_n216qs` (GASP sample content), branch `claude/project-thread-rqxsn8` from
`claude/runstart-generic-desktop` 84cc0057 (fast capture scripts). Does NOT touch `_ue59`. One model (Opus), one checkpoint.
Budget: 18 $ (code + 1 build + tests + 2 capture sessions + handover). Stop at the first stable point when reached (rule 8).

## Today (read from code, 84cc0057)
- `GA_GetupNeutral_Test` (AM_React_GetUp), `GA_GetupRoll_Test` (AM_React_Roll, 80 cm toward the feet), `GA_GetupRollBack_Test`
  (AM_React_RollBack, 65 cm toward the head), all in `/GSTNDefence/Test`, written by `Tools/GAS/setup_reaction_anims.py`: timeline =
  clip length, FAF = end, no tail, last pose held over the blend-out. The roll zeroes its velocity on its last travel key (no carry).
- The engine for the ask exists: `FGSTN_TimelineTail` (cancellable from its start by stick, jump or any command, montage blends out over
  `BlendOutFrames`), `OnMoveEnded(EGSTN_MoveEndReason::TailCancelled)`, `UGSTN_GE_PushBack` (the roll's own push).

## Plan
1. Tail (data, setup script): per get-up FAF = first clip frame where the body is up enough to act (pelvis >= ~85 % of its last-frame
   height, both feet planted; measured headless from the clip, numbers in the handover); `Tail.bHasTail`, start = FAF, blend-out 6;
   timeline length stays the clip length; no hold pose at the natural end (the clip plays to its own end). Range cut = [FAF, end).
2. Momentum (C++, GSTNDefence NP): on `TailCancelled` the get-up leaves an exit velocity for the next move (one-frame PushBack):
   rolls along their travel direction, the stand-up along the facing. Not applied when the tail plays out (no slide on idle, no skating
   while rising). Data `ExitSpeed` on `UGSTN_GA_GetupRoll`/`UGSTN_GA_GetupNeutral`; cvars `gstn.Getup.RollExitSpeed`,
   `gstn.Getup.RollBackExitSpeed`, `gstn.Getup.StandExitSpeed` (cm/s, -1 = data), listed in the GSTN Debug window, one control each.
   Starting values (rule 6, tunable): roll 220, roll back 160, stand-up 120 cm/s.
3. Tests: exit-speed rule (pure) + automation suite `GSTNDefence` green. Real build (Run-Timed) before any capture.
4. Captures (`pie_capture_react_ingame` sequences on the fast multi-run path, behind AND side): BASE = today's data (one session), then
   variants in ONE session: tail only (speeds 0), default speeds, x1.5; each get-up with (a) no input = tail plays out, (b) stick held
   toward the travel at FAF = run out with momentum, (c) jab at FAF. ASSERT: get-up montage weight >= 0.8 in its window.
5. Handover + mp4/grids in `docs/notes/reviews/getup-momentum/`, to the coordinator for the cloud review (rule 10), not the owner.

## Identity traits (rule 13a; poses are NOT changed here, the review checks they survive)
- Roll: rock back on the upper back, sit-up with hand push, low tuck over the feet, rises facing the same way (react-v3 roll-travel.md).
- Roll back: legs swing up and over the right shoulder, hands planted, feet land, rises facing the same way.
- Stand-up: sits up, crouches, stands in place (no travel while rising).

## Rules
- Rule 14: no change to GASP locomotion, ABP or Mover modes (abilities + data only); the walking mode just starts from a velocity.
  If the exit velocity makes GASP pick a different start/pose that reads wrong, that is a "gioco" note, no divergence row added.
- Rule 13e: no arc or timing of the clips is shortened; FAF only marks where control returns, the tail still plays when nobody acts.
- Risk: the knockdown kit is shared with the hub dummies (`KIT_Test_DefenceLanding`): the dummies get the tail too (no input = same as now).
