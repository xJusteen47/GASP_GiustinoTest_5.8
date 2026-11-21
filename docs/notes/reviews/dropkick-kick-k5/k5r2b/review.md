# Dropkick K5 (kick): rule-10 review of pack k5r2b, pose round 2 of 3 (2026-11-21, cloud, Fable high)

**Verdict: FAIL (posa). Does not go to the owner.** Upright the arms now read as crossed. Lying back (the strike pose and the
whole fall) they read as a raised guard, and because of that the fall does not read as smug.

Read: this folder, ../k5b/review.md, overlord_dropkick_k5_curves.md, branch uvzm3k 7f70af2b. Looked at 9 images (limit 8, one
lost to a frame offset, see 3): pack tiles az90 and az315, pack grid_k5high_fall, and my 480 px zooms taken from the slow4x mp4s,
saved here: `review_fold_az90_a3-a13.jpg`, `review_kick_az90_a13-a21.jpg`, `review_az315_a4-a18.jpg`,
`review_fall_loop_a16-a28.jpg`, plus one hit / game-view sheet (not saved).

## 1. Identity traits (first)
| trait | verdict | label | what I saw |
|---|---|---|---|
| legs together and straight from f15 | PASS | - | a15-a28, az90 and az315, whiff and hit: one plank, feet together, toes pointed, never opens during the fall. |
| arms crossed, upright and tuck (a4-a13) | PASS | - | az90 a4-a6: a compact level stack in front of the chest, no upright fist, no fist ahead of the elbows. a7-a13: the stack sits between chest and knees and reads as a fold. Clearly better than round 1. It floats about 10-15 cm ahead of the chest (estimate): fine here, but it is the same offset that breaks the next row. |
| arms crossed, lying back (a14 to landing) | **FAIL** | posa | az90 a14-a18 and all of the loop: the top arm's upper arm stands almost square to the torso, the elbow is the apex of a tent about 20-25 cm above the chest (estimate from head size), HIGHER than the top of the head. The silhouette is a tower over the chest, the same read as base a15 (forearms up in front of the face). az315 a15-a18: that elbow points at the sky and hides the head. Answer to the animator's question: it reads as a raised guard, not as crossed arms. Take the alternative: elbows lower, at the sides. |
| a3 | note | dati montage (minor) | The fold lands at a4. At a3 the forearms still point forward with one fist up (one frame). Not blocking: either fix the retime or change the sheet from "f3" to "f4". |
| smug while falling | **FAIL** | posa | Now captured (loop a16-a27, landing a29-a30). a16 = a20 = a24 = a28 to the eye: a statue. Stillness itself is fine (a rigid plank with folded arms is arrogant). What fails: the arm tent reads tense and defensive and covers the face from the quarter view; the head lies in line with the body looking at the sky; the 6 -> 12 deg tilt and +-1 deg sine of the sheet are invisible at 250 cm. Nothing in the fall says "pleased with himself". |

## 2. Life
| item | verdict | label | what I saw |
|---|---|---|---|
| chamber a7-a13 | PASS | - | Unchanged: a real ball held to a13. |
| snap a13 -> a15 | PASS, not regressed | - | a13 ball, a14 legs 2/3 out, a15 locked. Character width on az90 226 -> 294 -> 426 px (frames 33-35 of the whiff mp4). |
| accent at the lock | weak | posa (minor) | a15 = a16 = a17, no overshoot (by choice). The owner's note is "not enough OOMPH": on a whiff the lock still has no accent of its own. Since round 3 is needed anyway, I recommend a visible overshoot at a15 settling in 2-3 frames. Not blocking. |
| arms at a19 (sweet whiff) | note | dati montage, other clip | One frame before the ground both forearms spring straight up with the fists 30+ cm off the chest. Owner rule 11-17: arms stay crossed until impact. This is the blend into the landing clip, outside K5: for the code/landing queue, not counted as a pose round. |
| root motion vs capsule | not judged | gioco | No hitstop, dummy does not react (known, owner sets the hitboxes). |

## 3. Technical
- **cattura, fixed**: dummy 40 cm further, at a15 the feet land on his hip, nothing skewers him. Side and quarter mp4s at 250 cm and
  480 px frames are enough to judge the arms. Thank you, keep this format.
- **cattura**: the k5high mp4s start at a-10, the sweet ones at a-20 (lock found at frame 25 vs 35), and no label is burned in. It
  cost one wasted image. Next pack: same offset or the a-number burned into the mp4.
- **cattura**: game view at 700 cm shows the character about 25 px tall: the fall cannot be judged there. Add a head-and-arms
  close-up of two loop frames.
- **cattura**: ASSERT OK is not in the pack. It is cited in docs/notes/handovers/dropkick-k5-2.md lines 14-15 (assert.txt and
  assert_offline.json under Saved/DropkickK5), not re-checkable from the cloud. Next pack: copy the assert line into the folder.
- Timing matches the sheet (startup 16 f, lock a15, loop 12 f, one cycle before landing). Hit and whiff poses identical.
- **gioco**: dummy never reacts, no hitstop (known).

## Round 3 (laptop, last pose round, then the owner decides)
Correction to round 1: I wrote "lift the elbows forward to mid-chest height, clearance by moving the elbows forward". Upright that
gave a readable fold, lying back it built the tent. The target was too coarse. This time:
1. Lying phase (a14 to landing, startup tail and loop): upper arms down along the ribs, elbows OUT to the sides and only slightly
   forward, forearms resting ON the chest, stacked left over right, hands tucked on the opposite arm. Acceptance on az90: the whole
   arm block stays below the top of the head and within about 10-12 cm of the sternum. On az315: the head is fully visible.
2. Build the fold relative to the chest (contact), so it keeps its shape while the torso pitches 12 -> 82 deg. a4-a13 may come
   5-8 cm closer too, but they pass as they are.
3. Clearance: sideways (wider elbows). Not forward (round 2), not by pushing the hands (round 1). If the check cannot pass with the
   forearms touching the chest, stop and report the margin; do not lift anything to satisfy it (rule 13e in spirit).
4. Smug: with the arms flat the head becomes visible. Give it a HELD attitude that reads at 250 cm: head turned or tilted at
   least 15 deg, chin up but looking down along the body at the target, not at the sky. The body can stay a statue.
5. Recommended: visible overshoot at a15-a17 (owner's OOMPH note).
6. Recapture as this pack, with the three capture fixes above.
