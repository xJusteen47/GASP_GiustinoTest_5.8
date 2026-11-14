# REACT-HOOK-1 REVIEW handover (rule 10, cloud, 2026-11-14)
Branch `claude/project-thread-fy7agl` = `claude/project-thread-2n1mrc` `004489fb` + this review. Docs only, no code, desktop not used.
Full review: `docs/notes/reviews/react-hook-1/review.md`, evidence grids in `docs/notes/reviews/react-hook-1/review/`.
**Verdict: FAIL, nothing goes to the owner yet.** Frames are video frames (60 fps).
- hitstand FAIL: snap in v23-v24 is right, pose fades v25-v29, plain idle v30-v59 while still in Hitstun.
- tumble: snap in v23-v25 right; read not judgeable (victim hit from behind, behind view blocked). No standalone tumble/splat capture.
- knockdown FAIL: 85 cm one-frame drop v61-v62 (known); bounce about 44 cm and about 34 frames in the air (v64 to v95-v98) with the
  rigid LieUp pose floating from v76. GroundHit to LieUp join v75-v76 is FIXED (react-v3 pop 1 gone).
- get-up with A: behaviour PASS (v111 press, v112-v137, in place, no turn); seen whole in neither view; crouch v144-v170 still there.
- roll FAIL: starts v84 at the PEAK of the bounce (Knockdown frame 23), in the air v86-v92. Facing at the end is right.
- roll back FAIL: starts v85 in the air; turn begins inside the roll v101-v104; slides about 1 m v107-v155 without stepping.
**Known issues:** 1 confirmed. 2 confirmed, but the trigger is a fixed frame at the bounce peak, not the bounce end; the "late stick
gives no roll" case is not in the committed files. 3 facing and slide confirmed on video; 129/110 cm not checkable on video (taken
from the sim log). 4 confirmed and worse: the behind view is blocked by another pawn for hitstand, tumble and get-up.
**New:** the attacker stands BEHIND the victim, so HitStand leans toward the attacker and Tumble flies feet first.
**Next, code thread (Opus):**
1. HitStand held for the whole Hitstun, blend out only at its end.
2. Roll/stand choice only after the bounce has landed, then on any frame while lying.
3. Get-up abilities ignore move input, lock facing, zero horizontal velocity at start and end; then re-measure 80/65 cm.
4. Bounce about 15 cm and 12-14 frames; LieUp never before the second floor contact.
5. Capture script: attacker in front, other pawns hidden from the cameras, camera tracks the pawn, add standalone tumble and splat.
**Next, animation:** GroundHit from Tumble height (or ECB); first frame of GetUp/Roll/RollBack with a clear change; ABP crouch.
**Owner decision (move design):** a hit from behind. Recommended: the game turns the victim back-first along the launch, as a
setting that can be switched off; no second clip set.
**Choices made here:** capture README numbers from the sim log accepted without re-measuring; 8 evidence grids committed (1.3 MB).
**Cost:** one cloud thread (Fable), about 130k tokens of context, estimate $8.
