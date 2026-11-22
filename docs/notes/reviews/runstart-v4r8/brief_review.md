# Review brief: run start v4r8 (one move, rule 10)
Branch claude/runstart-v8-desktop. Files here: grid_az90.png, grid_az0.png, grid_game.png (top row v4r7 = approved base,
bottom row v4r8, SAME capture frames v84-v144 step 4, 60 fps), runstart_v4r8.mp4 (v60-v240), casc_revup_f0-12.png (Cascadeur,
first 13 clip frames), metrics_summary.json, ref/ (Luigi grids).

## What changed (owner ask: "measure Luigi's run start and shorten ours to match")
- v4r7 rev-up (frantic pumps before the first normal stride) = 0.95 s. v4r8 = one frantic stride cut, the rest warped into 7 f
  (0.23 s at 30 fps, Luigi 0.20-0.27 s), tail (normal strides) identical. Clip 38 f; DA stage 1 speed ramp 30 -> 14 sim frames.
- In game the clip owns ~v95-v118 and blends into the run loop by v130 (v4r7 held it to ~v170).

## Identity traits (check FIRST, rule 13a)
1. Frantic, scrambling legs from the very first frame (engine revving), then normal run strides.
2. Arms at rest as approved in v4r7 (hands not above the shoulder; no arm through the body).
3. Forward lean into the start, no standstill or stomp.

## Judge in this order (rule 10)
1. Life on the mp4 without numbers: anticipation, arc, weight, follow-through; does the 7-frame rev-up read as a scramble or as a
   pop/jitter? Knee or foot pop at the cut (clip f3-f5, ~v99-v103)? Hand-off into the loop (v115-v130) smooth?
2. Root motion vs capsule: feet sliding. Number to explain: planted-feet travel f80-240 L/R 46.6/45.0 cm (v4r7 36.7/33.3).
   The detector found no foot contacts inside the 7 rev-up frames (first contact clip f7.5): feet may skate during the scramble.
3. Technical: ASSERT OK v4r8 f97-115 100 %, min 1.00 (Saved/RunStartV8/v4r8/assert.txt).
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco. If feet slide: brief step 6 = CUT (0,0), TARGET_S 0.27.
