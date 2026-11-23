# Review locomotion-finish-3, skid attempt 3 (Fable, 2026-11-19) - SKID: FAIL (gioco: the glide moved to the hand-back to motion matching, 111 cm)
Source: Saved/LF3/skid3/mv_skid, 60 fps. Looked at skid_a3_az90.png, skid_a3_game.png and 5 single frames (az90 f147 f151
f164 f170 f175). Numbers from frames.jsonl (actor x + foot offset = world x; planted = foot z about -77/-78). Not judged: the
mp4 in motion, sound, the run stop (speed drops from f194). The camera follows the character, so slide is judged from the
numbers, not from the images. frames.jsonl still has no anim state / clip name / clip time: timings below are deduced.
Timeline read from the data: run +x 585 cm/s, reverse f118-120, skid entry f120, exit to Skid Exit Run f146 (0.42 s, sim
at 324 cm/s), blend done f155, hand-back to motion matching about f164, run stop from f194.

## Identity: OK
- One clean 180 turn, f124-f141 (foot_l y 8 -> 62 -> 15, hand_l y -29 -> +51 once, no second turn up to f199).
- Brake weight at the bar: 546 -> 0 cm/s in 13 frames (f120-f132), pelvis sinks -2 -> -25 by f137, dust, head 30-36 cm
  ahead of the pelvis into the new way f140-f146. Reads medium-heavy until f146.
- First step is on the correct foot: left (the back foot, 54 cm behind) lifts at f147 and swings through f147-f162 while the
  right supports. The phase match of the run loop is right.

## FAIL items
- gioco 1 (blocker): frozen run pose f164-f175, right at the hand-back. Left foot flat on the floor under the body (z -73
  -> -78, actor offset -11..0) for 12 frames while its world x goes 348 -> 237 = 111 cm at 585 cm/s (125 cm from f163). Right
  leg hangs bent behind the whole time (z -42 -> -22), its swing lasts f158-f190 = 32 frames. az90 f164, f170, f175 are the
  same picture. First world-fixed plant only f175-f180 (6 cm). Baseline run in the same capture: 8-15 cm per stance, stance
  6-8 frames (f60-65, f72-78, f94-101). So the run loop steps (f155-f163 fine), then the pose motion matching blends into
  does not advance. Same defect as attempt 2 (170 cm f151-170), 15 frames later and 1/3 shorter.
- gioco 2: right foot dragged 42 cm while grounded (z -77.5/-78.8) during the 0.15 s exit blend: world x 518 (fixed +/-1 cm
  f141-f147) -> 476 at f153, most of it f148-f151 (36 cm in 3 frames). Then fixed f153-f157 (3 cm). The skid pose has the
  foot 11 cm ahead of the actor, the loop phase wants it 12 cm behind, and the actor covers 64 cm in the blend.
- gioco 3 (minor): left foot flick in the same blend, f148-f151: y 19 -> -10 (29 cm sideways in 2 frames) and world x
  532 -> 509 -> 521 (back then forward) while lifting. Pelvis rises 22 cm in 7 frames (f146-f152): snappy, acceptable.
- dati montage (minor): inside the skid clip the trailing left foot is on the floor (z -77) and moves with the actor,
  f137-f146: 617 -> 572 = 45 cm. Reads as a toe drag of the push-off leg on az90 f147; tolerable, but it is 3x the target.
- gioco 4 (minor): 2 identical frames f118-f119 at the reverse (it was 6), then catch-up. Still a 33 ms hitch: confirm it is
  the game and not the capture. Double ticks every 24 frames (f122, f146, f170, f194) are capture cadence: world positions
  of the bones stay smooth across them, not a pose pop.
- posa: none.

## Pops at the two blends
- Entry to Skid Exit Run (f146): no pop, pelvis world x steps 5-6 cm per frame through f144-f150, head follows.
- Hand-back (f164-f176): no pop, the opposite problem: the pose stops evolving (gioco 1).

## Slide per planted foot after the turn (target 15 cm)
L f137-146: 45 | R f141-147: 1 | R f148-153: 42 | R f153-157: 3 | L f164-175: 111 | L f175-180: 6.

## Next fix (one)
Fix the hand-back, not the exit: keep Skid Exit Run playing the run loop (speed-matched play rate) for at least one more
full step and hand back to motion matching in mid flight of a step, when the trajectory history is all straight run (about
0.7-0.8 s, not 0.3 s), so the search continues the run instead of holding a pose. Before that, add state name + playing
asset + asset time to frames.jsonl for f140-f185 to confirm what is held f164-f175. Then (second) start the loop about 4
frames later in phase so the right foot is at toe-off during the blend (removes the 42 cm).

## Keep
Turn and brake f120-f141, exit time, the new state and its phase choice (correct first foot), no pops, re-entry guard.
