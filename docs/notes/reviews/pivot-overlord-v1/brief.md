# Review brief: Overlord run pivot v1 (wc8ksn, 2026-11-21)

Move: 180 reversal while running (stick full forward, reversed at capture f95; the capsule now reverses in ~0.27 s: 586 cm/s,
min ~20 at f124, 420 cm/s at f134, 60 fps frames). Base 1 (stock GASP ABP + chooser), `gstn.Skid.Sim 0`, VFX on in all captures.
- BASE (`base_*`, `base_r_*`): stock pick (M_Relaxed_Run_Turn_*_180 mocap, ~0.7-0.8 s turn core, blend 0.3 s), PivotSet row off.
- VARIANT (`var_ovl_l_*` turns LEFT, `var_ovl_r_*` turns RIGHT; `var_ovl_*` = straight reversal, picks the left clip):
  AS_GSTN_RunPivot_Overlord_R = the same stock Turn_R_180 mocap with the turn core retimed 0.70 s -> 0.30 s (smooth time warp,
  1x at the seams, peak 3.7x), curves/notifies remapped, root lock as stock; _L = baked mirror of _R. Row enters f118, blend 0.1 s,
  full weight f124-f130. NO pose change yet: the pose is stock mocap (Cascadeur pose pass is the next step, desktop only).
- Grids: frames 112-158 step 2, views game camera and side (az90). Same frames for base and variant. Videos *.mp4 (f100-170).

Identity traits (owner, this move only):
1. Timing like Toad (Mario 3D World skid): stick reversal to running the other way ~0.26-0.3 s, quick, never locks you in place.
2. Brute style, "Ganondorf inside Astro Bot": heavy, grounded, cartoon-readable brake. (v1 keeps the stock pose: judge whether
   the retimed stock pose reads as a heavy brake or as a light jog turn, and say what the Cascadeur pose pass must add.)
3. Both sides: the body turns the same way the capsule turns, left and right.

Judge FIRST the life by eye (anticipation, arc, weight, follow-through, body turn direction vs capsule, root coherent with the
capsule), THEN technical: pops at row entry (f118-124) and exit (blend out), foot slide (intended slide in the brake = OK, mark it,
not a fail), mesh through floor, mirror errors on the left side, timing vs trait 1. Compare with base on the same frames.
Verdict PASS/FAIL per item, each FAIL labelled pose / montage-data / game. Overall: PASS only if traits 1 and 3 pass and no
data/game FAIL; trait 2 may be "pose pass needed" without failing v1 (it is a timing/data checkpoint). Write review-fable-r1.md here.
