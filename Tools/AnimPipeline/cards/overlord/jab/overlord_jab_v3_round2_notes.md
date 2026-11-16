# Overlord jab v3, round 2 (2026-11-15): offline pose pass on review r1, nothing built
Cards edited in place; r1 copies `overlord_jab{1,2}_v3.r1.backup.json`. Previews `overlord_jab{1,2}_v3_r2_stick.png` (side / BACK = game cam / top; grey idle, lilac dummy).
Numbers from `animpipe/author.py` (`animpipe author --no-write` exit 0 + bone lengths PASS on both; `card.load` OK); scripts in `Saved/AnimPipeline/r2_scratch/`. Dummy = idle mirrored 95 cm ahead (shoulders +-23 with radius, chest front ~y 87-90). "Clear" = 2D distance outside own torso/head outline seen from behind.
## Jab 1 (29 f, active 6-8, FAF 22, root fixed). Keys now 0-22 every frame + 28
1. Contact f6 wrist (-17.0, 78.0, 134.0), tip (-19.7, 93.5, 137.5). From behind: wrist 4.2 cm inside the dummy silhouette, knuckles 1.8 inside, tip on its edge.
   Side: fingertips 3.8 cm into the dummy's upper chest (no gap). Height = dummy collarbone: at face height (z 141+) the hand left its outline from behind.
2. Body: pelvis (+3, +14, -0.5) vs idle (travel 14.8 max), chest lean 26 fwd / 4 left, trunk yaw -73, head 14 cm left of idle. Arm angle: 40 deg could
   not hold (shoulder would need y 40 at x +8): contact 29.2 deg, f7 32.8, f8 36.5, hold f9-f12 36.3 -> 35.5. Own-outline clearance f6-f12:
   wrist 16.5-22.3, knuckles 18.2-27.8, forearm middle 11.6-16.0 cm.
3. Left hand on the left hip f5-f12 ((23, 7.5, 97); pelvis (5, 11.5, 90)), elbow out to the left (x 38): not forward under the striking arm.
4. Kept f0/f1/f2/f4 unchanged (feel rule key f1). Hand steps f0->f13: 44.6 43.0 19.2 18.6 41.0 75.8 | 3.6 3.1 0.4 1.0 1.0 0.4 | 15.3.
   Dry arrival 41.0 -> 75.8 into f6 (35/65%), then 3.6/3.1. Worst bone 6->7 index_03_r 8.4 cm. Reach (hand y - pelvis y) peaks f6. f28 = idle (0.12 cm).
   Feet: left on idle every frame (0.05 cm); right stomp now 17 cm ahead (r1 12, needed for the forward pelvis), still 0.01 cm f5-15, steps back f15-19.
5. Fidelity: every frame 0-22 is a key (f3, 9-11, 13-14, 16, 18, 20-21 baked from the solver, origin claude); f23-27 off a linear blend <= 0.07 cm.
## Jab 2 (53 f, active 9-11, FAF 26, root fixed). Keys 0-26 every frame + 52
1. f0 = 50% from the NEW jab 1 f12 toward the break: hand_r 39.0 cm, hand_l 16.7, pelvis 7.6, spine_05 11.3, head 13.7, shoulder line -67 -> -29 deg.
   Hand steps f0->f9: 25.6 13.9 12.2 13.0 10.8 9.2 10.9 40.8 67.3 (min 9.2, no dead frame); windup peaks f6 (z 172, y -30).
2. Short hold f9-f14, step back: left foot lifts f16, passes f17-18, lands on idle f19; right foot lifts f20, lands on idle f23.
   f26: pelvis 0.5 cm from idle, feet 0.00 / 0.00, hands 0.6 / 0.0, spine_05 0.5, head 0.6. f26-52 settle only; f52 = idle (0.12 cm).
   Pelvis y vs idle f8-f26: 19 24 24 24 24 24 22 19 16 12 8 6 4 2 1 1 0 0 0.
3. Contact f9/f10/f11 wrist (-19.5, 79.2, 128.0) / (-19.8, 79.3, 126.6) / (-20.2, 79.1, 125.1). Own-outline clearance wrist 19.7-21.2,
   knuckles 23.3-24.1, forearm middle 14.5-15.6. Dummy from behind: wrist 3.3-3.5 inside; side: tips 4-5 cm into its chest.
   Pelvis (+8, +23.5, -2), chest leans 10 left, trunk yaw -64, elbow at x -9 (outside the body line), left hand on the hip (as jab 1).
