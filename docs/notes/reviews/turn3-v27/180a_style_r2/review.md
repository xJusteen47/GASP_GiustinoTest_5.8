# Review, run start turnaround 180 L, variant 28, POSE ROUND 2 (base 27)
Date 2026-11-21. Rules 10, 13, 15a. Evidence: `claude/runstart-generic-desktop` 41cbc8e9, `docs/notes/reviews/turn3-v27/180a_style_r2/`.
Capture: `assert_v28.txt` ASSERT: OK (f88-f132, weight >= 0.8 on 100 %, min 0.86). Gates json: `fails: []`.
Looked at: `ab_27_vs_28_{az180,az90}_real.mp4` frame by frame v84-v107 (extracted file k = v(k+76); 27 = LEFT half, 28 = RIGHT half),
builder grids `grid_pivot_az180_game_v84-102.jpg`, `grid_game_crop_v84-102.jpg`. 7 images total. Reviewer tiles in this folder:
`rev_az90_v84-95.jpg` (rows 27, 28, 27, 28), `rev_28_v96-107.jpg` (28 only; az180 two rows, az90 two rows),
`rev_rfoot_v87-93.jpg` (right foot zoom; rows 27 az90, 28 az90, 27 az180, 28 az180).

## Verdict: FAIL (pose), round 2 of 3. Not for the owner. Two items left, both small in scope.

## 1. Round 1 FAIL items
| item | round 2 |
|---|---|
| P1 arms lead | FIXED in the clip. Left elbow leaves the torso at v85-v86 (clear in az90 v86, mild in az180), wide and back by v87, before the chest passes 90 deg at v88. See M1 for why it is still small in game |
| P2 arms hold v88-v94 | FIXED for the pivot. Left arm rises v85-v88, peaks v88-v89, lowers v90-v93; right arm opens v90-v94. An arc, not a hold. The hold has moved later: see P6 |
| P3 left hand on front of hip v88-v90 (behind) | FIXED. One passing frame (v88), hand beside the thigh at v89 and outside from v90 |
| P4 fist at belly centre v97-v101 | HALF. Fist is now at the left hip line, elbow out, both hands visible from the front. But the arm does not pump: see P6 |
| P5 right foot | NOT FIXED. See P5 |
| G pivot in two beats | reads as ONE beat now, v85-v93 (about 9 frames), facing the run at v93-v94. No action |

## 2. Life (video, no numbers)
- The turn finally reads different from 27 at game distance: arms open through the pivot (game crop v90-v94), body more upright,
  one continuous beat. No robot spin, not late, no jitter seen. Left step out to the new heading as before.
- Open arms v91-v94 are wide but still moving, fine as the peak of the arc.
- The life drops at the hand-over: v96-v100 the left arm lands in a "hand on hip" shape (elbow winged out, forearm folded in,
  fist at the hip) and stays there 5 frames while the legs already rev (steps from v97-v99). The right arm hangs beside the thigh
  for the same frames. The first real pump stroke is v102-v104. Frantic legs under parked arms: this is the remaining "not always moving".
- Right leg v88-v90 still looks twisted from the side view: foot on the old heading under a pelvis that is 70-110 deg round.

## 3. Identity traits
| trait | result |
|---|---|
| Pivot 180 in 6-9 f, head + shoulders lead | PASS by eye (v85-v93) |
| Planted pivot foot, other foot steps out already aimed, no crossing | PASS (left foot v86-v88) |
| Pelvis -3/-4 cm in phase 1 | PASS by eye, as round 1 |
| ARMS LEAD (L elbow back+out, R elbow out to its side) | PASS in the clip; R elbow is modest until v90 |
| Arms always moving, never tucked | FAIL P6 (v96-v100) |
| Hands outside hip/thigh silhouette | PASS in the pivot; borderline v96-v100 (fist on the hip line, in front) |
| No jitter | PASS on video. Gate `jit_elbow_r` 7.9 against a limit of 8.0 at f11: no margin, do not add speed there |
| R foot not rolled | no roll (gate roll_r 10.3), but FAIL P5 |
| Sprinter drop + frantic rev, two lines, no crossed ankles | PASS, drop from v95-v96, rev from v97-v99 |

## 4. FAIL items
- **P5 pose (or game, to be told apart first), right foot.** In az90 the right foot of 28 is the same as 27 at v87-v90: pointing
  along the OLD heading. It starts to turn only at v91 and peels at v92-v93, lift v94. The builder's note says it pivots on the
  ball toward the pelvis v84-v88; that is not in the capture. First check the authored foot yaw in the Cascadeur scene at clip
  f3-f9: if the yaw is there, the loss is in game (foot lock / IK), label "game", code queue, not a pose round. If it is not
  there, pose: foot yaw at least half of the pelvis yaw from v86 to v93 (about -30 deg at v88, -70 at v92), ball planted, heel swinging out.
- **P6 pose, arms parked v96-v100.** Both arms hold one shape for 5 frames at the start of the rev. Start the first pump stroke
  at v96: left elbow keeps driving back and the fist travels (down past the hip and back) instead of stopping on the hip; right
  arm comes forward on the first step. Same rhythm as the legs from v97. Keep the fist outside the hip line, do not go back to the belly.

Not counted as rounds:
- **M1 montage data / game.** The clip enters at its own f3 with a 0.1 s blend from idle (weight 0.15-0.41 at v84-v86), so the
  authored anticipation f0-f2 is never played and the lead is half strength for three frames. If the owner still finds the lead
  weak: start the clip at f0 or shorten the blend to 0.05 s for this state. One line in `gasp-divergenze.md` if it differs from stock.
- Coordinator note: project memory records the owner's wording as "other arm ACROSS THE FRONT"; this round's brief says "R elbow
  out to its side". I judged against the brief. Worth one confirmation with the owner before the other three directions.

## 5. Keep
Arm lead v85-v87, the pivot arc v88-v94, hand positions v88-v95, upright pelvis, left step, one-beat pelvis yaw, phase 2 legs.

## 6. Round 3 targets (last pose round, then the owner decides)
1. P5: tell pose from game on the Cascadeur scene, then fix on the right side. 2. P6: arm layers v96-v102 only.
Do not touch v84-v95 arms. Same three views plus the game crop; add the right foot zoom from az90 v86-v94.
