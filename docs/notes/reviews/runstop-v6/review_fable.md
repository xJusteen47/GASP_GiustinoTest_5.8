FAIL - twist fixed, but after the clip the whole body still glides 3-5 cm sideways for 35+ frames (feet slide, no step), and one arm still drops back alone
Capture Saved/RunStop6/cap1 (reviewer Fable, rule 10). Sources: 2 grids, 8 single frames (mv_run_stop game v215/252, az90 v228/244/279;
base game v91, az45 v91, az90 v113), both frames.jsonl, run_stop_L/R_bones.json (yaw from upperarm, thigh and foot-ball lines).
Clips: mv_run_stop = run_stop_R (right foot in front), f0 = v184, STOP f31 = v215, end f68 = v252. mv_run_stop_base = run_stop_L,
f0 = v60, STOP = v91, end = v128. Game = bones file within 1.2 cm (R, constant x offset) / 0.5 cm (L) on f50-f68: all keys play.

Identity traits
- Trait 1 (front foot planted, facing forward, no turn): OK, with a note. Front foot planted v188-224 (R) / v66-100 (L), its yaw is
  constant while planted (32.6 / 14.2 deg). Actor does not turn. Clip end is square: shoulders 0.4, hips 0.0 deg in both
  (V5: 24 / 15). v252 and v279 az90 are a clean profile. Note: see 3, the body un-turns about 25 deg while it rises.
- Trait 2 (torso rises only after the feet stop, arms open low): OK. Slide ends v215 / v91; rise v219-240 (head z 42.0 -> 62.9) /
  v95-117. Hands 36-44 cm out to the sides and below the pelvis through the brake.
- Trait 3 (soft parts last): OK. R: pelvis home v242, head goes back to 1.7 then settles 3.5 at v245, hands v249-250.
  L: pelvis v118, head v121, hands v125-126.

V5 items
- Item 1, twist at the clip end: FIXED (hands fore-aft 3.3 / 2.0 cm, V5 +12 / -17.6), no pops. Ooze after the clip: NOT FIXED, see 1.
- Item 2, single arm: NOT FIXED, see 2. The overshoot behind the body is gone (V5 hand R reached -17.4, now it stops at +3.2).

gioco (main FAIL)
1. Still for 2-4 frames (R v250-253), then everything drifts to +y with the feet flat on the floor. R v254-279: foot R
   (-10.0,14.7) -> (-11.7,17.6) = 3.4 cm, foot L 2.2, hand R 2.3, head 1.9, still 0.2 cm/frame at v279. L v126-154: foot R 4.3 cm,
   foot L 2.9, hand R 3.8, head 2.6, slowing. Limit was 2 cm. Target is the idle at rest in THIS capture (main v0-20, feet still):
   feet (1.8,-11.8) / (-11.4,19.2), pelvis (-0.6,3.9,3.7), head (4.4,5.0,62.0), hands (3.3,-21.4,-3.1) / (2.1,30.6,-3.9).
   The V5 capture rested on feet (1.3,-15.3) / (-8.8,14.7): the game has two rest poses about 4 cm apart sideways (nearly rigid shift,
   mesh_rel root constant at 0,0,-88). The clip ends on the V5 one. A measured pose cannot fix this until the cause is known.
posa
2. One arm drops back alone after the trunk is home. R: hand L x 22.7 -> 3.3 on v240-249 (19 cm, 9 frames) while hand R moves 7.
   L: hand R goes forward 15 -> 26 during the rise (v95-113, forearm lifted at az90 v113), then 25.2 -> 3.2 on v116-125 (23 cm in
   9 frames, 3.4 cm/frame peak). Not mirrored (R has no forward swing). Reads as a second small gesture, less than V5.
3. New from the additive, with V5 item 3 (open): the brake pose is turned right in BOTH clips, not mirrored. R: shoulders 26.4,
   hips 7.5 deg at STOP. L: hips 23.5, shoulders 16, rear foot 59 deg toe-out (az45 v91: chest and hips open to one side).
   The ramp f31-f58 removes it during the rise: R shoulders 26 -> 0, L hips 23.5 -> 0 on 27 frames, up to 1.4 deg/frame, smooth,
   no pop. It reads as squaring up, not as a turn, but in L the whole lower body rotates over a planted front foot (v91-100).
4. V5 item 4, open: v216-233 rear foot 47 cm in 9 frames, then front foot back 38 cm in 9 frames while the trunk rises
   (az90 v228: knees together, a quick shuffle, light for this body). Not blocking.
dati montage
5. Planted foot jumps 0.6 cm at v234-235 (R clip f50-51) and 0.7 cm at v110-111 (L): not visible, clean the key.
gioco
6. Base v60 = v59 repeated frame, still there. Root offset step v203 (R, 2.8 cm) is continuous in world.
7. V5 item 7, open: dust puff at the feet v236-279 (az90 v244, v279); base v53-85 the 60 cm blob. Stick still not logged.
Next fix (one): find why the idle at rest is not one pose before touching the clip again. Capture 150 frames of plain idle after a
stop with the root bone (component space), the offset root node and the playing idle asset + time logged, plus one run without
the stop clip (gstn.RunStop.Mode 0). If the idle itself moves or the game picks another idle, fix that hand-off (or end the clip on
the pose the game picks, per case); then recheck that feet and hands move under 2 cm in the 40 frames after the clip.
