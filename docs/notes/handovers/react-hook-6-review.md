# Handover: react-hook-6 rule-10 review (back hits after the hit-turn fix), 2026-11-14

Branch `claude/project-thread-a5idak`, captures `9d92bfec` (code `08338b8d`). Review: `docs/notes/reviews/react-hook-6/review.md`,
evidence in `docs/notes/reviews/react-hook-6/review/`. Base: react-hook-4 review on `claude/project-thread-7bhwol` `31d2f686`.

## Verdict
- **back_hitstand: PASS.** v36 (first Hitstun frame) already turned to the attacker, recoil leans away from them, held to v71,
  facing kept to v142 (70 frames after the stun).
- **back_knockdown: PASS, one note.** v28 (first Tumble frame) is mid-turn, v29 fully turned; right way round through the hitstop;
  no flip at launch; landing same as react-hook-4 (no pop); get-up in place; yaw 180 from v144 to v214.
- Both can go to the owner as the temporary fix.

## Notes, not blocking (details in review.md)
1. Knockdown: one in-between frame at the snap (mesh about 90 degrees short on the hit frame). Guess: mesh interpolation between
   simulation steps. Fix only if a literal one-frame turn is wanted.
2. After the snap the pose is no longer fully frozen: feet creep about 12 cm in the first 20 hitstun frames, body sags about 5 cm
   in the 7 hitstop frames. Guess: Anim BP leg IK or root offset. Put with the queued Anim BP crouch and drift fix.
3. Not captured: back hit WITH pushback in hitstand. HitFacing automation tests exist, not run here.
4. `[HitTurn]` debug logs still in the code.
5. Capture script must reset the victim facing between sequences (combined run missed the second hit).
6. Animation limits unchanged: GroundHit impact pose one frame, no visible bounce, get-up soft for 3 frames, exit crouch.

## Decisions taken and next
- Reviewer chose PASS for the in-between frame (clear change on frame 1, turn done one frame later; react-hook-4 had 7 wrong frames).
- 5 composites looked at instead of 2: the first-frame check needed native resolution.
- Coordinator: send the two mp4 to the owner. Optional small code thread for notes 1, 3, 4, 5 (Opus medium, about $5-10).

## Cost
One cloud thread, Fable 5.2, about 130k tokens of context at the end, 6 images viewed. Estimate $7-10.
