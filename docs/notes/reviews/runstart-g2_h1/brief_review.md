# Review brief: generic run start g2 (one move, rule 10)
Branch claude/runstart-generic-desktop. Files here: grid_az90.png, grid_az0.png, grid_game.png (top row generic_h1 = STOCK GASP run
start = base, bottom row g2, SAME capture frames v84-v180 step 4, 60 fps, same kit/sequence), runstart_g2_h1.mp4 (variant run),
../runstart-generic_h1/runstart_generic_h1.mp4 (base run), casc_revup_f0-24.png (Cascadeur, clip frames 0-24 at 30 fps, side),
metrics_summary.json.

## What it is (owner 2026-11-21)
Generic start for an "average Fortnite-style" character on the stock GASP gait, BEFORE any Overlord variant. Owner rule: a frantic
limb rev-up ("engine revving", Mario / Astro Bot) on every playable character, from frame 1, no planted pause; the hard part is not
to clash with the idle before and the loop after. Owner: care about the animation, not the speed (it must read right on its own,
also on ice). Built from the stock loop M_Relaxed_Run_Loop_F (the loop the game plays): 3-frame crossfade from the stock start's
f0 pose (idle entry), then the loop at a play rate falling from 3.2x to 1x over 16 frames (3 steps in 0.53 s), strides shortened
to the slow stock root ramp (feet churn a bit, high knees), then the stock loop frame for frame.

## Identity traits (check FIRST, rule 13a)
1. Frantic rev-up from frame 1: arms and legs pump fast at once, no planted pause, cadence visibly faster than the run.
2. Engine revving: short quick steps with high knees that settle into the normal stride (some foot spin is intended, a long
   treadmill slide is not).
3. No clash: idle -> start without a pop; start -> loop on the same poses, no hitch, no wrong-foot step.

## Judge in this order (rule 10)
1. Life on the mp4 without numbers (anticipation, arc, weight, follow-through, root motion coherent with the capsule): does the
   rev-up read as energy or as fast-forward / glitch? Compare with the base (stock: planted pause, smooth mocap accel).
2. Feet vs ground: planted-feet travel f80-240 (metrics_summary.json): base L/R 43.3/93.5 cm, g2 102.9/78.6.
3. Technical: ASSERT OK (AS_GSTN_RunStart_Generic_g2 weight >= 0.8 on 100% of f97-140, Saved/RunStartGeneric/g2_h1/assert.txt).
   Note: the capture's clip weight ramps over ~0.27 s at entry because of the lean layer (known, not a second instance).
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco, with the frames.

## Round 2 (g1 review FAIL posa, ../runstart-g1_h1/review_fable.md)
Changes vs g1: rev 21 frames (was 16), rate 2.6x -> 1x (was 3.2x); during the rev (envelope 0 at frame 0, full by frame 5, 0 at frame 21): pelvis vertical bounce x0.3 (hop), +22 deg forward drive lean of the upper body about the pelvis, arm swing x1.7 about the loop mean. Not applied: 'stride floor held low until the last 4 frames' (it would add foot slide; the stride follows the root speed). Check that the g1 FAIL items are fixed and that nothing new broke.
