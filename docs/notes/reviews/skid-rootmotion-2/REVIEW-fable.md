# Skid, clip ended at 0.7 s and hand-over to the locomotion run - review fable: FAIL (narrow: feet slide in the first two run steps after the hand-over)
Sources: 2 grids (once each), 8 singles (az0 f096 f102 f106 f109 f113 f118, az90 f100 f106), frames.jsonl f50-124, actor_speed_f40-135.txt. Video not opened. Foot world x = actor x + mesh-relative x; floor is foot z about -79. Yaw is not in frames.jsonl: facing is read from the images.
The dead stretch of attempt 1 is almost gone and the restart is 9 frames earlier. What still fails is the foot contact once the run takes over.

## Checks
1. ONE 180 turn, no snap: PASS. v058-v066 facing right, v072 chest to camera, v080 three-quarter left, v088 full left. Feet swap sides once (foot_l y -34 at f70 -> +21 at f91). From behind (az0 f096) the back is square to the camera right after the clip ends: the missing root yaw at 0.7 s does not show as a mesh pop or a yaw snap.
   - Note (gioco, minor): f105-112 the body does a wide side step. foot_l lands 36 cm out to the side (y +36 at f109, foot_r y -17: 53 cm stance against ~20 in the steady run), head 17 cm off-centre, arms spread (az0 f106, f109). It closes by f118. Reads as a small wobble, not a second turn. This may be the leftover yaw being blended out.
2. Pop on the hand-over (f94->95): PASS. Feet and pelvis continuous frame to frame (foot_l 221 -> 216 -> 208, foot_r 257 -> 256 -> 254, pelvis z -6 -> -5 -> -5). No pose step.
   - Still open from attempt 1: f53 identical to f52 in actor x and every bone (speed 840 then 0). Looks like a duplicated capture frame, to verify.
3. Feet at the run restart: FAIL (gioco).
   - Capsule: -84..-96 cm/s for only 5 frames (f90-94), then 6 frames of ramp, full 450 cm/s at f100.
   - foot_l comes down at f98-100 (z -75..-77) and never locks: world x 194 -> 108 over f98-110 at z -77..-72, about 80 cm of slide (7 cm of it is the f101 capture artefact). Dust under it f100-109.
   - foot_r lands at f110 (z -76..-79) and slides 101 -> 48 over f110-120 (53 cm), slowing down; it locks only around f121-124 (46 -> 40).
   - So the first two run steps skate; the run is clean from the third step. Before the skid the same run locks its feet (foot_r 102 for f50-53), so this is the hand-over, not the run cycle.
   - Not a clip problem any more: the clip is over at f95, the capsule is under the walking mode and the pose is the locomotion graph's. The pose it shows for ~20 frames has a shorter stride than 450 cm/s needs.
4. Earlier PASS items: no regression. Brake: foot_l locked at world x 236-238 for f64-73 with dust, pelvis z -19/-20 at f64-67, torso back (v064-v066). Turn: foot_r locked at 247 for f80-89 while the capsule is almost still (233.9 -> 232.2).
5. Versus attempt 1: BETTER.
   - Slow stretch in a run pose: 16 frames -> 5 frames. Full speed at f100 instead of f109 (0.78 s from the reversal instead of 0.93 s).
   - No capsule jump under a planted foot: the speed ramps while foot_l is still in the air (f95-97).
   - NOT better: total foot slide. Attempt 1 had one slide of 69 cm (f105-115); now there are two, ~80 cm and 53 cm (f98-120). They read softer because the feet are stepping, not planted, but the dust makes them visible.

## Parity
Smash Bros / AAA: no. Brake and pivot are at that level; the push-off is not. Biggest gap: the first two steps out of the turn skate (f98-120) and include a wide side step (f105-112). A Smash skid turn ends with one planted push-off foot and the first stride already gripping.
Next to check (gioco): what pose the graph picks at f95 (run cycle at full stride vs a start/low-speed pose, and the blend time); try stride warping or speed-matched play rate on the blend-in, or a pose search that enters on a foot_l contact; check the side step against the remaining clip yaw at 0.7 s (if it is the cause, apply the missing yaw in the last clip frames or end at a time where yaw is complete).
