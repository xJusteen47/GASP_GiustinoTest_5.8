# turnarounds-4 style (desktop, 2026-11-22): 180 L style pass in Cascadeur = variant 28; gates OK after round A, captured, ASSERT OK
Branch claude/runstart-generic-desktop (from 5b92370a). Lock _ue59 22:58-23:05Z, editor never opened (headless Save-Assets only).
Re-check of turnarounds-3 (rule 11): 27 (w2t180Lp) UE clip gates rerun now -> GATES: OK (joints json, gates script below).
## Round 1 (Cascadeur key poses + curve sheet, 180a_style/) -> gates OK after round A -> cloud review r1 FAIL posa (P1-P5)
- Key poses arms/head f0/2/4/6/8, pelvis phase-1 drop 3-4.7 cm, world-space breakdowns (scene is in the ROOT frame, yaw added
  at import: world = R(-yaw_ue_deg) * casc). Variant 28 rows (RST_K=28, LO 27.5/37.5); CHT not committed (had 27 rows already).
## Pose round 2 (coordinator 23:22Z, budget 6 $) -> 180a_style_r2/
- Found: in game the clip ENTERS AT ITS FRAME 3 (capture v84 = clip 0.050 s, frames.jsonl combat.anim), clip f = v - 81.
  Round 1's lead keys f0-f2 were never shown. New arm keys f3 (L elbow 0.80,-0.30,-0.52 out-back) and f6 (0.90,-0.25,-0.35),
  R out to its side; world-space Hermite in-betweens f1-2/4-5/7; one smoothing pass f2-f14 only.
- Pump f8-f26 = the 27 composer pump (arms_w2t180Lp_f0-26.json) + soft lateral correction (elbow out >= 0.35, hand >= 8 cm
  beyond the hip line, correction smoothed in time, faded f21-24): fists beside the hips, not at the belly (P3, P4).
- R foot spins on its ball toward the pelvis at f3-f7 (+17 +30 +47 +43 +11 deg = half the gap), knee dir half (P5).
- Fix tries this round: (1) full smoothing f2-23 -> gates OK but pump peak speed -18/-25 % (> 15 % guard) -> rebuilt;
  (2) soft correction -> GATES OK, conservation L amp 55/peak 24.2 vs 27 54/24.4, R 55/25.1 vs 56/27.3 (-8 %).
- Capture Saved/Turn4/t4b (27 + 28 same session, az180/az90/game, 960 px): ASSERT OK both (v28 f88-f132 min in assert_v28.txt).
- G (game/root) re-measured, frames.jsonl body.face_hips: v28 0 (v84) -27 (v86) -66 (v88) -107 (v90) -138 (v92) -155 (v94)
  -160 (v96): ONE continuous beat ~20 deg/frame v85-v93, then ~10 deg tail; the last ~20 deg are never reached (cap facing).
- Evidence 180a_style_r2/: ab_27_vs_28_{az180,az90,game}_{real,slow4x}.mp4, grid_ab_*.jpg, grid_pivot_az180_game_v84-102.jpg,
  grid_game_crop_v84-102.jpg (game camera cropped on the character, as asked), gates json, casc. My look (not a review): wider
  arms v90-96 and hand beside the hip v96-98 read also in the game crop; the lead at v84-86 is small because the clip enters
  under a 0.1 s blend from idle (weight 0.15-0.41).
- Next: cloud review round 2 (base 27). 180R/090L/090R still on the f clips at 28.
COSTO: ~14 $ total (6 r1 + 3 A + 5 r2) | ~25 min desktop | 0 token review
