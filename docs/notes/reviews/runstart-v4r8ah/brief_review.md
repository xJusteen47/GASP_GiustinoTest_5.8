# Review brief: run start v4r8ah = v4r8a + hand-off fix (one move, rule 10)
Branch claude/runstart-phase-desktop. Files here: grid_az90/az0/game.png (top = v4r8a BEFORE the fix, bottom = v4r8ah AFTER, SAME
frames v84-v144 step 4, same kit/sequence), runstart_v4r8ah.mp4 (v60-v240), metrics_summary.json, metrics_v4r8a.json / metrics_v4r8ah.json
(per-frame feet, actor). Capture ASSERT: Saved/RunStartPhase/v4r8ah/assert.txt = OK (clip weight >= 0.8 on 100 % of v97-v115).

## What changed (systemic, the same for every run start; cvar gstn.RunStart.Handoff 1)
- Entry: the start now enters on its FIRST frame (before: at clip time 0.133 s, after ~6 frames of a stock walk start) with a
  0.05 s blend-in (before 0.3 s QuadraticInOut). Owner: the reference enters the start almost instantly.
- Exit into the run loop: the clip copy carries the stock `phase` curve, which the loop's pose search reads to choose the loop
  foot. Before, the loop was entered on the other foot and the 0.5 s crossfade averaged two feet. Pose, root motion and timing
  of the clip are untouched. Hand-off window: loop weight 0 -> 1 at about v119-v141 (v4r7: v156-v183).
- Measured (frames.jsonl, both feet > 20 cm above the floor inside the hand-off window): before 13-14 frames, after 3-4
  (a run has a short flight phase). Actor x at v200: before 795.6, after 788.1.
- Previous verdict of v4r8a: PASS with note: skipped L step at blend-out v124-v134 (dati montage).

## The move (unchanged clip, only the phase curve differs in the copy) (owner 2026-11-20 decision A: "one high-knee pump then run, feet planted")
- v4r7 with the frantic section cut from R3 to R21.5 (same foot), the rest warped into ~0.27 s (rate0 1.38), then v4r7's normal
  strides. 39 f. The owner accepted that this changes identity trait 1 of v4r7 (one pump instead of several).
- Switch: gstn.RunStart.Variant 2.

## Identity traits (check FIRST)
1. ONE high-knee pump from the first frame, then normal run strides (no stomp, no standstill).
2. Feet planted throughout (no skate in the pump).
3. Arms at rest as v4r7 (hands not above the shoulder, no arm through the body), forward lean.


## Judge in this order
1. Identity traits first, then life on the mp4 without numbers: is the rev-up visible from the first frame (no pop from idle in
   the ~3 entry frames?), arc, weight, follow-through; then the hand-off into the run loop: every step planted, no skipped,
   double or floating step, no hop.
2. Compare with the top row (before) on the same frames: is each earlier FAIL/note item fixed? Any NEW defect from the fast entry?
3. Root vs capsule: feet sliding per plant (metrics). Loop plants of the stock run loop slide ~20-30 cm in BOTH captures
   (inherited, not this fix).
4. Technical last.
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco. Write it to verdict.md in this folder (max 25 lines).
