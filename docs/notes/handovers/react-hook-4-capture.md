# react-hook-4 capture (desktop, _ue59): handover

Date 2026-11-14, 20:19-20:45Z, worktree `GASP_Giustino2_ue59`, UE 5.9, owner playing Fortnite at the same time.

## Build
- _ue59 detached at tip **1b641dca** (CP1 + the MoveStartFrame rename). Build Development Editor: **Succeeded**.
- Test suite not run (laptop runs GSTN tests on the same commit, per brief).

## Capture (Play, listen host + 2 clients)
- Run 1: `pawns not ready after 180 s`. The hub watcher ran play setup 4 s after PIE, when world 0 had no pawn yet
  (`RuntimeError('world 0 has no player pawn')`), and did not retry. A slow editor under load caused it; a plain re-run passed.
- Run 2: knockdown_getup OK. back_hitstand / back_knockdown **missed**: the victim stood 326/320 cm away. The behind walk
  used 0.3 s of wall time, and under load it covered about 155 cm.
- Fix (in this commit): the behind walk now releases the stick once the pawn is turned away (within 15 deg) and >= `BEHIND_STOP` (160 cm,
  env `PIE_CAPTURE_BEHIND_STOP`), with a 0.9 s fallback. The front sequences are unchanged.
- Run 3 (back_* only): both hit, at 183 and 194 cm.
- Output: `docs/notes/reviews/react-hook-4/` (3 mp4, 6 sheets, frames.jsonl, capture.log, README with data facts).

## For the rule-10 cloud review (from the data, not a verdict)
- back_hitstand: yaw stays 0 the whole time, so the pawn **never turns** back along the knockback.
- back_knockdown: it turns 0 -> 180 between v30 and v34, mid-tumble, in about 4 frames.

## State left
- _ue59 detached at 1b641dca + this commit, built; Unreal closed; PM_BoostJump / PM_ThrowRider reverted; no maps saved.
- Logs: `_ue59/Saved/Logs/rh4/` (build3.log, capture_try1/2.log).

Cost: about 90k tokens (Opus), medium.
