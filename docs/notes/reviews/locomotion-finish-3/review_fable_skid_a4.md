# Review locomotion-finish-3, skid attempt 4 (Fable, 2026-11-19) - SKID: FAIL (gioco: the f164-175 freeze is gone, but the hand-back is not proven and the pose freezes f200-219)
Source: Saved/LF3/rs4/mv_skid, 60 fps. Looked at skid_a4_az90.png and skid_a4_game.png once, no extra images. Numbers from
frames.jsonl (actor x + foot offset = world x; planted = foot z about -77/-78), compared with Saved/LF3/skid3. Not judged:
the mp4 in motion, sound. The camera follows, so slide comes from the numbers. frames.jsonl still has no anim state / asset
/ asset time, so timings are deduced.
Timeline: run +x 585 cm/s, reverse f117-120, skid f120-146, Skid Exit Run from f146 (sim 342 cm/s), blend done about f153,
hand-back due f191 (f146 + 45), script releases the stick at f193 (pie_capture_move.py: sticks 95 -> 170), speed drops f194.

## Identity: OK (unchanged from attempt 3)
- One clean 180, f124-f141 (foot_l y 8 -> 64 -> 20 once, no second turn up to f219).
- Brake weight at the bar: 546 -> 0 cm/s f120-f133, pelvis sinks to -25 by f137, dust, head 30-37 cm ahead of the pelvis
  into the new way f139-f147. First step on the correct foot (left lifts f147).

## Fixed
- The f164-175 freeze is gone. Left foot lands f165 and holds world x 1703 -> 1690 for f165-f172 (13 cm, 7 cm from f166),
  stance 8 frames; right foot f181-f187 6 cm, stance 7 frames. Same as the baseline run (8-15 cm, 6-8 frames). Pose
  evolves every frame f153-f193, steps alternate L f165 / R f181 / L f193. az90 f158-f192 reads as a normal run.

## FAIL items
- gioco 1 (blocker, new place for the old defect): frozen pose f200-f219. From f200 every bone offset is constant to 0.1 cm
  for 20 frames (foot_l +52 behind the actor, y 4.8, z -71.6 = hovering 7 cm; foot_r -37, z -78.8; pelvis -9) while the actor
  goes 1363 -> 1274. Right foot is on the floor and slides 84 cm (1326 -> 1242). az90 f200-f214 are the same mid-stride
  picture gliding. Attempt 3, same release: a real stop, pelvis sinks to -29, L fixed f208-219 (0.4 cm), R fixed f207-219
  (5 cm). So the 0.75 s hold broke the stop that follows it: the release lands 2 frames after the hand-back, inside the
  0.2 s blend, and the pose stops advancing when that blend completes (f191 + 12 = f203). Same symptom as attempt 3 f164-175
  (the pose motion matching hands back to does not advance), moved 30 frames later.
- gioco 2: bad step into that freeze, f194-f199. Right foot swings 159 cm in 5 frames (43 cm/frame at f196-f197, normal
  swing is 15-18) and the left foot, at z -72, goes backwards 25 cm in world (1396 -> 1421) before freezing in the air.
- gioco 3 (unchanged, not touched): right foot dragged 41 cm while grounded in the exit blend, world x 1881 (fixed +/-1 cm
  f141-f147) -> 1840 at f153, 31 cm of it in f148-f151. Then fixed f153-f157 (4 cm).
- gioco 4 (minor, unchanged): left foot flick f148-f150, y 20 -> -9 and world x 1894 -> 1872 -> 1888 while lifting.
- dati montage (minor, unchanged): trailing left foot on the floor moves with the actor in the skid clip, f139-f146 36 cm.
- gioco 5 (minor, unchanged): f117-f119 identical (2 repeated frames) at the reverse. Double ticks f161, f185, f209 are
  capture cadence (bone world positions stay smooth), not pops.
- capture gap: the stick is released 2 frames after the hand-back, so this capture cannot show a held run through the
  hand-back. "Freeze fixed" is proven only for f164-175, not for the hand-back itself.
- posa: none.

## Slide per planted foot after the turn (target 15 cm) - attempt 3 -> attempt 4
L in skid: 45 -> 36 | R f141-147: 1 -> 2 | R exit blend: 42 -> 41 | R f153-157: 3 -> 4 | L first plant: 111 -> 13 |
R second plant: (6) -> 6 | after release f200-219: R 5 -> 84, L 0.4 planted -> not planted (hover, moves 96 with actor).

## Next fix (one)
Make the Skid Exit Run state leave at once when the stick is released or reversed (go to the normal stop / motion matching
path, do not wait out the 0.75 s), and capture two cases: (a) stick held 1.5 s more after the hand-back (release at script
frame 260) to prove the hand-back in a held run, (b) release at 0.3 s into the hold. Add state name + playing asset + asset
time to frames.jsonl for f140-f230: two attempts in a row the hand-back target is a pose that does not advance and we still
cannot name it. Second, after that: start the loop about 4 frames later in phase to remove the 41 cm exit drag.

## Keep
Turn and brake f120-f141, exit time, phase choice (correct first foot), the longer hold in a held run (steps f153-f193).
