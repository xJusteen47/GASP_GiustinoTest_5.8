# Review Fable: air crouch (crouch-veloce-4, capture air_v10)

**Verdict: FAIL, on one item only: the landing with crouch held (v77-v81).** The air crouch itself now passes all three identity traits; both causes of the v7 FAIL are fixed on screen.

Seen: air_side.png (az90 v46-v76, plus enlarged crops of v49-v52, v53-v56, v70-v73), air_front34.png (az45, step 2), air_game.png, land_side.png (plus enlarged crop of v73-v85), review_fable_air_v7.md. Not seen: cr_air_v10.mp4. Frame counts are +/- 1 in air_side, +/- 3 in land_side (step 3). Sizes are my estimates from pixels, not measurements. The foot offsets quoted by the author (30-34 cm under the pelvis) are consistent with what I see but I did not re-measure them. The game-camera grid is too small (character about 25 px) to judge the pose.

## Identity traits

1. **Readable tuck: PASS** (v53-v70, az90 and az45). Both feet under the pelvis, knees up at about hip height, spine curled over the knees, silhouette clearly shorter than the fall pose at v50 (about 65-70% of its height). The knee asymmetry is kept, so it does not read as a symmetric ball. In az45 the front knee is no longer splayed.
2. **Guard arms like the ground crouch: PASS** (v54-v70). Both fists in front of the lower face, elbows out, lead fist slightly forward; same guard as the ground crouch at v85-v130 in land_side. Steady through the hold, no jitter.
3. **Quick enter with follow-through, no pop on release: PASS.**
   - Enter v51-v55: no longer a one-frame pop. v51-v52 chest comes upright and the arms come in, v53 legs are folded, v54-v55 the fists reach the guard after the knees and the feet sit slightly higher than in the loop, v56 settles. The overshoot is small (I read about 2 px, which matches the stated 4 cm) but the order knees first, fists second is readable.
   - Release v70-v73: v71 arms open with the legs still folded, v72 legs extend, v73 fall pose. Arms lead legs, no pop. Same as v7, still acceptable.

## FAIL item

1. **dati montage / gioco, land_side v77-v81: the character stands up on touchdown while crouch is held.** v76 is the air tuck with guard; v79 is a near-standing landing pose (figure about 1.6x the tuck height, fists dropped to waist height and pushed forward); v82 is the ground crouch with the guard back. So with the button held the read is fold, stand, fold in about 6 frames: a stretch where a landing must squash, and the guard breaks. This is v7 item 4, unchanged. It is the most common use of the move (hold crouch into the ground), so it counts.
   Fix, either of:
   - With crouch held at touchdown, skip the standing landing clip: blend air crouch loop to ground crouch directly in 4 frames (0.067 s).
   - Or play a crouched landing: pelvis dips 6-8 cm below the ground-crouch height at frame 2-3 after touchdown, back to the ground-crouch height by frame 8; head never rises above the ground-crouch head height + 5 cm; fists stay within 5 cm of the guard position for the whole landing (keep spine_03 and up on the crouch pose).
   Check: in a step-1 side capture of v76-v86, the head top must never be higher than at v85.

## Minor, not blocking

- **dati montage, v51-v52: two-frame leg stretch at the start of the enter.** At v51 the rear leg goes almost straight down and the front shin kicks forward of the knee, so the legs are longer than in the fall pose at v50 before they fold at v53. With the known mesh jump on the same frame it reads as a small snap. If cheap: make frame 0 of the enter clip match the fall pose legs (both knees bent about 90 deg) or use a 2-frame blend in, and keep the front shin at or behind the knee line in frames 1-2.

## Game items (gioco, known, not counted)

- v50 to v51 mesh jumps up with the capsule resize, drops back at v71-v72.
- Camera horizon shift on enter and release (air_game).

## Keep

Tuck pose v56-v70 (feet under the pelvis, knee asymmetry, spine curl), guard arms and head behind the fists, enter order (knees first, fists second) and its length (5 frames), small overshoot v53-v55, steady loop, release order (arms first, legs second, 2 frames), ground crouch after landing v82-v130, landing ring and dust timing.
