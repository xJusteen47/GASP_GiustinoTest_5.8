# Review skid-runstop-4 (Fable, 2026-11-19) - SKID: FAIL (gioco, narrow: facing fixed, feet slide after the turn)
Source: Saved/Skid/lf1/mv_skid, 60 fps. Looked at 4 images: skid_side.png, skid_front34.png, skid_game.png and, once, the old
r3_mv_skid_side.jpg. Numbers from frames.jsonl (actor loc, feet/pelvis/head/hands relative to the actor, world axes).
Frames = capture frames (fNNN = vNNN). Not judged: the mp4 in motion, sound, the run stop after f190. frames.jsonl still has no
actor / root / mesh yaw, so the facing is read from the hands' side offset and from the sheets.

## Identity traits
1. Brake reads as a heavy brake: YES. f119-f132, 546 -> 0 cm/s in 14 frames, pelvis -1 -> -22 cm, dust at the heel.
2. One continuous motion, brake THEN one 180 turn: YES (fixed). The turn runs f121-f142 (about 21 frames) inside the brake: the
   hands swap side at f128-f129 (hand_l y -20 -> +35, hand_r y +32 -> -31), the chest passes the camera at az90 v124 / az45 v122
   and faces the new way from v136. No hold of the old facing, no late snap: f142-f212 the facing does not change again.
3. Comes out running the new way: YES for the facing, NO for the feet (see gioco 1).

## FAIL items
- gioco 1 (blocker): feet slide after the turn, f139-f172. The clip holds its low lean with both feet on the floor while the sim
  already accelerates: left foot fixed in actor space (x +54) f139-f157 while the actor goes 1988 -> 1862 = 126 cm (240 -> 580
  cm/s); right foot on the floor (x +11 -> +34, z -79/-83) f150-f172 while the actor goes 1925 -> 1706 = 219 cm. Seen on az90
  v142-v172: 16 tiles of the same pose gliding away from its own dust. It was 95 cm in round 3, so this is worse, not better.
  The sim hits 0 at f132 and restarts at f133; the first foot leaves the floor only at f161 (clip frame ~43).
  Fix, pick one:
  A (preferred) leave the skid state when the turn is done, clip frame 24-26 (f142-f144), with a 0.2 s blend into the run start,
    so the stepping legs carry the acceleration; the last ~24 clip frames are not played.
  B keep the whole clip but hold the sim under 60 cm/s from f133 until the push-off (clip frame ~40, f158), and play clip
    frames 16-43 at rate 1.5 so the standstill is about 0.3 s and not 0.43 s.
  Target for the next capture: no planted foot (z under -75) travels more than 15 cm in world space after f134.
- gioco 2: clip end still held, f168-f170 (3 frames with the same pose at 586 cm/s), then the blend to the run f171-f181.
  Goes away with fix A; with fix B leave the state 4-6 frames before the clip end, blend 0.15 s.
- posa: none. The lean into the new direction (head 30-40 cm ahead of the pelvis, f142-f157) and the left arm swing are good.
- dati montage: none found (clip plays 1x, about 50 frames f118-f168, entry f119-f122 without pop).
- Capture note (not a verdict item): the mesh lags the actor by one tick every 24 frames (f109, f158, f182, f206; f118 speed 0
  then 546). Same as round 3; confirm it is not in a real-time run. Add actor yaw, root bone yaw and mesh yaw to frames.jsonl.

## Keep
- Root motion enabled on SS_180_Turn_Root180: one clean turn, f121-f142, inside the brake. Do not touch.
- Brake f119-f132: 14 frames, sit back to -22 cm, heel dust, wide pivot step (left foot 62 cm to the side at f130-f133).
- Entry blend f119-f122 and the forward lean with the arm swing at f142-f157.
- Brake-phase foot slide (right foot about 50 cm, f122-f132, with dust): it is the skid, keep it.

Parity: not yet. Facing and brake are at the bar; the 1.3-2.2 m glide on planted feet is what a player would see first.
