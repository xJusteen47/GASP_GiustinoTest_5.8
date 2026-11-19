# Overlord jab v3, round 1 (2026-11-14): offline pose pass, nothing built
Owner notes from the game camera (behind) and the side. Cards edited in place (asset names still V3); r0 copies: `overlord_jab{1,2}_v3.r0.backup.json`.
Previews: `overlord_jab{1,2}_v3_r1_stick.png` (rows side / BACK = game camera, character's right on screen right / top; grey = idle). Numbers come from the
author solver (`animpipe/author.py`) and its own smoothstep in-betweens, not from the built rig curves. Axes: +X character left, +Y forward, +Z up, cm.

## Jab 1 (timing unchanged: 29 f, active 6-8; keys 0,1,2,4,5,6,7,8,12,15,17,19,22,28)
- f1 NEW key (owner feel rule): pelvis 45% of the chamber move, pelvis yaw 43%, chest 44%, right elbow 58%. f2 chest raised to 68% of the chamber coil (was 33%).
- f4 chamber: hand moved in the chest frame (-3, +7.5, +4); the planned +3.5 on Y alone only gave -0.7 cm offline. Hand 3.5 cm off spine_05 (worst f1-f5).
- f5 drive: pelvis almost at its contact yaw (hips first), chest lags 38 deg, hand trails left of the face (elbow 140). Hand f4->f5 44.9, f5->f6 73.2 cm = 38/62% (R4).
- f6 contact: DIAGONAL. Shoulder line and arm both 40 deg right of +Y, wrist (-43.9, 60.6, 138.5), 30.6 cm outside the right shoulder in X, +8.0 above it, elbow 20.
  Backhand: palm faces back-left, back of the hand leads the motion. Pelvis (-6, +9, +0.3), yaw -20; chest lean 8 toward the target, chin up 12.
- Legs long f5-f12: left foot flat on idle, right on the stomp spot. Knees f6 L12/R20, f8 L16/R23, f12 L11/R20 (idle L19/R16). Pelvis 0 to +0.5 cm vs idle.
- f7 flick / f8 overshoot along the same arc (arm 44 / 46.5 deg). Hand 4.1 / 2.7 cm. Worst bone 6->7 is middle_03_r at 8.4 cm (R7 <= 10).
- Hold f8->f12 breathes: hand 4.4 cm back to the line (arm 43 deg), head 2.9 cm (chin 14 -> 19), pelvis +0.5, chest straightens 3 deg.
- Recovery f15/17/19: trunk unwinds from the new contact yaw (-38/-20/-7 deg); f15 hand placed under the new arm line. Planted exemption removed (no left heel lift).

## Jab 2 (timing unchanged: 53 f, active 9-11; keys 0,1,3,4,5,6,7,8,9,11,12,13,20,26,38,41,44,46,48,50,52)
- Root motion removed (`edits` on root_ctrl deleted). Step 18 cm in pose space: left ankle y 12.5 -> 30.5 at f9 (heel peels f3-4, air f5-7 with the knee up
  at f6, heel strike f8, flat f9). Pelvis.loc y 9 (f0) -> -4.5 (f6) -> 17 (f8) -> 28 (f9): +19 cm vs f0, +28 vs idle, over the left ankle.
  Right foot stays on the jab 1 stomp spot f0-46 (heel up 10 deg f9-38). Recovery: left foot steps back f41-46, right shuffles back 12 cm f46-50,
  f52 = idle (max bone 0.12 cm, rounding), root never moves: net travel 0.
- f0 = jab 1 f12 (identical numbers). f1 NEW key at 42% toward the break; hand 22.7 cm on f0->f1.
- Windup f3/f4(new)/f5(new)/f6: hand z 156/163/168/172, y 12/-6/-20/-30, pelvis rises to +2.8, chest yaw (shoulder line) 95/125/148/168 deg.
- Claw f6->f7->f8->f9: 10.9 / 35.7 / 72.0 cm = 9/30/61%. Active f9->10->11->12: hand 4.0/4.0/6.1, worst bone 7.9 cm.
- f9 contact: wrist (-16.3, 78.4, 125.8), palm down, fingers pitched 45 deg down, tips about y 92, z 117. Shoulder z 125: arm is horizontal from the shoulder;
  the DESCENT is in the path (172 at f6 -> 126 at f9 -> 114 at f12 -> 72 at f13) and in the hand pitch, not in the arm angle. Elbow 37. Trunk upright (lean +4 back).
- Lunge f9: front (left) knee 51, rear (right) knee 29, heel up. Observe f20-38: front 46-48, rear 14-21. f20-41 upper body = r0 (tremor layer unchanged).
- Left hand at f20-41 moved 4-7 cm out in the chest frame (it was 1.5 cm inside the left thigh).

## Deviations from the plan / asks
- Jab 2 rear knee 29 at contact, not 15-20: a longer rear leg needs the pelvis 4-5 cm further forward, which pushes the hand 4-5 cm deeper into the target.
- Jab 2 contact arm is not "fully extended forward-down": with the lunge the shoulder sits at z 125, so a hand at chest height cannot be below it. Plan said spine 30 forward: used upright.
- Jab 2 fingertips reach y ~92: about 7-9 cm inside a dummy placed 95 cm ahead (its chest front estimated y 83-87); the wrist is 5-9 cm in front of it.
- Jab 1 f4 clearance offset 7.5 cm (not 3.5). Offline clearance (physbodies, all frames): min 2.6 cm jab 1, 2.5 cm jab 2 (hand_l to pelvis).

## Open risks (need the editor)
- Build in-betweens are rig curves, not these smoothsteps: re-check hitstop_snap (f6-9, f9-12), clearance f1-f5, and foot slide on f4-f9 / f41-50 after build.
- Jab 1 hitbox is fixed in root space (reach 60->110, 0 lateral, z+30, r45): it still overlaps a dummy 95 cm ahead, but the hand lands 44 cm right of
  its centre line and ~25 cm short of its face. Proposal (not applied): lateral +30 (actor right), reach 55->65, r40; or owner reduces the arc to ~25 deg.
- Jab 2 hitbox (reach 60->120) was sized for +30 root motion; proposal (not applied): reach 70->85, lateral +10 right, height +25, r45.
