# react-hook-1 visual review (rule 10, cloud): reactions driven by the game, 2026-11-14

Material: `docs/notes/reviews/react-hook-1/<seq>.mp4` on `claude/project-thread-2n1mrc` `004489fb` (Play in L_Test_Hub, client 1 hits
the host). Every video frame of the four sequences extracted with ffmpeg; the state of every frame read from the burned-in label;
one composite grid per view and sequence (knockdown in halves), consecutive frames around the hit and every state change.
Frame numbers are VIDEO frames (`v`, 60 fps). Evidence grids in `review/`. Compared with `react-v3/review.md` (clips in place).

**Result: nothing here is ready for the owner.** The clips are the ones that passed react-v3; what fails now is how the game
plays them, and the capture itself hides half of it.

| Reaction | Verdict | What the video shows (frames) |
|---|---|---|
| hitstand | **FAIL** | Snap in is right: v23 idle, v24 full HitStand pose (1 frame, strong change). But the pose is at full strength only on v24, fades v25-v29 and from v30 the pawn stands in plain idle until v59 while the label still says Hitstun (Hitstun:7 to :36). 6 frames of reaction, 30 frames of idle while stunned. Known issue 1 confirmed. Evidence `hs_az90.jpg`. |
| tumble (inside the knockdown capture) | **PASS on the snap, not judgeable as a read** | v23 idle, v24 already horizontal (transitional, knees tucked), v25 full Tumble pose, held to v60. First-frame rule respected. But the victim is hit FROM BEHIND (see finding A), so it flies feet first on its back, and the behind view is blocked until v60. No standalone tumble or splat capture exists (handover step 5 asked for six sequences, four were made). |
| knockdown (ground hit, bounce, lie) | **FAIL** | (1) v61 to v62: body drops about 85 cm in one frame, Tumble height to floor (known, same as react-v3 pop 2, declared not done). (2) GroundHit to LieUp join v75 to v76: **fixed**, no game pose frame, no visible pop (react-v3 pop 1 is gone). (3) The bounce is not small: the body leaves the floor at v64, rises about 44 cm until v75-v78, hangs there to about v85 and is back on the floor only around v95-v98. That is about 34 frames in the air, and from v76 the pawn is the rigid LieUp pose floating above its shadow: reads as levitation, not as "small bounce, then lie". Evidence `kd_az90_A2.jpg`, `kd_az180_A.jpg`. |
| get-up with A | **Behaviour PASS, capture FAIL** | Press at v111, get-up starts v112, standing by v137, in place, no turn: matches "A = stand in place". First visible motion only at v114-v115 (2-3 soft frames, weak against the first-frame rule). Hunched crouch after it is still there, v144 to about v170. The side view cuts the pawn at the frame edge from v122; the behind view is covered by another pawn's mesh v112-v131. So the get-up itself was seen whole in neither view. Evidence `kd_az90_B.jpg`, `kd_az180_B.jpg`. |
| roll (stick toward feet) | **FAIL** | Starts v84 (Knockdown:23) with the body at the top of the bounce: legs up v86-v91 with the floor marker clearly below the body, lands in the squat about v93-v95. Known issue 2 confirmed (starts in the air). Ends standing v107 with the right facing; crouch v110-v126. Side view: pawn at the frame edge and cut from v94, so travel and slide cannot be checked on video. Evidence `roll_az90.jpg`. |
| roll back (stick toward head) | **FAIL** | Starts v85, also in the air (v86-v92). Lands on hands and knees v95-v99, rises v100-v103. The turn starts DURING the last frames of the roll (v101-v104), 90 degrees by v107, facing the opposite way by v150. From v107 to about v155 the pawn keeps travelling about 1 m on screen in the crouch pose without stepping. Known issue 3 confirmed for the facing and the slide. Evidence `rollback_az90.jpg`, `rollback_az180.jpg`. |

## Known issues from the build thread

1. **Confirmed.** HitStand visible v24-v29, idle v30-v59 during Hitstun.
2. **Confirmed that the rolls start in the air; the "only at the end of the bounce" part needs a correction.** Both rolls start at
   Knockdown frame 23, which is the PEAK of the bounce, not its end. It looks like a fixed time gate, not a landing check. The
   attempt where a late stick gave no roll is not in the committed files, so that half could not be checked.
3. **Facing and slide confirmed on video (roll back). The 129/110 cm numbers cannot be checked on video** (camera pans, pawn at the
   edge); they come from the sim log and I take them as given. The turn begins before the roll ends, so move input is live during
   the get-up abilities; that also explains the extra travel.
4. **Confirmed, and the behind view is worse than the README says.** Side view: legs out of frame from v40, pawn cut at the right
   edge for the whole landing, get-up and forward roll. Behind view: a second pawn stands between camera and victim for the whole
   hitstand sequence and for the tumble (v20-v60), and the camera sits inside a pawn mesh during the get-up (v112-v131).

## New findings

- **A. The victim is hit from behind.** It is launched the way it faces (side view: faces right, flies right; behind view: flies
  away from the camera). HitStand therefore leans back toward the attacker and Tumble flies feet first, both the reverse of what
  the clips were made for. First a capture mistake (attacker must stand in front). It also opens a design question for the owner:
  what a hit from behind should look like. Recommended: at launch the game turns the victim so its back points along the launch
  (as in Smash), with a setting to switch it off; no second set of clips.
- **B. Bounce reads as levitation** (table, knockdown point 3).
- **C. Soft first frames** on GetUp (v112-v114), Roll (v84-v85) and RollBack (v85-v86): 2-3 frames before anything visibly moves.
- **D. Crouch after get-up and rolls is still visible** for about 25-30 frames; the 20-frame hold did not remove it.

## Fix list

Code:
1. HitStand holds for the whole Hitstun (keep the pose while the tag is on, blend out 4-6 frames only when it ends). Today the
   one-frame clip's montage ends at once.
2. Get-up choice (roll or stand) only after the bounce has landed, and then on any frame while lying, by polling the stick.
3. During GetUp, Roll and RollBack: ignore move input, lock facing, zero horizontal velocity at start and end, so travel is only
   the travel keys. Then re-measure against 80/65 cm before touching the keys.
4. Bounce: about 15 cm and 12-14 frames in total (the length of GroundHit), LieUp never before the second floor contact.
5. Facing at launch (finding A): owner decision, recommended as a setting.
6. Capture script: attacker in front of the victim, other pawns hidden from the capture cameras, camera tracks the pawn (dead zone
   40 cm or less) with the whole body in frame, plus standalone tumble and splat (splat from the attacker's side).

Animation:
1. GroundHit variant that starts at Tumble height, or the shrinking capsule (ECB): removes the 85 cm drop at v61-v62.
2. First clip frame of GetUp, Roll, RollBack must already show a clear change (head and shoulders up, knees up).
3. Anim BP: crouch after the get-up (known, not in git).

## Summary

FAIL as a whole. PASS inside it: snap in of HitStand, Tumble and GroundHit (1 frame each), the GroundHit to LieUp join, A = stand
in place, forward roll facing. FAIL: hitstand duration, 85 cm landing drop, bounce size and length, rolls in the air, roll back
facing and slide, capture framing in both views. Next: one code thread for fixes 1-4 and 6, then a new capture and a new review.
Cost: one cloud thread (Fable), about 130k tokens of context, estimate $8.
