# Review, run start turnaround 180 L, variant 28, POSE ROUND 3 of 3 (base 27)
Date 2026-11-21. Rules 10, 13, 15a. Evidence: `claude/runstart-generic-desktop` 3c779b08, `docs/notes/reviews/turn3-v27/180a_style_r3/`.
Capture: `assert_v28.txt` ASSERT: OK (f88-f132, weight >= 0.8 on 100 %, min 1.00). Gates json: `fails: []`.
Looked at: `ab_27_vs_28_{az180,az90}_real.mp4` frame by frame v84-v107 (extracted file k = v(k+76); 27 = LEFT half, 28 = RIGHT half),
builder grid `grid_pivot_az180_game_v84-106.jpg`, round 2 tile `rev_28_v96-107.jpg` for comparison. 7 images total.
Reviewer tiles in this folder: `rev_az90_v84-95.jpg`, `rev_az180_v84-95.jpg` (rows 27, 28, 27, 28), `rev_rfoot_az90_v86-95.jpg`
(right foot zoom, rows 27, 28, 27, 28), `rev_28_v96-107.jpg` (28 only, az180 two rows, az90 two rows),
`rev_28_arms_v95-104.jpg` (28 only, upper body zoom, az180 two rows, az90 two rows).

## Verdict: FAIL (pose), round 3 of 3. One pose item left (P6, reduced but still visible). Pose rounds are used up: the owner decides.
The pivot itself (v84-v95) is good and clearly better than 27. The defect is only the 6 frames of hand-over to the rev.

## 1. Round 2 open items
| item | round 3 |
|---|---|
| P5 right foot | UNCHANGED in the capture v86-v90: the right foot of 28 is pixel-identical to 27 (old heading) while the two clips differ by 17-50 deg there according to the builder. Two different clips giving the same foot means something in game holds it. From v91 the foot of 28 does turn more than 27 (heel swings out v92-v93, `rev_rfoot_az90_v86-95.jpg`). Label **game**, not a pose round |
| P6 arms parked v96-v100 | HALF. There is motion now: left elbow drives back v96-v101 (az90) and the fist slides down from hip to thigh; second stroke v102-v107 is bigger than round 2 (left hand reaches head height). But from the front the left arm keeps the same folded "hand on hip" shape v95-v100 (6 frames) and the right arm hangs beside the thigh until v101. First readable stroke is still v102. See P6 |

## 2. Life (video, no numbers)
- Pivot v85-v93: one beat, upright, head and shoulders first. The arms now really lead and open: left arm out and back from v85-v86,
  wide through v88-v92, right elbow out to its own side v90-v95. At game distance 28 no longer looks like 27. No robot spin, not late.
- No jitter seen. Left foot steps out to the new heading, no crossing. Drop from v95-v96, rev steps from v97, two lines, ankles not crossed.
- The life still dips at v95-v100: legs already rev (steps v97, v99-v100) while the left arm is a folded wing with the fist in front
  of the pelvis and the right arm hangs. It moves, but as one slow stroke over 11 frames (v96-v107) against steps every 3 frames.
  It reads "hand on hip, then go", not "arms pumping with the legs".

## 3. Identity traits
| trait | result |
|---|---|
| Pivot 180 in 6-9 f, head + shoulders lead | PASS (v85-v93) |
| Planted pivot foot, other foot steps out aimed, no crossing | PASS |
| ARMS LEAD THE TURN (L elbow back+out, R elbow out to its side per brief) | PASS, clear in az90 and az180 |
| Arms always moving, never tucked / T-rex | FAIL P6 (v95-v100 folded left arm, right arm hanging) |
| Hands outside hip/thigh silhouette | PASS in the pivot. FAIL v96-v100 from the front: left fist in front of the pelvis, then on the front of the thigh |
| No jitter | PASS on video. Gate margins are thin: `jit_knee_l` 9.7 of 10.0 (f1), `jit_hand_r` 7.4 of 8.0 (f7), `clear_hand_l-thigh_l` 14.3 of 14.0 min (f10) |
| R foot not rolled | no roll (gate roll_r 10.3). Late turn = P5 game |
| Sprinter drop + frantic rev, two lines, no crossed ankles | PASS for body and legs |

## 4. FAIL items
- **P6 pose, hand-over v95-v101 (arm layers only).** Left arm: unfold the forearm at v95-v96 so the fist passes BESIDE the hip
  (outside the pelvis line seen from the front) instead of across the belly and down the front of the thigh; elbow not winged
  out for more than 2 frames. Right arm: comes forward with the first step at v97, not at v102. One full stroke per two steps
  from v96. Do not touch v84-v94 and do not add speed on the right hand or left knee (gate margins above).
- **P5 game, right foot v86-v90.** Code queue, not a pose round. Check in this order: foot placement plant lock holding the
  planted foot yaw during the turn state, then the entry blend (clip enters at its f3). Rule 14: ask the Unreal Assistant how
  stock GASP releases the foot lock on pivots before changing anything. Re-capture the foot zoom az90 v86-v94 after the fix.

Notes, not counted:
- Second stroke v104-v107: the left arm swings out to the side with the hand at head height (az180). It is alive, but it goes
  sideways more than front-back; if the owner finds it flappy, lower the elbow about 15 deg. Not a FAIL.
- Left arm peak v88-v92 is high (about shoulder height from the side). Fine as the peak of the lead; watch it on the mirrored 180 R.
- Right arm: judged on the brief ("R elbow out to its own side"). The owner's earlier wording was "other arm across the front":
  still to be confirmed with the owner before the other three directions.

## 5. Keep
Everything v84-v94 (arm lead, arc, upright pelvis, left step, one-beat yaw), phase 2 body and legs, right elbow out v90-v95.

## 6. For the owner (rule 10: three pose rounds used)
A (recommended). One more small pose pass limited to v95-v101 arm layers (P6 above), after the P5 game fix, then one re-review. About 4 $ desktop + 3 $ review.
B. Show 28 to the owner as it is with the known 6-frame arm dip; he decides on video.
C. Keep 27 (already PASS) for 180 L and drop the style pass.
