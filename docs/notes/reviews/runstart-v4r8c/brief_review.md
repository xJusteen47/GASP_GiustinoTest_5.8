# Review brief: run start v4r8c (one move, rule 10)
Branch claude/runstart-v8c-desktop. Files here: grid_az90.png, grid_az0.png, grid_game.png (top row v4r7 = approved base, bottom
row v4r8c, SAME capture frames v84-v144 step 4, 60 fps, same run), runstart_v4r8c.mp4 (v60-v240), casc_revup_f0-11.png
(Cascadeur, first 12 clip frames at 30 fps), metrics_summary.json. Reference grids of Luigi: ../runstart-v4r8/ref/.

## What it is (owner 2026-11-20: "in Mario the start is exactly the run clip, just sped up")
- No special start poses: one cycle of v4r7's loop-matched run (clip f28-46) played at 3.5x at frame 0, rate decaying linearly to
  1.0 over 8 frames (0.27 s), then v4r7's normal strides 1:1. Root forward speed follows the DA ramp (0.15 -> 1), strides scaled
  down offline (to 0.04 at frame 0) so the planted foot stays planted: short fast steps, knees as high as in the run.
- Switch: gstn.RunStart.Variant 1 (0 = v4r7 default). DA ramp unchanged (stage 1 30 f / 0.15).

## Identity traits (check FIRST, rule 13a)
1. Reads as the normal run cycle sped up at the start (no pumps, no stomp, no special pose), cadence slowing into the run.
2. Feet planted on contact: short scrambling steps, not a treadmill slide.
3. Arms and lean as the run (v4r7 tail), no pop when the rate reaches 1.0 (~clip f8, ~v101-v105) or at the hand-off to the loop.

## Judge in this order (rule 10)
1. Life on the mp4 without numbers: does the sped-up start read as energy (rev-up) or as a glitch/fast-forward? Blend from idle
   into a mid-stride pose (clip f0 is a run pose, the blend from idle is the stock 0.3 s)?
2. Root vs capsule: feet sliding (metrics_summary.json planted-feet travel vs v4r7).
3. Technical: ASSERT in Saved/RunStartV8c/v4r8c/assert.txt.
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco.
