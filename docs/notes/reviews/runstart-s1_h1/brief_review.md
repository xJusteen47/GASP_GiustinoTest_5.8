# Review brief: generic run start s1 "sprinter drive" (one move, rule 10, round 1)
Branch claude/runstart-generic-desktop. Files here: grid_az90.png, grid_az0.png, grid_game.png (top row generic_h1 = STOCK GASP run
start = base, bottom row s1, SAME capture frames v84-v180 step 4, 60 fps, same kit/sequence), runstart_s1_h1.mp4 (variant run),
../runstart-generic_h1/runstart_generic_h1.mp4 (base run), casc_revup_f0-23.png (Cascadeur, clip frames 0-23 at 30 fps, side;
NOTE: this sheet is the first pass with +40 deg extra lean, the built clip uses +28 deg and a 14 cm pelvis drop; auto camera drifts),
metrics_summary.json. Sibling variant g3 (PASS): ../runstart-g3_h1/review_fable.md.

## What it is (owner queue, 2026-11-21)
Alternative approach for the same generic start (average Fortnite-style character, stock GASP gait, ends on the stock loop
M_Relaxed_Run_Loop_F, stock root motion). Source: an Antigravity/Gemini proposal: sprinter start, first 4 frames pelvis down 10-15 %,
torso forward 35-45 deg, elbows back; then 2 short quick drive steps "ta-ta-TUM" with a low centre of mass, opening into the loop.
Built: clip f0-3 load (play rate 0.8x -> 2.3x), pelvis -14 cm (about -11 % of 89.5 cm) f0-4 held to f10, released by f17; torso
40-46 deg from vertical f4-f12 (+28 deg extra about the pelvis), easing out to f18; hands/elbows drawn 10 cm back; arm swing x1.7;
rate 2.3x held to f10 (two short steps), back to 1x by f15 (the long "TUM" stride), then the stock loop frame for frame.

## Identity traits (check FIRST, rule 13a)
1. Sprinter drive: low, leaning body with elbows back in the first frames (pelvis down, torso 35-45 deg).
2. Rhythm "ta-ta-TUM": two short quick low steps, then one long opening stride into the run.
3. No clash: idle -> start without a pop; start -> loop on the same poses, no hitch, no wrong-foot step.
Also the owner's general rule for every start: limbs alive from frame 1 (no dead planted pause).

## Judge in this order (rule 10)
1. Life on the mp4 without numbers (anticipation, arc, weight, follow-through, root motion coherent with the capsule).
   Does the low drive read as a sprinter / cartoon dash, or as a stumble / crouched glide? Compare with the base and with g3.
2. Feet vs ground: planted-feet travel f80-240 (metrics_summary.json): base L/R 43.3/93.5 cm, g3 114.1/71.8, s1 92.2/71.8.
   Per-contact L (foot z < 13 cm): s1 10.0 / 4.7 / 29.1 / 28.7 / 26.9 / 25.0 cm (base max 27.1, g3 max 33.3).
3. Technical: ASSERT OK (AS_GSTN_RunStart_Generic_s1 weight >= 0.8 on 100 % of f97-140, min 1.00). Travel f0-200 1007.4 cm
   (base 1017.0). Known game item (code queue, not a pose FAIL): first step half blended with idle (entry blend 0.1 s + lean layer).
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco, with the frames. Write review_fable.md here.
