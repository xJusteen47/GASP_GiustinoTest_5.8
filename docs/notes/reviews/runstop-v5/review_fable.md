FAIL - the clip ends on a pose that is not the game idle: the body turns about 24 deg to its right during the rise, then oozes back for 35+ frames
Capture Saved/RunStop5/cap1 (reviewer Fable, rule 10). Sources: 2 grids, 7 single frames (mv_run_stop az90 v0/215/240/252/279,
game v252; base az90 v117), both frames.jsonl, run_stop_L/R_bones.json.
Clips: mv_run_stop = run_stop_R (right foot in front), clip f0 = v184, STOP f31 = v215. mv_run_stop_base = run_stop_L, f0 = v60,
STOP = v91. Game matches the bones files to 0.1 cm on f36-f65 in both, so every late key plays.

Identity traits
- Trait 1 (front foot planted, facing forward, no turn): FAIL. Front foot planted v188-224 (R) / v66-100 (L): OK.
  The ACTOR does not turn and the camera is still from v216 (actor y constant, body = clip to 0.1 cm in actor space).
  The turn is in the POSE: the clip end (f66-68) has shoulders 24 deg right, hips 15 deg right, right foot 39 deg toe-out,
  hand L +12 / hand R -17.6 cm fore-aft. The game idle (v0-20) is square: hands +4.0 / +3.4. R clip: hips 8 -> 15 deg and
  right foot 33 -> 39 deg on v215-240; once the trunk is upright (v232-252) the chest faces the az90 camera (v240, v252).
- Trait 2 (torso rises only after the feet stop, arms open low): OK. Slide ends v215; rise v219-240 (head z 41.5 -> 61.5).
  Hands 37 cm out to the sides and low through the brake (v190-218).
- Trait 3 (soft parts last): OK. R: pelvis home v242, head overshoot -0.2 -> 1.5 settles v245-246, hand L v249, hand R v250.
  L: pelvis v118, head v121, hand R v125.

V4 FAIL items
- Late keys play, no still block: FIXED. Still frames are only v250-253 (R) and v125 (L). V4 had 20.
- Push eased in: FIXED. Head x per frame v216-218 = 1.2, 2.2, 1.0 cm (V4: 3.9 in one frame). No tick.
- Short tail: FIXED (69 frames, 2-3 still).
- No creep into the idle: NOT FIXED, see 1.

dati montage (generator, main FAIL)
1. run_stop_gen.py idle = GASP M_Neutral_Stand_Idle_Loop frame 0 (line 58). The game idle is a different pose. After the clip
   the game blends for 35+ frames: R v254-279 foot L slides (5.0,-20.0) -> (2.5,-16.9) and is not done (idle is 1.3,-15.3,
   6 cm total), foot R 4 cm, hand R -17.6 -> -5.0 (idle +3.4, 21 cm), hand L 12 -> 6, pelvis z +1.6. Same on L v126-154.
   v279 az90 is still half twisted. Feet slide on the floor with no step.
posa
2. Second gesture, caused by 1: one arm swings back alone after the trunk is home. R: hand L 34.7 -> 12.0 cm on v240-249.
   L: hand R +9.5 -> -17.4 cm on v116-125 (27 cm in 9 frames), then the game brings it forward again by 21 cm. Reads as a
   wobble, not drag (V4 item 4 confirmed).
3. Feet during the brake are not mirrored: R clip front foot 33 deg toe-out, hips 4-8 deg; L clip front foot 14 deg toe-in,
   hips 27 deg right. The idle yaw is baked into both. Recheck after 1.
4. v216-233: still two quick steps (rear foot 9 frames, then front foot 36 cm in 9 frames) while the trunk rises. Light for
   this body, not blocking (V4 item 3, open).
gioco
5. Base: v48 = v47 and v60 = v59 are repeated frames (actor step 0, pose frozen); v60 is the stop state start. Main: none.
6. Root offset steps at v187 (R) and v72 (L) are continuous in world space. Not visible. No pops into or out of the clip.
7. Dust: a pale puff sits at the feet from v236 to v279, and also at v0 in idle before any run. Base v53-67: a 60 cm blob
   hangs behind the run start. V3 item 8, open.
8. Stick still not logged ("st" constant), release frame not verifiable.

Next fix (one): make the clip end on the pose the game really idles in. Read the idle from the running game (ABP output at
rest: hands (4.0,-25.7,-2.7) / (3.4,26.3,-3.5), feet (1.3,-15.3) / (-8.8,14.7), pelvis (0.6,-0.3,4.0), head (5.2,0.8,62.4)
actor space, shoulders square) and use it as P0 for idle, IDLE_HAND, IDLE_ANKLE and foot yaw, or find why the game idle is
not M_Neutral_Stand_Idle_Loop frame 0. Then check the blend after the clip moves under 1 cm.