4. Strike step f8->f9 67.3; f9->f12 1.4 1.6 4.3; arm lowers f12-f19 in 7-14 cm steps (max after f11 = 14.2, 21% of the strike).
   Reach peak f9 (58.2; f10 58.1, f11 57.8, f12 55.7). Worst bone 11->12 ring_03_r 6.3 cm.
5. Rear (right) knee 18/19/20 at f9/10/11 (heel up 10 on the ball). Front knee 38-40 deg, knee at y 38 (limit 55).
6. Fidelity: every frame 0-26 keyed; f27-51 off a linear blend <= 0.06 cm.
   Feet: left planted f9-15 0.01 cm, on idle f19-52 0.05; right on the stomp spot f0-19 (ball 0.01; heel up 3/8 deg f5/f6 so the leg reaches).
## Deviations
- Jab 1 contact is 29 deg (fallback), at upper-chest height, not the face; front knee 32 (r1 "legs long" had 12) from the weight shift onto the left foot.
- Jab 2 observe/look-at-hand beat removed (FAF rule). sheet_extra.hold_window 9-26 -> 9-14. Left heel peels on f2 (r1 f3) after the re-spacing.
- Jab 2 contact arm is near straight (elbow 12) with the elbow on the outer line, not a bent flared elbow: a bent arm could not reach y 79 from the lunge.
## Risks to check after the build
- Owner/Manuale layer: the jab 2 hand_r tremor f27-37 (additive, not in the card) now plays on a hand hanging at idle: owner to keep, move or drop.
- Hitboxes still at r1 placement. Jab 1 hand at f6 is 17 cm right, 78 fwd, z 134 (pelvis +44): proposal lateral +17 right, reach 65->85, r35.
  Jab 2 hand f9-11 ~20 right, 79 fwd, z 125-128: proposal lateral +20, reach 70->90, height +35 over pelvis.
- Not checked offline: physbodies clearance of hand_l on the hip vs pelvis (hand 18 cm from the pelvis centre), legibility/motion checks, the end-of-move
  crouch (Anim BP blend-out, review r1 blocker, not in the poses), the jab 2 first-frame montage jump (a 2.1 repeat in capture, game side).
## Jab 1 f0 (r2b)
1. In-game montage starts at position 0 with no blend-in, so f0 = old f1 "leave" (FEEL RULE); f1 "load" = old f1->f2 at 0.55; f2 = old f2; f3 = old f3 + toe lift; f4 chamber and f5+ untouched (diff: author keys 0, 1, 3 only). Backup `overlord_jab1_v3.r2a.backup.json`, script `r2_scratch/design_j1_r2b.py`.
2. f0 vs idle: pelvis 4.3 cm (46% of the 9.5 to the chamber), shoulder line turned -57 deg (hips -17), right hand 44.6 cm, right elbow +13 cm up, knees L23 / R37 (idle 19 / 16).
3. Hand steps f0->f6: 23.6 20.2 19.2 18.6 | 41.0 75.8 (min 18.6, max neighbour ratio 2.20 at f4, as before); shoulder line -57 -74 -89 -109 -130 deg f0-f4.
4. Feet: left on idle f0-f4 (<= 0.07 cm). Right already on the toe at f0 (heel up 2.1 / 2.7 / 3.2 f0-f2, ball on the idle ball <= 0.08 cm). Old f3 dragged the ball 4 cm back on the ground: now lifted 2.0 cm at f3, sets down 8 cm back at f4 (unchanged).
5. Validation: `card.load` OK, `animpipe author --no-write` exit 0, bone lengths PASS. Timing CSV, hold_window 6-12, exemptions (none) unchanged; `_planted_why` and `_changes` updated.
6. Watch: from the game camera the f0 right hand is in front of the torso (wrist 8.4 cm inside the own outline); f0 reads by the turned shoulders, the forearm across the chest and the left arm swinging out. Stick `overlord_jab1_v3_r2b_stick.png`.
