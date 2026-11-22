# Review brief: run start v4r8bh = v4r8b + hand-off fix (one move, rule 10)
Branch claude/runstart-phase-desktop. Files here: grid_az90/az0/game.png (top = v4r8b BEFORE the fix, bottom = v4r8bh AFTER, SAME
frames v84-v144 step 4, same kit/sequence), runstart_v4r8bh.mp4 (v60-v240), metrics_summary.json, metrics_v4r8b.json / metrics_v4r8bh.json
(per-frame feet, actor). Capture ASSERT: Saved/RunStartPhase/v4r8bh/assert.txt = OK (clip weight >= 0.8 on 100 % of v97-v115).

## What changed (systemic, the same for every run start; cvar gstn.RunStart.Handoff 1)
- Entry: the start now enters on its FIRST frame (before: at clip time 0.133 s, after ~6 frames of a stock walk start) with a
  0.05 s blend-in (before 0.3 s QuadraticInOut). Owner: the reference enters the start almost instantly.
- Exit into the run loop: the clip copy carries the stock `phase` curve, which the loop's pose search reads to choose the loop
  foot. Before, the loop was entered on the other foot and the 0.5 s crossfade averaged two feet. Pose, root motion and timing
  of the clip are untouched. Hand-off window: loop weight 0 -> 1 at about v119-v141 (v4r7: v156-v183).
- Measured (frames.jsonl, both feet > 20 cm above the floor inside the hand-off window): before 13-14 frames, after 3-4
  (a run has a short flight phase). Actor x at v200: before 795.6, after 795.6.
- Previous verdict of v4r8b: FAIL: wheel f1-f7 hidden by 0.3 s blend-in (weight 0 until v90); blend-out v118-v130 out of phase -> stutter step; minor posa R landing skim v112-v116.

## The move (unchanged clip, only the phase curve differs in the copy) (owner 2026-11-20 decision B: frantic scramble as a cartoon WHEEL, Luigi blur)
- v4r7's frantic pumps warped into 8 frames (rate 6 at frame 0), and in clip f1-f7 the feet are INTENTIONALLY kept off the ground
  (ball >= 12 cm): the legs spin like a wheel in the air while the body starts. Feet planted again from the first normal contact
  (clip f8). THE AIRBORNE SCRAMBLE IS INTENDED: do not fail it as skating; judge it as a cartoon wheel.
- Switch: gstn.RunStart.Variant 3.

## Identity traits (check FIRST)
1. Frantic multi-pump scramble from frame 1, legs blurring like a wheel, no ground contact f1-f7.
2. First real contact lands cleanly into normal strides (no slide after clip f8, ~v101+).
3. Arms at rest as v4r7, forward lean, no stomp or standstill.


## Judge in this order
1. Identity traits first, then life on the mp4 without numbers: is the rev-up visible from the first frame (no pop from idle in
   the ~3 entry frames?), arc, weight, follow-through; then the hand-off into the run loop: every step planted, no skipped,
   double or floating step, no hop.
2. Compare with the top row (before) on the same frames: is each earlier FAIL/note item fixed? Any NEW defect from the fast entry?
3. Root vs capsule: feet sliding per plant (metrics). Loop plants of the stock run loop slide ~20-30 cm in BOTH captures
   (inherited, not this fix).
4. Technical last.
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco. Write it to verdict.md in this folder (max 25 lines).
