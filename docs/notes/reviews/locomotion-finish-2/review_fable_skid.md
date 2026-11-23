# Review locomotion-finish-2, skid attempt 2 (Fable, 2026-11-19) - SKID: FAIL (gioco: feet still glide after the turn, 170 cm)
Source: Saved/LF2/loco1/mv_skid, 60 fps. Looked at skid_a2_side.png plus 6 single frames (az90 f150 f158 f164 f170 f176, game
f164). Numbers from frames.jsonl (actor x + foot offset = world x), compared frame by frame with the old Saved/Skid/lf1 run
(old f = new f - 4). Not judged: the mp4 in motion, sound, the run stop after f190. frames.jsonl has no anim state / clip name
/ yaw, so what plays after the exit is deduced from the pose, not read.

## What fix A changed
- The state does leave the clip: the pose follows the old clip up to f149 and departs from f150 (0.42 s after the entry at
  f124). No pop at the exit: f148-f152 feet, pelvis and head move under 4 cm per frame. No double turn: one turn f125-f141,
  facing constant from f139 to f199 (hand_l y swing f137-f155 is the arm, the chest does not come back).
- But the pose it blends into does not step. Result: the glide is shorter, not gone.

## FAIL items
- gioco 1 (blocker): feet glide f151-f170. Right foot on the floor (z -78/-80) and under the actor (x offset -8..+7) for 20
  frames while the actor goes 1894 -> 1724 = 170 cm at 366 -> 588 cm/s (it was 219 cm). Left foot planted (z -77/-75) f141-f156,
  world x 1995 -> 1901 = 94 cm (it was 126), then dragged behind and only reaches -63 by f168. Pelvis stays low (-23 -> -17) to
  f167. Seen on az90 f150/f158/f164/f170: the same crouch, right foot under the hips, sliding left away from the dust. First
  real step: right foot lifts f172, run cycle readable from f176. Target (15 cm max on a planted foot after f134) missed.
  Only clean window: right foot world-fixed f144-f151 (1895 +/- 1 cm).
- gioco 2: 7 identical frames f117-f123 (actor and all bones frozen at x 1881), then 546 cm/s at f124. It was 1 frame in the
  old run. If it is a real hitch at the reverse (first load of the clip, RunStop log) it is a visible 0.1 s freeze: confirm.
  Double ticks still there at f132, f156, f180 (every 24 frames).
- posa: none. Brake f124-f136 (546 -> 0 in 13 frames, pelvis to -21, heel dust) and the lean into the new way (head 30-37 cm
  ahead of the pelvis f142-f158) are at the bar. Weight reads medium-heavy until f150; the glide then makes him weightless.
- dati montage: none (montage null; entry f124-f127 without pop; exit time and blend length are right, the target is wrong).

## Next fix (one)
Change what the Skid state blends INTO, not when it exits: at the exit (f149, sim already 340 cm/s) go straight to the run
loop clip at speed-matched play rate, started at the phase with the left foot back / right foot about to lift (matches the exit
pose), not to whatever the locomotion picks now (it looks like a start-from-idle pose held 20 frames). First add the playing
clip name + clip time to frames.jsonl for f145-f180 to confirm which asset is held. Same target: 15 cm max per planted foot.

## Keep
Root-motion turn f125-f141, brake and dust, entry blend, exit at 0.42 s with 0.2 s blend, re-entry guard (no second skid).
