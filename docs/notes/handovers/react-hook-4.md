# react-hook-4 (desktop, _ue59): handover

Date 2026-11-14, 17:35-17:46Z, worktree `GASP_Giustino2_ue59`, UE 5.9.

## Build
- Tip 4e5ff4cf (c709fbc3 + CP1 merge 57301f8f) **fails** in CP1: C4458 in the generated
  `GSTN_GA_BaseCombat.gen.cpp(486)`. The UFUNCTION `K2_OnMoveStarted(int32 StartFrame)`
  (`Plugins/GSTNCombatNP/.../Abilities/GSTN_GA_BaseCombat.h:163`) hides the member `FSyncInteger StartFrame` (line 228).
  Fix: rename the parameter (e.g. `MoveStartFrame`). One line, not applied here (brief: drop CP1).
- **CP1 dropped**: _ue59 is checked out *detached* at **c709fbc3**. `git reset --hard` was refused by
  the session's permission guard, so I used a detached checkout. The branch pointer was not moved and nothing was pushed besides this note.
  Build Development Editor: Succeeded.

## Tests (c709fbc3)
- `Automation RunTests GSTN`: **1596 success / 0 fail** (baseline 1596/0).
- The 9 CP1 tests (MoveEvents.* x4, Input.Queries.* x5) do not exist on c709fbc3: not run.

## Capture
- **Not done.** I started it at 17:41Z. The coordinator stopped it at 17:43Z: the owner saw a lagging editor, and
  it was this capture editor (the only Unreal process running). I killed it, so it produced no frames.
- Its log: 366 warnings, 310 of them NP "Lag Compensation Rewind Exceeded". These are pre-existing (282-308 in earlier
  capture logs today). None come from ReactionAnim, MoveEvents or input queries.
- Lesson: `-log=<path with spaces>` makes UnrealEditor-Cmd exit at once. Use a bare `-log` and copy
  `Saved/Logs/GASP_Giustino2.log`.

## State left
- _ue59 detached at c709fbc3, built; PM_BoostJump / PM_ThrowRider restored; no editor running.
- Logs: `_ue59/Saved/Logs/rh4/` (build1/2, tests.log, procs.txt, capture_warnings.txt).
- Next: rename the param in CP1, rebuild, run the 9 tests, then the capture (needs ~5 min with the desktop free).

Cost: about 60k tokens, small.
