# Run skid fix: critic review (Fable)

**Verdict: PASS with 3 follow-ups.** The two target pops are gone and nothing looks worse than the noguard grid in f130-153. Not clean: the guard misfires at clip entry, and the right foot still slides on the floor after the turn (old problem, not caused by the fix).

Evidence: feet_guard.txt / feet_noguard.txt, 5 grids/crops of hold and release (az90, az0, noguard, zoomed crops from the half-speed mp4). Feet are partly hidden by the dust cloud in every view, so the numbers carry more weight than the images.

## 1. Right foot f136-145: pop gone
- Noguard: 11.1 cm in one frame at f137 (hold), 10.3 at f138 (release). Guard: 0.3 / 0.4 on that frame.
- The guard repays it as about +1 cm/frame for 6 frames (hold f138-143: 6.9 7.6 7.7 7.4 6.9 6.2 against 5.9 6.2 6.7 6.4 6.1 5.8). Not visible.
- **WARN, montage data or pose (old, same in noguard):** from f138 to f149 the right foot travels 77 cm (hold) / 71 cm (release) in 12 / 11 frames at 5-8 cm/frame, flat on the floor (z -78.0..-79.3), while the left foot stays pinned (1.7-2 cm/frame). The foot is released at clip time 0.333 but never lifts, so it rides the capsule in the new direction. In hold f140-148 the stance stretches to a very wide lunge with a straight rear leg. The dust hides most of it. Check in the clip whether the right foot should step here.

## 2. Left foot f145-153: pops gone, leg now lags
- Noguard hold f146-148: 31.2 / 20.9 / 33.8 cm per frame. Guard: 3.1 / 4.7 / 4.8. Release f147-149: 31.9 / 20.6 / 32.7 against 2.7 / 4.2 / 4.0.
- **WARN, game:** the guard fires 3 frames in a row and swallows about 73 cm of real swing, not a pop. The foot sits 12.8 cm lower than the clip at f148 (z -59.8 against -47.0) and the leg stays stretched behind in f146-151, where noguard already has the knee tucked. It then whips forward at 27.8-28.6 cm/frame in f153-155 (noguard 21). Small speed dip at f149-151 (11.0 / 6.0 / 12.3). Smooth enough, but the recovery reads later and longer.

## 3. Floor: OK
- Planted z stays in -77..-79.4, no dip below. Bonus: hold noguard had the planted left foot floating at z -64..-67 in f125-135; with the guard it is at -72..-77.

## 4. Body turn: one turn, acceptable, not eased
- Hold yaw rate: 7.0 deg/frame falling to 4.7 at f130, then a constant 10.27 for 8 frames (f131-138), then 5.1 / 2.1 / 0.5. The 4.7 -> 10.3 step in one frame is a kink (**montage data**: the 75 deg exit is linear, no ease in or out). In the grids it still reads as one turn, far better than 22.5 deg/frame.
- Both feet are pinned (0.1-3.7 cm/frame) in f131-137 while the pelvis turns 82 deg: it reads as a pivot, no torso spin against the legs.
- Overshoot 7.7 deg (peak -172.29 at f141, back to -180 by about f170), same in both runs, so it comes from the baked curve. Acceptable as follow-through.
- Log caveat: actor yaw double-steps once (release f136 +20.5, hold f118 +13.7) while mesh yaw is smooth. Sampling, not visible.

## 5. New and worse than noguard
- **FAIL item, game:** the guard misfires at clip blend-in. Hold f120-122 left foot 0.7 / 1.6 / 2.0 cm (noguard 20.1 / 24.4 / 17.9), then 20.3 / 18.8 / 19.6. Release f121-123 the same. The foot hangs still in the air (z -72.7) for 3 frames at run speed, then does the same travel 3 frames late. Subtle under the dust, but it is a hitch the noguard run does not have.

## Follow-ups
1. Game: do not let the guard fire during clip blend-in/out (weight < 1), or cap consecutive triggers to 1 frame.
2. Montage data: ease the 8-frame exit turn.
3. Pose / montage data: right foot after 0.333 should step or stay locked, not ride the floor for 70+ cm.
