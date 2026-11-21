# Review, run start turnaround 180 L, variant 28 (Cascadeur style pass), base 27
Date 2026-11-21. Rules 10, 13, 15a. Evidence: `claude/runstart-generic-desktop` b49ec08a, this folder.
Capture: `assert_v28.txt` ASSERT: OK (weight >= 0.8 on 100 %, min 0.86, f88-f132). Gates json: `fails: []`.
Looked at: `ab_27_vs_28_{az180,az90,game}_real.mp4` frame by frame v84-v107 (extracted file k = v(k+76); 27 = LEFT half, 28 = RIGHT half),
`grid_pivot_az180_v86-102.jpg`. 8 images total. Reviewer tiles: `rev_lead_az180_v84-90.jpg` (27 top, 28 bottom), `rev_feet_v88-95.jpg`
(28; rows az90 v88-91, az90 v92-95, az180 v88-91, az180 v92-95).

## Verdict: FAIL (pose), round 1 of the style pass. Not for the owner.

## 1. Life (video, no numbers)
- At game camera distance 28 and 27 are almost the same clip. The only readable change is a more upright pivot (pelvis higher,
  legs straighter v87-v93), which is good. The arm work the owner asked for does not read from the play camera.
- Arms do not lead. v84-v86 are the same as 27 from behind; the left arm starts with the chest at v86-v87, not before it.
  A one-frame lead (curve sheet f2) is invisible at 60 fps.
- Arms then sit: the open pose is reached at v88 and held with small drift to v94 (both views), then the left hand jumps from
  the hip side (v95) to the centre of the belly (v97) in two frames and stays there v97-v101 while the legs rev. It reads as
  open pose, hold, tuck, not as arms that keep moving through the turn.
- No robot spin, no late feel beyond what 27 has, no jitter. Phase 2 drop and rev read as in 27 (two lines, no crossed ankles).

## 2. Identity traits
| trait | result |
|---|---|
| Pivot 180 in 6-9 f, head + shoulders lead | as 27 (root untouched). Head leads v85-v86. Body passes 90 deg at v88-v91 and faces the run about v96: see note G |
| One planted foot, other steps out to the new heading, no crossing | PASS, left foot steps out pointing to the new heading v87-v88, never behind |
| Pelvis only -3/-4 cm | PASS by eye, clearly higher than 27 at v88-v93 (sheet: 4.7 cm at H) |
| ARMS LEAD the turn | FAIL P1 |
| Arms keep moving, never frozen | FAIL P2 |
| Hands outside hip/thigh silhouette | FAIL P3 (v88-v90 from behind) and P4 (v96-v101) |
| No jitter | PASS (gate elbow_l 3.3, hands 0.0) |
| Right foot does not roll on its side | no roll seen (gate roll_r 10.3 deg), but see P5 |
| Phase 2 drop + frantic rev, two lines, no crossed ankles | PASS, drop from v96, rev steps from v99 |

## 3. FAIL items
- **P1 pose, arm lead.** Left elbow must start at clip f0-f1 and be clearly outside the torso silhouette from behind by v86,
  two frames before the chest turns, with enough travel to read in `ab_*_game_real.mp4` (now the game view shows no difference).
- **P2 pose, arm hold.** v88-v94 is one pose. Give the arms an arc through the pivot (overshoot at the step plant, settle while
  the pelvis finishes) and carry the arm layers past H so the hand-over to the pump is a swing, not a 2-frame tuck at v95-v97.
- **P3 pose, left hand v88-v90.** In az180 (the play camera side) the left forearm folds forward and the hand sits on the front
  of the hip/pelvis for three frames; 27 had the hand farther out here. Keep the hand lateral, outside the hip line, from behind.
- **P4 pose, hand-over into the rev.** Left fist at the belly centre v97-v101 (builder's note, v96-v98 and beyond), forearm flat
  across the body, right hand hidden behind the hip. After a wide open pivot this is the tucked look the owner rejected. In this
  clip only: first pump with the fist beside the hip line and the elbow driving back, both hands visible from the front.
- **P5 pose, right foot.** Feet are the same as 27, which the owner called broken, so the complaint is not addressed. There is no
  side roll. What shows is the right foot kept on the old heading v88-v93 while the pelvis is already 90-130 deg round: v88-v90
  the two feet point in opposite directions, then the heel peels with the toe still aimed backwards. Let the right foot spin on
  its ball with the pelvis (at least half of the pelvis yaw from v88) before it lifts at v94.

Not counted as rounds:
- **G game/root, note.** In both halves the body sits near 90-100 deg for v88-v91 and completes about v96, so the pivot reads as
  two beats over about 11 frames, not one beat in 6-9. Same as 27 round 2 (passed with numbers I did not re-measure here).
  If the owner still feels it late after the arm fix, trace hips yaw v84-v98 again.

## 4. Keep
Upright pivot and pelvis heights, jitter-free breakdowns (world-space midpoints), left foot step, phase 2 legs, wide arms v91-v94
as the peak pose of the arc (not as a hold).

## 5. Round 2 targets (2 rounds left, then the owner decides)
1. P1 + P2 + P3 together on the arm layers f0-f14. 2. P4 on the first pump only. 3. P5 right foot yaw v88-v93.
Capture again the same three views; add one behind view at game distance cropped on the character, because that is where it must read.
