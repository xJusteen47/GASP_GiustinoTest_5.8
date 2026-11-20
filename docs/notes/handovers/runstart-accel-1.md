# runstart-accel-1 (desktop, 2026-11-20): run start acceleration ramp, PAUSED
Branch claude/runstart-accel-1-desktop (from claude/runstart-v4r4-elbows ee7d799b + paths from claude/runstart-accel-1 77fbc9d6).
_ue59 lock taken and freed.
Done: Unreal Assistant answer in docs/notes/runstart-accel/report.md, row 13 "stock alternative" fixed (gasp-divergenze.md).
Setup dump: DA stage values differ from code defaults (stage 1 470/2000 with 6 f/0.3 ramp already, stage 2 610/1400, stage 3 850/1200);
setup_run_start_ramp.py skipped stage 1 for that, fixed to idempotency on target values (setup_run_start_ramp.txt).
One measure capture with the ramp (ASSERT: OK): t10/t50/t90 0.40/0.70/1.52 s vs v4r3 0.37/0.52/1.30 (speed_ramp.png, report.md).
Owner STOP 19:12Z (stomp misunderstood): DA_Locomotion_Default git-restored, no owner video, no uasset committed.
Open: wait for the owner's clarification; then re-brief (the script re-applies in 15 s headless).
COSTO: ~4 $ | ~6 min desktop | review 0
