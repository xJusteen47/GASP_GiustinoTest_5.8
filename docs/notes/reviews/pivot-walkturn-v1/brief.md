# Review brief: run pivot / skid, walk turn variant v1 (skid-walkturn-3, 2026-11-20)

Move: 180 reversal while running (stick full forward, reversed at capture f95; the pivot row enters at f117). Base 1 (clean GASP
stock ABP + chooser), our sim skid OFF (`gstn.Skid.Sim 0`): the Mover capsule brakes and re-accelerates like stock GASP.
- BASE (`base_*.png`, `base.mp4`): stock pick M_Relaxed_Run_Turn_L_180_Rfoot (mocap, turns LEFT), plays straight into the run.
- VARIANT (`var_*.png`, `var.mp4`): our clip AS_GSTN_RunPivot_WalkTurn180 (Mixamo sword-and-shield walk turn, turns RIGHT only, no
  mirror yet) picked by the new PivotSet row; weight >= 0.8 f130-f136, then GASP re-picks M_Relaxed_Run_Reface_Start_090_R (f135+),
  so ~0.3 s of our clip is seen. Grids: frames 110-157 step 2 (60 fps), views game camera and side (az90). Same frames for both.
Owner goals (scope: this skid/pivot only):
1. A QUICK skid that flows back into running, not a move that locks you in place (old skid: 0.83 s clip + exit at sim end).
2. Timing reference = Toad (Mario 3D World), by eye on a 30 fps phone video: skid onset to running the other way ~0.3 s (+-0.1).
3. GASP warping should prevent foot slide (fix setup, not clips). The clip is a walk turn used for the run (owner accepted the test).
Judge FIRST the life (anticipation, arc, weight, follow-through, does the body turn the same way the capsule turns, root coherent
with the capsule) by eye, THEN technical: foot sliding, pops at f117 / f135, wrong turn direction, timing vs Toad.
Verdict PASS/FAIL per item, FAIL labelled pose / montage-data / game. Max 80k tokens. Write review-fable-r1.md here.
