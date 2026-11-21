# runstart-v8-cloud: Luigi run start measured, v4r8 retime prepared (cloud, 2026-11-20)
Branch claude/project-thread-64vizj (from claude/runstart-v7-desktop 83e6f1f3). Nothing applied: needs the DESKTOP (Cascadeur + editor).
v4r7 is approved and LOCKED (approved/runstart-v4r7): v4r8 is a new variant on a copy of RunStart_v4r7.casc.

## 1. Luigi measure (IMG_9588.mov, Mario 3D World on a TV filmed by phone, 30 fps; grids docs/notes/reviews/runstart-v4r8/ref/)
- Straight start t 10.13 s (first move) -> 10.33 s (first normal stride): 6 f = 0.20 s. Start with ~120 deg turn t 1.33 -> 1.60: 8 f = 0.27 s.
- OWNER 00:38Z: the measure is the time to cross the FIRST BLOCK from the first move. IMG_9590 (stone floor, glass tiles): starts on
  the right edge of a tile f24 (0.80 s), leaves its left edge f38 (1.27 s) = 14 f = 0.47 s (+-1 f). Tile ~= 1.35 x Luigi's height
  (170 px vs 125 px, same depth). Strip project files handoffs/luigi-ref/luigi_first_tile_IMG9590_f21-42.jpg.
- TARGET 0.47 s (29 sim f at 62.5 Hz), distance 1.35 x Overlord height (D_TARGET in the script sets the ramp start fraction).
## 2. v4r7 today (AS_GSTN_RunStart_v4r7_build.json): contacts R3 L6.5 R11.5 L16.5 R21.5 | L28.5 R37.5 L46.5 R56
- Frantic cadence every ~5 f until L28.5, normal 9 f after: rev-up 0.95 s; in game hand-off to the loop at t 1.28-1.42 (v7 handover).
## 3. v4r8 = runstart_retime_casc_v4r8.py (Cascadeur, one pass on points + FK boxes)
- Cut one frantic stride L6 -> L16 (crossfade +-1.5 f, root-relative), warp the remaining 18 f into round(TARGET_S*30) = 14 f with play
  rate decaying 1.6 -> 1.0 (engine revs hardest at frame 1, no cadence jump into the run), tail f28-58 copied 1:1 (loop match kept).
- Root re-profiled: linear speed F0*V_END -> V_END over the rev-up (DA ramp shape); leg strides scaled by v_new/v_warped (offline
  stride warping, min 0.3) so planted feet stay planted; D_TARGET = 1.35 x Overlord height fixes the distance. Arms/torso/identity untouched.
- Expected: 45 f (1.47 s), EXIT_AT (18/30) = 0.6 s; the script returns N_NEW, EXIT_AT, rate0, V_END, s_min.
- Params (globals): TARGET_S (v4r8b = 0.25, first-stride mark, rows off), CUT (0,0) = pure warp, RATE_SHAPE 'const', S_MIN, F0.
## 4. Desktop steps (thread nuovo, Opus medium, ~12 $, 1 player)
1. Fetch this branch, lock _ue59. Copy Saved/AnimPipeline/runstart/RunStart_v4r7.casc -> RunStart_v4r8.casc, open it, run the
   script (TARGET_S 0.47, D_TARGET = 1.35 x Overlord height in cm, from the mesh bounds). Delete keys N_NEW..58, scene size N_NEW (as v4r5). Export export_joints up Z -> RunStart_v4r8.fbx.
2. Gate before UE: runstart_arm_clearance.py (CL_RANGE 0-N_NEW), visual check of the 7 rev-up frames (no knee pop at the cut).
3. Save-Assets runstart_v4r8_ue.py with RSV8_EXIT_AT=<EXIT_AT>: AS_GSTN_RunStart_v4r8, rows v4r7 off / v4r8 on.
4. DA: setup_run_start_ramp.py RAMP_MODE=dump first, then RAMP_STAGES="0:29:<F0 printed by the script>" RAMP_KEEP_STAMP=1 (only stage 1 ramp
   changes; the 'before' line is the restore value). Stage 2 left as the owner set it.
5. Capture mv_run_start (expect clip v4r8, window shortened accordingly; also update the stale v4r5 expect), ASSERT: OK; grids v4r7 vs
   v4r8 same frames + mp4 in docs/notes/reviews/runstart-v4r8/; then cloud review (rule 10) before the owner video.
6. If feet slide or the cut pops (attempt 2): CUT=(0,0). Optional v4r8b (0.25) with RSV8_ROWS=none for an A/B.
Divergence row 17 in gasp-divergenze.md. setup_run_start_ramp.py: new env RAMP_STAGES / RAMP_KEEP_STAMP (old behaviour unchanged).
COSTO: ~5 $ | 0 min desktop | review 0 token
