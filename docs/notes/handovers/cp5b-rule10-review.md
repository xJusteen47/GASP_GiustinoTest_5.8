# Handover: rule-10 review of cp5b recaptures (cloud, read-only, 2026-11-15, ~$6 est.)
Source `claude/integ-cp3` 069287f7; this branch adds docs only. Reviews: `docs/notes/reviews/cp5b-{jab-chain,dropkick,dair-v9,hit-cue}/review.md` + `rv_*.jpg`.
Owner clips (decision support, not variants): project files `reviews/cp5b/q1..q4b_*.mp4` (normal speed, then 1/4).

**Montage 30 Hz fix: CONFIRMED by eye.** One new pose per frame from a0 in all moves (jab, dropkick, dair x3, cue);
startup 0-px frames gone (jab v23/v25 were 0 px in cp5). Rows agree: `cd docs/notes/reviews/cp5b-data && python tools/count_repeats.py`.
**Victim half-turn frame: GONE** (cue v26, dair hit v91): idle → hit pose in one frame, facing the attacker.

Verdicts (none PASS, nothing goes to the owner as a move):
- Jab chain FAIL: `posa` jab 2 contact = jab 1 silhouette from behind (round 2 of 3 still to execute); `gioco` end squat v83-v130.
  Jab 2 hand tremble: not visible at 2x side zoom (hold drifts down 313→729 px/frame); owner clip q2.
- Dropkick FAIL `gioco` only: air part passes; lying still v105-v128 then pops to crouch in 4 frames v129-v132 (no get-up); 53 cm drop.
- Down-air v9 FAIL `gioco`: legs blend into GASP jump pose after montage end (v71→v99, soft now, no snap); air tail plays on ground
  (land v101-v116, owner decision pending: landing section per aerial); NEW: attacker interpenetrates victim after landing (hit v108-v118).
- Hit cue FAIL `dati montage` (example kit): attacker still idle on hit frame. Shake visible in az0 (~4.5 px), invisible in game cam.

Notes for next threads:
- Frame convention: video index 0 = v(rows − video frames); `rv_grid.py`/`rv_diff.py` take that value as FIRST_V
  (jab 14, dropkick 36, dair 31/77/69, cue 14). The capture overlay "vNNN" top-left is the ground truth.
- Code queue (gioco): get-up for dropkick; dair land-cancel/landing section; post-montage pose in air (ABP); character push-out
  after aerial landing; ABP crouch at end of jab.
- Pose queue: jab 2 round-2 instruction (hand at hip, ≥30 cm below jab 1 hold, visible right from behind).
