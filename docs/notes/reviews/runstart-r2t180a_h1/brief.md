# Review brief r2: generic run start TURNAROUNDS (180 and 90, L and R), w2 family, in-game captures (rule 10), pose round 2 of 3

Move: start from idle with the stick pulled back (180) or sideways (90). GASP stock plays a "reface" start (root yaw 180/90 -> 0 in
~0.6 s while the body runs out in the new direction). Our clips AS_GSTN_RunStart_Generic_w2t{180,090}{L,R} replace them on
gstn.RunStart.Variant 21 (default): same root path and root yaw as stock, body = the approved forward start w2 composed in the turning
root frame. Base = the stock reface start (top row of each grid), variant = ours (bottom row), same game frames.

Round 1 (180 L/R) was FAIL, trait 1: the lean pointed at the final heading while the chest still faced back, so at v88-v96 the body
tipped BACKWARD (head thrown back, slip / limbo fall). Round 2 change: the chest leads the turn (45 deg ahead of the root yaw, head
20 deg more, ramped in over the first 2 clip frames = anticipation) and the lean tips along the CHEST facing, fading in only once the
chest has less than ~120 deg left to turn: the 180 should read sink upright -> turn (chest/head first) -> fall forward. The 90 has the
chest almost on the new heading from the start, so it should fall forward almost at once. Everything else unchanged (traits 2/3 passed).

## Identity traits (judge FIRST, rule 13a)
1. Sprinter weight transfer INTO the new direction: body sinks at once, turns (chest/head leading) and FALLS down-and-forward toward
   the run direction (torso ~40 deg, pelvis ~8-10 cm low), stays low through two drive steps, rises once on the long stride.
   Never a backward tip (centre of mass behind the support).
2. Frantic limbs from frame 1, no planted pause, no stiff spin on a flat planted foot.
3. No clash idle -> start -> run loop (no pop, hop, double step). Knees never bend backward.
Then life (anticipation, arc, weight, follow-through), then technical checks. Owner bar: cartoony and fluid (Nintendo / Astro Bot),
readable at gameplay distance.

## Files (docs/notes/reviews/)
- runstart-r2t180a_h1/grid_az90.png, grid_az0.png, grid_game.png: 180 L (main judgement). Columns v084..v180 every 4; our clip
  enters ~v84, full weight ~v96, hand-off ~v120-140. mp4 runstart_r2t180a_h1.mp4 only if needed (mp4 frame n = capture frame v(n+60)).
- runstart-r2t180b_h1/grid_az90.png: 180 R (mirror check).
- runstart-r2t090a_h1/grid_az90.png and grid_az0.png: 90 L. runstart-r2t090b_h1/grid_az90.png: 90 R (mirror check).
- For comparison with round 1 (only if useful): runstart-t180a2_h1/grid_az90.png and review_fable.md there.
- Measured contact slides (cm per contact, foot < 13 cm, f80-240; runstart_contact_slide.py):
  180 L ours 10.4 / 5.9 / 38.2 / 21.6, stock 18.8 / 12.9 / 29.6; 180 R ours 0.1 / 3.9 / 30.4 / 29.7, stock 12.1 / 10.6 / 35.5;
  90 L ours 11.6 / 2.9 / 38.2 / 21.6, stock 67.4 / 29.4 / 22.3; 90 R ours 1.3 / 2.8 / 30.8, stock 57.7 / 22.9 / 14.7.
  The 30-38 cm is the long-stride landing at the hand-off (~v131-139); approved forward w2 had 20 there, stock 29.6-35.5.

## Output
Write runstart-r2t180a_h1/review_fable.md: verdict PASS or FAIL for the 180 and for the 90 (separately), the 3 traits with frames,
life notes, then each FAIL / watch item labelled posa / dati montage / gioco with frames. Max ~45 lines. Budget: each listed grid
once, at most 6 extra crops, only around entry (v84-v108) and hand-off (v112-v140). Keep the whole review under ~80k tokens.
