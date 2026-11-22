# Verdict run start v4r8bh: FAIL (narrow; the hand-off fix itself is confirmed)

Identity traits
1. Wheel from frame 1, no contact f1-f7: OK for v84-v88, KO for v89-v95 (diluted, see item 1). Feet stay >= 16 cm up v83-v97.
2. First contact clean into strides: OK. L plant v98-v104 moves 6 cm while the actor moves 30 cm.
3. Arms at rest, forward lean, no stomp/standstill: KO in part. Torso goes lean (v84-v90) -> upright (v91-v95) -> lean (v97+).

Fixed versus v4r8b (same frames)
- Hidden wheel: fixed. Start clip is on screen at v84 (weight 0.69, clip t 0.033); before it was 0 until v90.
- Stutter step at the hand-off: fixed. Contacts R v113, L v132, R v149 (19 and 17 frames), one foot each, root z flat, no hop.
  Both feet > 20 cm: 3 frames (v126-v128); before the feet hung at 20-30 cm for about 10 frames (v124-v134).

FAIL items
1. dati montage / gioco, v87-v95: logged start-clip weight sags 0.82 (v85) -> 0.50 (v90) -> 0.87 (v95), 1.00 only at v99.
   On video the torso straightens and the second half of the wheel loses amplitude (R foot flat at 25-27 cm v92-v94).
   The OK assert only covers v97-v115. The log lists the top 3 anims only, so what fills the other half is not verified.
2. posa, v113-v118: R landing skim still there, foot slides 130 -> 147 cm at z 14 -> 10 (17 cm). Clip untouched, as the brief says.

Notes, not failed
- Entry v82-v84: idle to feet 22/37 cm up in one frame, R foot 66 cm at v84. Reads as instant, no anticipation; accepted per owner.
- Actor x jumps +10 at v104 and +16 at v128 (about 7 per frame around them); v4r8b has the same at v126. Capture hitch, inherited.
- Not checked: grid_az0.png and grid_game.png not opened (token budget); judged on side view from the mp4 and grid_az90.

What the owner will notice: the stutter into the run is gone and the wheel now starts at once, but halfway through the wheel
the body pops upright for about 5 frames before leaning in again, and the first right landing still skims.
