# Dropkick tech bounce, review r2 (Fable, on 716d492c): FAIL only on D3; round 3 answers (desktop)
PASS: identity, f0, break in game camera, squash/rebound, contact frame.
FAIL D3 (posa/dati montage): GroundHit hand-off: arms from a bent W (hands by the head) to a flat T in ONE frame, torso turns 10-15 deg, then freeze
(g4 v152->153, g6 v149->150, g7 v153->154, g1 v149->150). Fix (preferred): f8-f11 elbows extend, hands out and down, reach GroundHit's first shown pose.
Warning (gioco): low hit contact v142: the variant drops ~40 px on v143.

## Round 3 (desktop), chain *R3 (KIT_Test_PlaytestTBR3), card overlord_dropkick_techbounce_r3
- Cause: GroundHit (reaction rule) shows from its frame 1 (frames.jsonl: 'AnimMontage_*', 1.0) and its own f0->f2 slams the arms from W (hands
  46-52 cm up) to flat on the floor (4 cm) in 2 frames (cards/reaction/reaction_groundhit.json). r2 ended on GroundHit f0 (the W) -> the slam showed.
- r3: f0-f8 as r2; f11 = GroundHit f1 (its first frame on screen), f12-f14 = GroundHit f2 (arms flat); f9-f10 interpolated from f8, so elbows extend
  and hands travel out/down over f8-f11 and the pelvis pitch goes 96 -> 87 deg (GroundHit f1).
- Low hit v142 (Saved/DropkickBounce/variant_r2/dropkick_low_hit/frames.jsonl): pelvis world -35 cm on the contact frame, 13.6 the next: the capsule
  already grew (actor 94.3) but the bounce montage starts one frame later, so the air loop is drawn from the standing root for 1 frame. Code queue
  (1-frame order: capsule restore vs. the forced ability's montage); the whiff case does not show it (the montage switches on the same frame).
