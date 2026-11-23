# Review brief: generic run start TURNAROUND 180 (w2 family), in-game capture (rule 10)

Move: start from idle with the stick pulled BACK (about 180 deg from the facing). GASP stock plays a "reface" start
(M_Relaxed_Run_Reface_Start_180_L/R: the root yaw goes 180 -> 0 in ~0.6 s while the body runs out in the new direction).
Our clip AS_GSTN_RunStart_Generic_w2t180L (and the mirror ...t180R) replaces it on gstn.RunStart.Variant 21 (default):
same root path and root yaw curve as stock, body = the approved forward start w2 (owner: "new default") composed in the turning root frame.
Base = the stock reface start (top row of every grid), variant = ours (bottom row), same capture, same game frames.

## Identity traits (judge FIRST, rule 13a)
1. Sprinter weight transfer INTO the new direction: within the first ~4-6 clip frames the body turns and FALLS down-and-forward toward
   the run direction (pelvis about 8-10 cm lower, torso ~40 deg toward the new heading), stays low through two drive steps, rises once
   on the long stride. (w1/w2 reference: fall in 4 frames, hold, one rise.)
2. Frantic limbs from frame 1 (cadence ~2.3x the run loop), no planted pause, no stiff spin on a planted foot.
3. No clash idle -> start -> run loop (no pop, no hop, no double step at the hand-off). Knees never bend backward.

## Then life (anticipation, arc, weight, follow-through, root/capsule coherence), then technical checks.
Owner style bar: cartoony and fluid, like a Nintendo / Astro Bot character, readable at gameplay distance.

## Files (all in docs/notes/reviews/)
- runstart-t180a2_h1/grid_game.png, grid_az90.png, grid_az0.png: L turn; columns = game frames v084..v180 every 4 (30 Hz rows of
  a 60 Hz capture). Our clip enters at ~v84 (clip time 0.05 s), full weight from ~v96, hand-off to the loop ~v120-140.
- runstart-t180b2_h1/grid_az90.png: the R mirror (check only that it mirrors L and has no extra defect).
- runstart-t180a2_h1/runstart_t180a2_h1.mp4: only if a grid leaves a doubt. mp4 frame n = capture frame v(n+60).
- Measured (runstart_contact_slide.py on the metrics json, foot below 13 cm, f80-240), cm per contact:
  ours L 10.6 (entry pivot) / 5.2 / 38.3 (long-stride landing) / 10.1 / 5.9, R 0.1 / 6.9 / 33.4 / 28.2 / 6.1;
  stock base L 18.8 / 12.9 / 29.6 / 6.7 / 10.7, R 12.7 / 34.4 / 21.2 / 6.7 / 18.5. Approved forward w2 had 20 cm at its long-stride landing.

## Output
Write runstart-t180a2_h1/review_fable.md: verdict PASS or FAIL, the 3 traits one by one (frame numbers), then life notes, then each
FAIL / watch item labelled posa (pose) / dati montage (clip data) / gioco (game code) with frames. Max ~40 lines. Look at each grid once;
at most 8 extra images (zoom crops or mp4 frames) and only around the entry (v84-v104) and the hand-off (v112-v140).
