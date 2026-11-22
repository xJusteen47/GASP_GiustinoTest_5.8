# react-hook-4 rule-10 review (cloud): handover

Date 2026-11-14, about 20:40-21:00Z. Branch `claude/project-thread-7bhwol`, captures at `06481cb2` (code `1b641dca`).
Full review: `docs/notes/reviews/react-hook-4/review.md`, evidence `review/review_az90.jpg`, `review/review_az45.jpg`.

## Verdicts
- **knockdown_getup (front): PASS.** Landing pop fixed (v85 Tumble, v86 GroundHit at the same height, v87 flat; pelvis moves < 10 px,
  was about 110). Reactions no longer show one frame late. Ready for the owner.
- **back_knockdown: FAIL (code).** Hitstop v24-v30 shows the Tumble pose with the head toward the attacker (yaw 0); the turn lands at v31
  and is on screen in 2 frames (not 4), as a flat propeller spin at launch. After the get-up (free v139) the pawn turns back to yaw 0
  by itself by v170, so it ends with its back to the attacker.
- **back_hitstand: FAIL.** No turn: the hit has no pushback and the turn only runs with a horizontal reaction speed
  (`GSTN_CombatSimVictim.cpp:600`). The front recoil clip then leans toward the attacker. Turn in hitstand still NOT verified.

## Next code thread (small)
1. Turn on the hit frame (hitstop start) instead of at the launch.
2. Make the facing hold after the reaction (my guess: Mover goes back to the facing target of the last walk); capture 60 frames past the get-up.
3. Capture back_hitstand with a hit that pushes; add the automation test on `bTurnBackAlongKnockback`.

## Owner decision needed (move feel)
- Hit from behind with no pushback: (a) turn anyway on the hit frame, behind a rules flag (recommended), or (b) keep the facing and wait
  for a back-hit clip.

## Unchanged, animation job
- GroundHit: impact pose lasts one frame, 90 cm above the floor; no visible rebound. GetUp: first 3 frames soft (v137-v139).
- Crouch after the hitstand v62-v85; smaller hunch after the get-up v165-v195 (react-hook-3 called this upright: it is not).

## Decisions taken without the owner
- Front knockdown passed although get-up and bounce break the feel rule: both are known clip limits already queued, not this build's code.

Cost: about 110k tokens of context (Fable 5.2), roughly 7-9 USD. No subagents, 12 image views.
