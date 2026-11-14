# REACT-HOOK-3 REVIEW handover: rule-10 cloud review of the reaction captures (2026-11-14)
Branch `claude/project-thread-f2j5fq` = `claude/project-thread-wkds3r` d27c77f5 + this review (docs only, no code, no desktop).
Review: `docs/notes/reviews/react-hook-3/review.md`; evidence in `review/` (one composite per view + two crops of the landing pop).

**Verdicts (video frames, 60 fps)**
- PASS: hitstand held v22-v78; restart on the 2nd hit v41 invisible (acceptable); tumble snap v23-v24; GroundHit to LieUp v98-v99;
  roll v107-v129 (79.4 cm, from the floor); roll late (KD+52); roll back v107-v130 (RollBack clip, 65 cm toward the head, ends
  facing the attacker); A = stand in place v135-v160, no crouch after.
- FAIL (code): at KD+0 (v84, v83 in roll_late) the body jumps up about 85 cm for one frame in the Tumble pose. All four sequences.
  Cause: `UGSTN_ReactionAnimComponent` applies the entry drop on the tick the montage starts, the pose arrives one tick later.
- NOT VERIFIED: back hit turns the victim back first. Only front hits were captured; no test names `bTurnBackAlongKnockback`.
- Feel, wait for the anim job: get-up lies still v135-v137 (3 soft frames); the bounce is small (+18 cm, v89-v95) but shows no
  rebound, because the capsule rises while the mesh still comes down.
- Known and visible: deep crouch after the hitstand v80-v104 (Anim BP).

**Decisions taken (rule 6)**
- RollBack 65 cm vs Roll 80 cm judged acceptable; left as data for the owner to feel in the playtest.
- The set is not passed to the owner; the rolls alone could be.

**Next (one small code thread, Opus medium)**
1. Entry drop one tick later, or evaluate the pose on the tick the montage starts (also removes the one-frame delay of every reaction).
2. Capture with the attacker behind (hitstand + knockdown) and a test on `bTurnBackAlongKnockback`.
3. Hitstand exit with the get-ups' held blend-out, to hide the crouch.
Then recapture the landing and the back hit only, and a short review of those frames.
Animation queue unchanged: GroundHit from Tumble height (then judge the bounce again), GetUp first frame.

**Cost:** one cloud thread (Fable), about 130k tokens of context, 9 grids looked at once each; estimate $9.
