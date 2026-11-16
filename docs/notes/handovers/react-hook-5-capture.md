# react-hook-5 capture (desktop, _ue59): handover

Date 2026-11-14, 20:55-21:25Z, worktree `GASP_Giustino2_ue59`, UE 5.9. Three build+capture rounds on claude/project-thread-a5idak.

## Rounds
1. 8952dd93 (the fix): build failed at HitFacing tests C2666 (TestEqual double/float). `(float)` cast -> **070b562a**, pushed, built.
   Tests GSTNCombat.NP.Hits 77/77. Capture -> Saved/AnimReact/react-hook-5.
2. ebe6ab06 ([HitTurn] logs): built, capture -> react-hook-5b, log Saved/Logs/rh5/capture_diag.log. Not committed.
3. **aa698b45** (the orientation intent points at the hit turn): built (build4.log). Tests: Hits 77/77, Timeline 10/10, 0 fail. No test has
   "Steer" in its name. Capture -> react-hook-5c, which is the review material in `docs/notes/reviews/react-hook-5/` (overwritten).

## Data (identical in all three rounds)
- `[HitTurn]`: turn decided, and the pre-tick `SetFacing -180 actorYaw=0 effect=1` runs on the hit frame. No "no room" errors.
- back_hitstand: yaw 0 on every frame (no push, the pawn never moves). Never faces the attacker.
- back_knockdown: yaw 0 from hit to hit+6 while loc is unchanged (hitstop), then 180 on the first moved frame (hit+7). It holds 180
  through tumble, KD, get-up and to the end of the capture.
- So aa698b45 changed nothing visible: the yaw reaches the pawn only once it moves. Next suspect: the Mover sync/presentation state
  (or the actor rotation) is not updated while velocity is zero or during hitstop. Check where the capture's `yaw` comes from (actor
  rotation on the host) and whether Mover's MoveRecord or the SyncState's orientation gets the SetFacing.

## State left
- _ue59 detached at the pushed tip (aa698b45 + this commit), built. Unreal closed, PM_BoostJump / PM_ThrowRider reverted, no maps saved.
- Logs `Saved/Logs/rh5/` (build*.log, tests*.log, capture_diag.log, capture_c.log, grid*.log).

Cost: about 90k tokens (Opus) for the three rounds, medium.
