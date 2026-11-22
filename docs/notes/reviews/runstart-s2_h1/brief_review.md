# Review brief: generic run start s2 "sprinter drive, longer push" (one move, rule 10, polish round of s1)
Branch claude/runstart-generic-desktop. Files here: grid_az90.png, grid_az0.png, grid_game.png (top row generic_h1 = STOCK GASP run
start = base, bottom row s2, SAME capture frames v84-v180 step 4, 60 fps, same kit/sequence), runstart_s2_h1.mp4 (variant run;
mp4 frame n = capture frame v(n+60), the frame label is burnt in), strip_az45_v86-120.png and strip_az90_v86-120.png (every 2nd
video frame v86..v120, row 1 = v86..v102, row 2 = v104..v120), metrics_summary.json. Sibling s1 (PASS narrow): ../runstart-s1_h1/
review_fable.md (read only its item 3 and the "Thread look" paragraph).

## What changed from s1 (owner 2026-11-21: "anche la 8 non è male, fai il giro di rifinitura")
s1 read right but fast (~0.1 s per "ta") with elbows forward-down. s2 = s1 recipe with: play rate 2.3x -> 2.0x and held to clip f12
(not f10); pelvis -14 cm held to f12, released by f19 (not f17); torso ease-out to f20; elbows/hands back 10 -> 14 cm; 3 frames longer
clip (63 f). Measured in the pose file: torso 42-46 deg from vertical f4-f14 (s1: dropped to 40 at f12, 32 at f14), pelvis ~80 cm.
Knees rebuilt forward by two-bone IK (0 backward frames in Cascadeur, min 6.4 cm ahead of the hip-foot line).

## Identity traits (check FIRST, rule 13a)
1. Sprinter drive: low, leaning body with elbows back in the first frames (pelvis down, torso 35-45 deg).
2. Rhythm "ta-ta-TUM": two short quick low steps, then one long opening stride into the run. Should now feel less rushed than s1.
3. No clash: idle -> start without a pop; start -> loop on the same poses, no hitch, no wrong-foot step.
Also the owner's rule for every start: limbs alive from frame 1 (no dead planted pause); knees never bend backward.

## Judge in this order (rule 10)
1. Life on the strips and on the mp4 without numbers (anticipation, arc, weight, follow-through, root vs capsule). Look at v86-v120
   frame by frame (strips), then v140-v180 in the mp4 (mp4 frames 80-120) for the hand-off to the loop. Is the push longer and
   readable? Elbows back? Any stumble / crouched glide?
2. Feet vs ground (foot z < 13 cm, f80-240): per-contact travel L 8.0 / 5.8 / 31.9 / 32.4 / 26.4 / 24.8 cm, R 3.4 / 19.0 / 24.1 /
   22.5 / 31.5 / 22.5 (s1: L max 28.6, R max 31.5; base L max 26.9, R one 103 cm skid). Planted travel L/R 92.3 / 86.7 (s1 92.2/71.8).
3. Technical: ASSERT OK (s2 weight >= 0.8 on 100 % of f97-140, min 1.00). Travel f0-200 1017.0 cm (= base). Known game item (code
   queue, not a pose FAIL): first step half blended with idle (weight 0.29/0.5/0.87 at v85/90/95).
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco, with frames; say s2 vs s1 in one line. Max 8 extra images.
Write review_fable.md here (max 40 lines).
