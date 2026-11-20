# runstart-accel-1 (cloud + desktop, 2026-11-20): run start acceleration ramp, no standstill
Branch claude/runstart-accel-1 (merged claude/runstart-accel-1-desktop bdd06da8, which carries v4r4 elbows ee7d799b). _ue59 free.
Owner: "takes a while to get going" = gradual acceleration (Astro Bot / Bananza), base it on Toad, tune by hand; 19:12Z the
"stomp" was meant for the run STOP: the run start has no standstill (runstart-identity.md trait 1 withdrawn for the start).

Measured (report.md, speed_curve.py on metrics_v4r3.json): v4r3 = hold 0-0.37 s (stamp 21 f), snap to stage 1 in 0.23 s,
flat 0.6-1.25 s, snap to stage 2; t10/t50/t90 0.37/0.52/1.30 s. Model ramp_preview.py reproduces it.
DA_Locomotion_Default real values (desktop dump setup_run_start_ramp.txt): stage 1 470/2000 ramp 6 f/0.3, stage 2 610/1400,
stage 3 850/1200, stamp 21 f. Toad (grids ref-smash-run): ~0.33 s in place, then ~0.6 s of growing speed, no step.
Unreal Assistant: stock = walking-mode Acceleration with spring smoothing; a linear ramp on top double-eases (row 13).

READY, NOT APPLIED: Tools/GAS/setup_run_start_ramp.py (headless ~15 s, idempotent on target values):
  Start Stamp Frames 21 -> 0; Stages[0] AccelRampFrames 62, RampStartFraction 0.15; Stages[1] AccelRampFrames 30, 0.25.
  Model: moves from frame 1, ~95 cm/s at 0.2 s, 470 at 0.57 s, 610 at 1.34 s, no snaps (ramp_preview.png).
Hand tuning (DA_Locomotion_Default > Run Stages): Start Stamp > Frames; Stages[0|1] > Acceleration Ramp > Accel Ramp Frames
  (longer = softer start), Ramp Start Fraction (higher = moves sooner); Stages[0] > Acceleration; Stages[0] > Frames To Next
  Stage (40 = the flat run phase before sprint 1).
Apply WITH the clip change (thread "Fix run start torso snap"): AS_GSTN_RunStart_v4r4 still opens with the planted stomp step,
  with the stamp off the feet slide for ~0.3 s; needs frantic cadence from the first contact, stride growing with the ramp.
  Then capture, speed_curve.py with REF=v4r3 metrics, owner video.
Earlier desktop measure with the stamp kept (31 f ramp): t50 0.70 s, t90 1.52 s, superseded.
COSTO: ~7 $ cloud + ~4 $ desktop | ~6 min desktop | review 0
