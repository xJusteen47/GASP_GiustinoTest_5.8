# ANIM-FINISH-4 rule-10 review handover: jab v3 r1 and down-air v9 blend-out (cloud, 2026-11-14)
Branch `claude/project-thread-anim4x`. Reviews: `docs/notes/reviews/overlord-jab-v3-r1/review.md`,
`docs/notes/reviews/overlord-dair-v9-blendout/review.md` (Italian), grids `rv_*.jpg` beside them. No code, no asset touched.
**Verdict: jab 1 FAIL, jab 2 FAIL, both down-air blend-out variants FAIL. Nothing goes to the owner.**
- **Blocking, game side, new on a plain jab**: after every variant sequence the character drops into a deep squat and needs about 35
  frames to stand (jab 1 alone: one frame, v45, 2743 px, more than the hit frame; chains: 4 frames from v80 / v84). Base never does it.
  It starts when the montage begins blending out, 4 frames before the timeline ends: the game pose under the montage is already the
  squat. Same family as the crouch after reactions. Changed between base and variant: clips, blend-in 2 f to 0, jab 2 root motion off.
- **Jab 1** passes the first-frame rule (v21: 1435 px side, 1516 behind; no dead frame) and the arm now shows from behind. Fails:
  the hit does not connect to the eye (in hitstop the hand is right of the dummy, which reacts to the left; gap from the side).
- **Jab 2** passes: step in the pose, capsule still, feet down, no magnet, 2-frame strike, contact on the dummy's chest from the side.
  Fails: (1) first frame equals the jab 1 hold (36 px; base had 2306), then `a` shows 2.1 on v34 AND v35 = one dead frame, in both
  variant captures; (2) the lunge is held until a40 while FAF is 26: body 28 cm ahead of the capsule at FAF (deduced, no capture acts at
  FAF); (3) from behind the striking arm is hidden by the torso at contact.
- **Down-air**: confirmed from frames that the leg jump is game side. It follows the timeline end in all three runs; in attempt 2 the
  montage had left 16 frames earlier, arms already in the fall pose, and only the legs jump on the first frame without timeline. While
  an air timeline runs, the game's legs are not the fall legs. Keep the montage at 8 f / -1.
- **Fix list (details in the two review.md)**: (a) code/ABP thread first: two jab 1 captures to isolate the squat (r1 clip with blend-in
  2 f; r0 clip with blend-in 0), the jab 2 montage stall on chain start, and for the down-air a per-frame log of Mover mode, the ABP's
  in-air variables, combat tags and slot weight around m0:42, then make the pose under an air timeline the fall pose (or blend that
  switch over 8-10 frames); target no frame above 2400 px side in v69-v85. (b) pose round 2: jab 1 diagonal about 25 deg (hand within
  25 cm of centre, 75 cm forward, must overlap the dummy from behind in hitstop), left arm out of the way; jab 2 f0 = today's f1 pose,
  step back finished by f26, contact hand 15-20 cm further right; add a capture sequence that acts at FAF.
- **Owner decision still open**: jab 1 angle (25 deg proposed). Possibly also: first-frame rule versus blend-in if the squat follows
  blend-in 0.
- **Not established**: cause of the squat and which state flips at air ability end (no editor opened; the Anim BP is binary). The
  metric is my own mask, compare only inside this review.
- **Method that worked**: label strips of every frame in two sheets first (gives v to m/a mapping, hitstop frames and montage exit for
  free), then grids of at most 9 cells of 220 px per row; a 14-cell row is downscaled and loses the arms.
- **Cost**: one Fable thread, about 150k tokens of context, 9 images viewed; estimate $10-15.
