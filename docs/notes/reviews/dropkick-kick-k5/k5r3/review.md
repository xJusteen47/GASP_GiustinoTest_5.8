# Dropkick K5, pose round 3 of 3: review (cloud, 2026-11-21)

Pack: docs/notes/reviews/dropkick-kick-k5/k5r3/ on claude/project-thread-cqad05 82d09c64.
Criteria: k5r2b/review.md "Round 3" (di23qw 7213d9c2), curves sheet "Round 3", owner's words 11-21 (not enough OOMPH, arms not
crossed, "compiaciuto di se stesso" while falling in that pose).
ASSERT: OK on all four runs (assert_lines.txt; see Technical for the two "empty spec" lines).

## VERDICT: PASS (posa). Goes to the owner.
The three identity traits read on both side cameras, on whiff, hit and high press. What is left of the owner's OOMPH note is not
pose: it is a montage change (owner's call) and hit feedback (owner's hitboxes).

Images looked at: 8 of 8. Pack: tiles whiff az90, tiles whiff az315, closeup_loop. Built from the pack mp4s (committed here):
review_life_az90.jpg (base over K5, a0-a19), review_strike_frames.jpg (whiff a11-a20, az90 and az315, every frame),
review_high_fall.jpg (high press a16-a35, every frame), review_mixed.jpg (hit az90 a13-a17, game view, upright fold a3-a11),
review_hit_az315.jpg (hit a12-a19, every frame). Frame numbers are the labels burned into the mp4s.

## 1. Identity traits (checked first)
| trait | verdict | label | what the frames show |
|---|---|---|---|
| legs together and straight | PASS | posa | a15 to landing: knees locked, feet together, toes forward, on az90 and az315, whiff, hit and high. a14 is the one smear frame with the knees half open. |
| arms crossed, upright (a4-a13) | PASS | posa | Unchanged from round 2. Re-checked on az315: a5 and a7 forearms stacked level in front of the chest, left over right; a9-a13 behind the knees in the tuck. Note, not blocking: a0-a3 the arms are still apart (two fists forward at a3), the fold is complete at a4-a5. |
| arms crossed, lying back (a14 to landing) | PASS | posa | Forearms lie on the chest, elbows low at the sides. On az90 the whole block is below the top of the head on every frame a14-a18 (base and round 2: tent above the head). The highest point is the fist resting on the other arm, about one fist above the chest line, in line with the 11.3 cm in the README (I cannot measure cm from video). On az315 the head is fully visible at 250 cm (a14-a18) and at 160 cm on the high press (a16-a28): the near upper arm passes under the chin, never in front of the face. No frame with anything above the head while K5 plays. |
| smug in the fall | PASS | posa | Head lifted off the body line, chin up, turned to his right, looking down the body toward the feet and target, not at the sky (base: head in line with the torso, hidden behind the tent). With the arms flat this reads at 250 cm as a man lounging on his own kick. Held from a15 to landing: a16-a28 on the high press are the same attitude on every frame, no pop at the loop wrap a27 -> a28. Limits: the mannequin has no face, so the attitude lives only in the head angle and the fold; the +3 deg at loop f6 is invisible (by design). The owner is the final judge of "compiaciuto". |

## 2. Life (video frames, before numbers)
| item | verdict | label | notes |
|---|---|---|---|
| anticipation | PASS | posa | a8-a13: knees to the chest, torso leaning back, a compact ball that holds through a13. Tighter and later than base. |
| arc and snap | PASS | posa | a13 ball, a14 half open, a15 locked: full extension in 2 frames. Base opens over a12-a15. This is the part of OOMPH that pose can give, and it is there. Not regressed from round 2. |
| accent at the lock, follow-through | weak, not blocking | dati montage | a15 = a16 = a17 = a18. Nothing overshoots or settles, so a whiff has no accent of its own. Not done on purpose (README): startup 16 f and loop 12 f are gameplay timing, an overshoot inside the loop would pulse every 0.2 s. Needs a one-shot segment after the strike = montage change = owner's call. |
| fall | PASS | posa | A held statue, as the criteria allow. No jitter, no pop at the wrap. |
| weight, root motion vs capsule | PASS by eye | posa / gioco | Travel is continuous a11-a18 on both cameras, the body pitches back with the pelvis leading. Hit feedback not judged: no hitstop, see below. |

## 3. Technical
- Timing matches the sheet: lock at a15 (v153), loop 12 f. Hit and whiff poses identical a13-a17 on az90 and az315.
- Rule 10, 15% limit: the fix touched arms and head only. Leg arc, lock frame and travel a13 -> a15 are the same as round 2. No reduction.
- Rule 11 re-check: offline assert windows against the burned labels. Whiff f140-f157 = a2-a19, and a19 is the last air frame
  (a20 is on the ground). High f131-f157 = a2-a28, and a28 is the last air frame. Both consistent.
- **cattura**: hit and high print "ASSERT: FAIL no expectations (empty spec)" on the online line; the offline lines are OK and I
  accept them. Next packs: run every capture with a spec so no FAIL line sits in the pack.
- **gioco / landing clip, not counted as a pose round**: whiff from 480 cm, a19 (last air frame): both forearms spring to vertical,
  fists above the head on az90 and in front of the face on az315, and the torso pitch jumps. Owner rule 11-17: crossed until impact.
  New for the code queue: on the high press the last air frame (a28) keeps the arms crossed, and on the hit run a19 is still
  airborne with the arms crossed. The spring appears only when the landing comes in the first frames of the loop, so it is the
  blend into the landing clip, not a K5 key.
- **gioco**: no hitstop, no visible reaction I can judge (owner sets the hitboxes).
- **cattura**: on the hit run the dummy sits at the frame edge on both side cameras and leaves the frame at a15, so the contact
  point cannot be judged from this pack. Not blocking, the poses equal the whiff. Next hit capture: frame attacker and dummy together.
- **cattura**: game view shows the character 15-25 px tall. Nothing can be judged there; the owner judges the play camera in PIE.
- The three capture requests of round 2 are done: labels burned in, same a-20 offset, close-up, assert lines in the pack. Thank you.

## 4. For the owner (rule 13d, collected by the coordinator)
1. Approve K5 round 3 as the dropkick kick? If yes: tag approved/dropkick-k5r3 and a line in docs/notes/anim-approvate.md.
2. One-shot overshoot segment after the strike (montage change, changes nothing in the gameplay timing if it replaces the first
   loop frames): yes or no. Recommended yes, it is the missing accent on a whiff.
3. Hitstop and dummy reaction on hit come with his hitboxes.

## Riassunto per l'owner
- Si legge bene: gambe unite e dritte, calcio che scatta da raccolto a teso in 2 frame (a13 -> a15), braccia conserte sia in piedi
  sia sdraiato (basse sul petto, gomiti ai lati, testa sempre visibile), e in caduta resta sdraiato con le braccia conserte e la
  testa sollevata che guarda il bersaglio: compiaciuto.
- Non si legge ancora: a vuoto il colpo non ha un accento suo al blocco (manca l'overshoot, serve un pezzo di montage: decisione
  tua); a segno mancano hitstop e reazione del manichino (hitbox tue); premendo da 480 cm, un frame prima di terra le braccia
  scattano in alto (clip di atterraggio, coda codice).
- Da guardare: ab_base_vs_k5r3_whiff_slow4x.mp4 (base a sinistra, K5 a destra, lato e tre quarti), poi
  k5_dropkick_sweet_whiff_az90_realspeed.mp4 (lo scatto a velocità vera) e k5high_dropkick_sweet_whiff_az315_slow4x.mp4 (la caduta lunga).
