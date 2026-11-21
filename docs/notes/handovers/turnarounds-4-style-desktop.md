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
## Pose round 3 (coordinator 23:39Z, last before the owner, budget 5 $) -> 180a_style_r3/
- P6: the pump moved front/back in depth only, so from behind it read as a hand on the hip. Shoulder swing about the lateral
  axis +-25 deg with the pump phase (+20/15 deg extra lift and elbow flex on the forward swing), f6-f27, faded in/out:
  L hand vertical range 23 -> 35 cm, peak speed 24.4 -> 32.9 (more, not less). Fix try 1: elbow_r zigzag f12-13 -> one
  smoothing pass f10-16 on the R arm -> GATES OK. Capture Saved/Turn4/t4c, ASSERT OK both.
- P5 = GAME item (not a round): the UE clip HAS the R foot spin (foot-pelvis gap f3-f6 -17/-31/-48/-44 deg vs 27's
  -34/-61/-95/-87, joints json), the game shows it only from v91. Suspects: entry blend from idle 0.1 s (clip weight
  0.15-0.55 at v84-88, idle foot dominates), ik_foot / leg IK or foot lock. Needs a capture that logs foot_r yaw per frame
  (pie_capture_move body.*) and the leg IK alpha; code queue.
- R arm "elbow out to the side": owner question (coordinator).
COSTO: ~18 $ total (6 r1 + 3 A + 5 r2 + 4 r3) | ~30 min desktop | 0 token review
