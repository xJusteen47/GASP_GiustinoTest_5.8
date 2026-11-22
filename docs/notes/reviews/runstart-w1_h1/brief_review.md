# Review brief: generic run start w1 "sprinter weight transfer, frantic limbs" (one move, rule 10)
Branch claude/runstart-generic-desktop. Files here: grid_az90.png, grid_az0.png, grid_game.png (top row generic_h1 = STOCK GASP run
start = base, bottom row w1, SAME capture frames v84-v180 step 4, 60 fps, same kit/sequence), runstart_w1_h1.mp4 (variant run;
mp4 frame n = capture frame v(n+60), the frame label is burnt in; tiles: top row az0 | az45 | az90 | game, bottom az180 | az270 |
az315), strip_az45_v86-120.png and strip_az90_v86-120.png (every 2nd video frame v86..v120, row 1 = v86..v102, row 2 = v104..v120),
curves-sheet.md (READ FIRST: what the sprinter's weight transfer is and what w1 should show). Sibling for comparison, the sprinter
s2 (PASS): ../runstart-s2_h1/strip_az45_v86-120.png and strip_az90_v86-120.png (same frames, same layout).

## What it is (owner 2026-11-21)
The owner prefers the sprinter for HOW the weight transfer happens. w1 is the general-purpose run start for an average
Fortnite-style character: the sprinter's weight transfer (fall down-and-forward first, low hold through two drive steps, single rise
on the second push-off) with the frantic limb rev-up from frame 1 that every playable start must have, blending idle -> loop.

## Identity traits (check FIRST, rule 13a)
1. Sprinter weight transfer: the body FALLS down and forward in the first ~4 frames (pelvis down, torso ~40 deg), stays low through
   two quick drive steps, and rises only on the long opening stride. It must not read as a lean on the spot nor as a hop.
2. Frantic limbs from frame 1: legs and arms already pumping fast on the first frames (no slow load, no planted pause).
3. No clash: idle -> start without a pop; start -> loop on the same poses, no hitch, no wrong-foot step.
Also the owner's rule: knees never bend backward.

## Judge in this order (rule 10)
1. Life on the strips and on the mp4 without numbers (anticipation, arc, weight, follow-through, root vs capsule). Strips v86-v120
   frame by frame, compare with the s2 strips (same frames); then v136-v180 in the mp4 (mp4 frames 76-120) for the hand-off.
   Is the weight transfer the sprinter's? Is it frantic from the start? Too fast / churning / skating?
2. Feet vs ground (runs with foot z < 13 cm, f80-240, horizontal path per run; one-off on metrics_*_h1.json):
   w1 L 14.3 (idle + push foot, v80-~v92) / 20.5 / 25.6 / 28.3 / 29.3, R 1.1 / 11.3 / 33.9 / 28.8 / 24.7 cm.
   s2 L 9.6 / 6.0 / 32.3 / 32.5 / 26.6 / 25.0, R 5.3 / 19.1 / 24.3 / 22.6 / 31.6 / 22.7. Base L max 27.1, R one 103.7 skid.
3. Technical: ASSERT OK (w1 weight >= 0.8 on 100 % of f97-140, min 1.00). Travel f0-200 1017.0 cm (= base). Known game item (code
   queue, not a pose FAIL): first step half blended with idle (weight 0.29/0.5/0.87 at v85/90/95), same as s2.
Verdict PASS/FAIL; each FAIL item labelled posa / dati montage / gioco, with frames; say w1 vs s2 in one line. Max 8 extra images.
Write review_fable.md here (max 40 lines).
