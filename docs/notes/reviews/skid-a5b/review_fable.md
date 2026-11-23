FAIL - removing movedata_speed fixed only the hold stall (pelvis); the right-foot pop at v137/138, the two different skid poses and the 30 cm hand-back slide are all still there, and the v265 release is not judgeable
# Review skid attempt 5b (Fable, 2026-11-19)
Source: Saved/LocoR2/cap1, 60 fps, f = v, both runs travel +x then -x. Looked at the three az90 grids and 4 single frames (release
az90 f137; hold az90 f136, f195, f266). Numbers from frames.jsonl (actor + foot offset = world, planted = foot z about -77/-79),
compared with Saved/Skid/cap5 by the same script. Not judged: mp4s, sound. ABP state and actor yaw are not logged, so causes are deduced.
Timeline release: reverse f117, speed 0 f131/132, release f145, actor still f177 (101 cm after release).
Timeline hold: reverse f117, speed 0 f130/131, exit f146, full speed f157, hand-back about f191 (deduced), release f265.

## Fixed since a5 (keep)
- gioco: hold stall is gone in the pelvis. Pelvis z now follows the same curve in both runs (-24.5 at release f137 = hold f136;
  a5 hold stopped at -20). Left foot z also equal. The clip time is now the same in both, the release run is 1 frame behind
  (its repeated frame f118).
- Release case still clean: 0 frozen frames f119-f209, L lifts f147, L planted f164-f177 moves 0.2 cm, R planted f177-f209 1.6 cm.

## FAIL items
- gioco 1 (not fixed): right foot pop inside the skid, same size as a5. Release f138: 12.9 cm in one frame (61.8 -> 50.4, y +6);
  hold f137: 10.3 cm. It is the same clip frame in both runs (20 frames after the reverse), so it is tied to clip time, not to
  speed. After it the planted foot keeps slipping: release 12.5 cm f138-f146, hold 24 cm with 20 cm sideways f137-f146.
- gioco 2 (not fixed): the skid after speed 0 is still two different motions for the same input. Equal within 2 cm up to speed 0,
  then: release, left foot slides out 30 cm on the floor f131-f136 and comes in to y +35 (wide low lunge, az90 f137); hold, left
  foot stays at x 98, y 61 f130-f140 (narrow crouch, az90 f136). At release f136 / hold f135: left foot 32 cm apart, right hand
  38 cm, head 18 cm, pelvis z equal. Every point is turned about the actor by the same angle (about 38 deg): the body yaw differs,
  not the clip. Candidate: actor or mesh yaw is driven from outside the clip and starts 1 frame apart relative to the clip.
- gioco 3 (not fixed): hand-back stance slides. Hold, left foot planted f192-f198 goes 2069.6 -> 2038.9 = 31 cm (5 cm per frame;
  az90 f195 reads as a normal run stance that skates). Stances before and after: L f161-166 5 cm, R f176-181 5 cm, R f213-218
  9 cm, L f232-236 9 cm.
- gioco 4 (minor, not fixed): exit pops. Lifting left foot 28 cm in one frame (release f147, hold f146). Planted right foot zigzags
  10 + 12 cm over two frames (release f148-149, hold f147-148), pelvis z +4.3 in the same frame. Hold R exit drag f145-f149 19 cm.
- gioco 5 (minor): release f118 identical to f117 with the actor still (1 repeated frame at the reverse); hold has none. Idle
  after the release: left foot creeps 20 cm at z -72/-74 f178-f203 (known idle stance drift).
- capture fault: the v265 release cannot be judged. From f244 the speed rises 586 -> 816 with the stick unchanged, and at
  f264-f277 the pawn runs over a 10 cm raised disc of the playtest kit (actor z 88 -> 98, az90 f266). After the release the right
  foot slides 190 cm on the floor f273-f294; with this contamination it proves nothing either way. a5 start lane (y -150) was clear.
- capture note: every 24 frames one captured frame holds two movement ticks (hold f149, f173, f197...; release f117, f142). Head
  and hands jump 10 cm on those frames. Capture pacing, not animation, but it hides small pops.
- dati montage: none new (left foot in the skid 0 cm in hold; the 30 cm in release is gioco 2). posa: none; the release pose
  (wide low lunge) is the one to keep, the hold crouch is too upright for the style target.

## Slide per planted foot (target 15 cm) - a5 -> a5b release / hold
L in skid 14 / 8 -> 30 / 1 | R in skid 23 / 33 -> 24 / 34, pop 9-12 -> 10-13 | R exit 16 / 16 -> 14 / 19 | L first plant 0.5 / 7
-> 0.2 / 5 | R second plant 1.5 / 5 -> 1.6 / 5 | L at hand-back 30 -> 31 | after v265 release: not judgeable.

## Next fix (one)
Make the body yaw during the skid come from one source only, latched at skid start: from the reverse to the exit frame the actor
and mesh yaw must follow the clip, not the velocity or the stick. Log actor yaw and mesh yaw in frames.jsonl and verify that the
two captures give the same pose (2 cm) from speed 0 to v146. Then recheck the f137/138 pop at that clip frame (first suspect:
contact_r releasing the foot lock). Capture needs: move the hold lane off the kit props (clear floor for 20 m), fix the ABP log.
