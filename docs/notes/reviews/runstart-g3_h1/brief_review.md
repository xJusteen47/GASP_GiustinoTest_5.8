# Review brief: generic run start g3 (one move, rule 10, posa round 3 of 3)
Branch claude/runstart-generic-desktop. Files here: grid_az90.png, grid_az0.png, grid_game.png (top row generic_h1 = STOCK GASP run
start = base, bottom row g3, SAME capture frames v84-v180 step 4, 60 fps, same kit/sequence), runstart_g3_h1.mp4 (variant run),
../runstart-generic_h1/runstart_generic_h1.mp4 (base run), casc_revup_f0-23.png (Cascadeur, clip frames 0-23 at 30 fps, side;
the auto camera drifts, the character leaves the frame), metrics_summary.json. Previous round: ../runstart-g2_h1/review_fable.md.

## What it is (owner 2026-11-21)
Generic start for an "average Fortnite-style" character on the stock GASP gait. Owner rule: a frantic limb rev-up ("engine
revving", Mario / Astro Bot) on every playable character, from frame 1, no planted pause; it must not clash with the idle before
and the loop M_Relaxed_Run_Loop_F after. Owner: care about the animation, not the speed. Built from the stock loop: entry crossfade
from the stock start f0 pose, then the loop at a raised play rate, strides shortened to the stock root ramp, then the stock loop
frame for frame (stock root motion).

## Identity traits (check FIRST, rule 13a)
1. Frantic rev-up from frame 1: arms and legs pump fast at once, no planted pause, cadence visibly faster than the run.
2. Engine revving: short quick steps with high knees that settle into the normal stride (some foot spin is intended, a long
   treadmill slide is not).
3. No clash: idle -> start without a pop; start -> loop on the same poses, no hitch, no wrong-foot step.

## Round 3 changes vs g2 (g2 FAIL: rev front-loaded, dead by clip f12; entry drop in 2 frames)
- Push PLATEAU: lean +22 deg, bounce x0.3 and arm swing x1.7 at full strength from clip f4.5 to f14, ease out f14-f21 (g2 decayed
  from f5). Play rate 2.6x held near full (loses only 30 % of the extra by f14), back to 1x at f21.
- Softer entry: 5-frame crossfade (was 3), pelvis and legs finish 1.5 frames before the chest.
- High knees on steps 2-3: swing-leg hip flexion +25 deg (shin folds back) in clip f4-f13.
Clip f14 = capture ~v84 + 28 (60 fps); the rev should still read as revving through ~v110-115.

## Judge in this order (rule 10)
1. Life on the mp4 without numbers (anticipation, arc, weight, follow-through, root motion coherent with the capsule). Does the
   rev now last to clip f14 and release smoothly into the loop? Does it read as energy or as fast-forward / glitch?
2. Feet vs ground: planted-feet travel f80-240 (metrics_summary.json): base L/R 43.3/93.5 cm, g2 102.9/78.6, g3 114.1/71.8.
3. Technical: ASSERT OK (AS_GSTN_RunStart_Generic_g3 weight >= 0.8 on 100% of f97-140, min 1.00).
   Known game item (already in the code queue, not a pose FAIL): first step half blended with idle (entry blend 0.1 s + lean layer).
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco, with the frames. Write review_fable.md here.
