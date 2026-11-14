# react-hook-5 capture (desktop, _ue59): handover

Date 2026-11-14, 20:55-21:15Z, worktree `GASP_Giustino2_ue59`, UE 5.9.

## Build
- Detached at 8952dd93 (the back-hit turn fix). Build 1 failed: `GSTN_CombatNPTests_HitFacing.cpp(46,60)` C2666, TestEqual ambiguous
  (FRotator::NormalizeAxis returns double on 5.9, tolerance float). Fix: `(float)` cast, commit **070b562a**, pushed. Build 2: **Succeeded**.

## Tests
- `Automation RunTests GSTNCombat.NP.Hits` (-nullrhi): **77 success, 0 fail**, both new `Hits.Facing.*` pass. Log `Saved/Logs/rh5/tests.log`.

## Capture
- back_hitstand + back_knockdown, one run, both hits landed (194 / 206 cm). Output `docs/notes/reviews/react-hook-5/` (2 mp4, 4 sheets,
  frames.jsonl, capture.log, README with data facts). Raw frames `Saved/AnimReact/react-hook-5`.

## For the rule-10 review (data, not a verdict)
- back_hitstand: yaw 0 on every frame (v0-v129): **no turn in game**, though the unit test for the no-push turn passes.
- back_knockdown: yaw 0 on v23-v30 (first Tumble frames, loc unchanged), snaps to 180 at v31, then holds 180 through KD, get-up and
  to the end of the capture (v209). The hit-frame turn is not visible; the "keep the facing after the reaction" part is.
- So the in-game result does not match the tests: worth checking whether the new rules flags are on in the kit used by the capture
  (KIT_Test_DefenceLanding / the test rules asset), or whether the yaw reaches the pawn only after hitstop.

## State left
- _ue59 on local branch claude/project-thread-a5idak = origin tip + this commit, built; Unreal closed; PM_BoostJump / PM_ThrowRider reverted;
  no maps saved. Logs `Saved/Logs/rh5/` (build.log, build2.log, tests.log, capture_try1.log, grid.log, yaw.txt).

Cost: about 60k tokens (Opus), low-medium.
