# Back-hit turn (thread "Turn victim on back hits"): handover, 2026-11-14 ~22:00Z

Branch `claude/project-thread-a5idak` (from 7bhwol 31d2f686). Owner ask: turn the victim to face the attacker on the FIRST reaction frame
of a back hit (hitstand and knockdown), keep the facing after the get-up; temporary until back-hit/launch clips exist.

## Code (built in _ue59, tests GSTNCombat.NP.Hits 77/77, Timeline 10/10)
- 8952dd93 `FGSTN_CombatVictim::ResolveHitFacing` + victim step queues `SetFacing` on the hit frame. Rules: existing
  `bTurnBackAlongKnockback`, new `bTurnToAttackerWithoutKnockback` (default on) + `TurnToAttackerMinAngleDeg` (90 = back hits only).
  Input side `bHoldFacingAfterReaction`: neutral stick keeps the facing after a reaction/get-up. Tests `GSTNCombat.NP.Hits.Facing.*`.
- 070b562a test cast (5.9 double). ebe6ab06 `[HitTurn]` diagnostic logs (Log level: REMOVE or make Verbose when fixed).
- aa698b45 ProduceInput holds the orientation intent at the latest frame's SetFacing yaw (hypothesis: walking mode undid it). No effect.

## Result in game (react-hook-5 captures, 9c4482ec, docs/notes/reviews/react-hook-5/)
- WORKS: after the get-up the knockdown victim keeps facing the attacker to the end (react-hook-4 drifted back to 0).
- FAILS (2 attempts used, stopped per rule 5): no turn on the hit frame. Logs prove the decision and the pre-tick
  `SetFacing -180` run on the hit frame (effect=1, requests 3/8). Yet the host actor yaw stays 0: hitstand never turns,
  knockdown turns only on the first frame it MOVES (launch-time turn in ExecuteReactionMove, 8 frames late).
- So an instant `FGSTN_SetFacingEffect` queued while the pawn is at rest / entering hitstop is lost or overwritten. Steering
  uses the same effect inside moves and works.

## Next (one small Opus thread, desktop)
1. Run one back_hitstand capture with `log LogMover Verbose`: is the SetFacing instant effect queued/applied on F+1, and what
   orientation does the sync state have after the tick? Check the FreezeAtRest GE / freeze layered move and the walking mode's
   orientation (ChaosMover? GASP mode) at zero velocity; check whether the capture's `yaw` (actor rotation) lags the sync state.
2. Likely fix shapes: apply the turn from inside the freeze layered move (it owns the frame: set orientation in its GenerateMove /
   first tick), or queue the facing as part of the freeze effect itself instead of a separate instant effect.
3. Then remove the `[HitTurn]` logs and capture back_hitstand + back_knockdown for the rule-10 review.

Cost: this thread ~150k tokens Opus (~$8-10) + 3 desktop runs ~150k (~$8). Total ≈ $16-18.
