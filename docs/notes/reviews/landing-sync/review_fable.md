PASS - on the first dust frame (fix f81) the body is 1.5 cm from the crouch bottom; limits: 1 capture, 1 of 32 rows, the frozen frame stays open

Reviewer: Fable (rule 10), 2026-11-19. Sources: 4 grids, frames.jsonl of both captures, single frames fix az90 f078/f080/f081, base az90 f084.
Step sizes below are relative to the actor (cm per frame), z values are world.

## posa
- Dust frame, fix f81: deep crouch, knees bent, feet planted (fix_az90 f81, fix_game f81). Pelvis 59.7, bottom 58.2 (f83):
  96 % of the drop from the touchdown value 95.0.
- Dust frame, base f84: legs still nearly straight, air silhouette inside the cloud (base_az90 f84). Pelvis 91.4, bottom 58.9
  (f93): 24 % of the drop from 101.6. The crouch is only reached 9 frames after the dust. The owner complaint is confirmed on base.
- Impact read, fix: world pelvis drop per frame f76-77 -20.0, f78 -25.5, f79 0, f80 -25.8, f81 -9.5, f82 -0.7. The body keeps
  the fall speed into the crouch and stops in 2 frames: this reads as weight, not as a teleport. Base brakes over 10 frames
  (f83-93, 3 to 7 cm per frame): soft, floaty.
- New in the fix: the left arm shoots forward. hand_l x -3.0 (f77) -> 11.7 (f78) -> 45.3 (f80) -> 52.9 (f81), 38 cm in the one
  frame f79->80, and stays stretched out f80-f83 (fix_az90). In base the 0.2 s blend hid it (peak x 31 at f89). It is the clip's
  own pose, not a blend error, but it is now the loudest thing in the frame: the owner should look at it.

## dati montage
- Touchdown (cap_bottom 2.1): base f80, fix f78. jump.phase goes to 0 at base f83, fix f80. Dust is visible 1 frame later in both.
- The landing blend starts on the touchdown frame in the fix: f78 step foot_l 19, foot_r 26, hand_l 21, pelvis 17 (foot_l x
  -28.7 -> -15.2). So f78 is not the pure air pose, it is about 40 % of the way. f80 step foot_l 27, foot_r 30, pelvis 26; f81
  feet 1 cm; from f82 the feet are locked (0 cm). Total: 3 pose frames, as 0.05 s predicts.
- Foot slide: base drags foot_l 40 cm forward over 7 frames (f83-90, x -30.0 -> 9.9) at 10-20 cm height while grounded. The fix
  does the same travel in f78 and f80 only. Less visible slide than base, not more.
- Floor: lowest foot z in the fix 10.1 / 9.2 (f81), base 10.2 / 9.0. No new penetration.
- No pop after the blend: pelvis f81-f91 stays 58.0-59.7, hands 7-9 cm per frame, same curve as base f90-f98.

## gioco
- Frozen frames: base f80-82 (3 identical), fix f78-79 (2 identical, so 1 repeated frame, 17 ms). In the fix the held pose is
  standing hunched on the floor (az90 f078: feet 12 and 24 cm above their planted height, pelvis 95), with no dust yet. At game
  camera distance (fix_game f78-79) it cannot be told apart from f77. Alone it does NOT keep the owner complaint true: dust and
  crouch are late together (f81), they are no longer out of sync.
- It still costs: dust and crouch come 3 frames (50 ms) after the capsule touches. The cut would be cleaner with the dust on
  f78/f79. That needs the state-entry freeze fixed, not another blend value.
- The capture has a timing hitch of its own (all bones step 16-17 cm at base f72 / fix f71, actor z drops 2 frames' worth), so
  part of the "freeze" may be the capture tick. Not proven either way.

## Not covered (conditions of this PASS)
- One run per side, one case: a jump in place, apex z 404, light landing. The other 31 rows (heavy, moving, directional) got
  the same 0.05 s and have no frames. A run landing with horizontal speed is where a 3-frame blend can show a foot snap:
  capture one before calling all 32 rows done.
- az0 grids and the mp4 files were not viewed.

## Coverage add-on (thread, not Fable): running landing mv_air_reverse (Saved/LandSync/run_land), touchdown f133 (cap_bottom 2.1), f133-134 frozen pair, pelvis 78.9 -> 66.8 f135 -> 58.6 f136 (bottom), dust first visible f135. Grid runland_az90.png. Same sync as the in-place jump.
