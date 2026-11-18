# Skid with root-motion samples (SS_180_Turn_Root180 at 1x, sim speed/facing baked from the clip) - review fable: FAIL (narrow: only the hand-over to the run)
Sources: 2 grids (once each), az90 singles f058 f065 f072 f080 f090 f100 f108 f113, frames.jsonl f50-128, actor_speed_f40-135.txt. Video not opened. Foot world x = actor x + mesh-relative x; floor is foot z about -79. Yaw is not in frames.jsonl: facing is read from the images and the feet left/right swap.
Brake and turn are now good: the two FAILs of skid-57-1 (brake pose late, feet dragged in brake/turn) are fixed. What fails is the last third: clip tail and hand-over.

## Checks
1. ONE 180 turn: PASS. f058-f065 facing right, f072 chest to camera, f080 three-quarter left, f090 full left. One direction, no second turn, no snap. Feet y swap once (foot_l y -34 at f70 -> +21 at f91).
2. Pop on entry / hand-over: PASS for the pose, with two notes.
   - Entry f54-58: feet, pelvis continuous. Pelvis world speed stays ~330 cm/s f54-70 (the root dips to 108 cm/s at f59 and climbs back to 282 at f65, but the pelvis offset absorbs it: no visible stutter).
   - To verify (harness or gioco): f53 is identical to f52 in actor x and every bone (capsule speed 450 -> 0 -> 390). It is not on the known 24-frame artefact (f40/64/88/112). Either a duplicated capture frame or the first skid tick applies a zero delta.
   - Hand-over f103->104: no pose step (feet and pelvis continuous), but capsule speed goes -84 -> -450 cm/s in 5 frames (f104-109). See check 4.
3. Brake pose WITH the brake: PASS. Front foot (foot_l) plants at f64 with dust, stance 84 cm (foot_l 184, foot_r 100), pelvis z -19/-20 against -13 in the run (f64-67), torso leaning back (f065). The body is still travelling at ~330 cm/s then and stops by f84: the lunge is on screen during the stop, not after. Pose is readable but moderate (not a deep skid, torso near upright by f072).
4. Feet glide: brake PASS, turn PASS, run restart FAIL.
   - Brake: foot_l locked at world x 184-185 for f64-73 while the capsule moves 144 -> 175. Was a 46 cm drag in skid-57-1.
   - Turn: foot_r locked at 196-197 for f79-89, capsule almost still (180 -> 184). No sideways carry, no frozen straddle.
   - Restart, FAIL (dati montage): f88-103 the capsule moves at only ~84 cm/s for 16 frames (184 -> 162) while the body is already in a full leaning run pose (f090, f100). Reads as running on the spot. foot_l lands at f97 and creeps 16 cm the wrong way (163 -> 179, f97-102) as it touches down.
   - Restart, FAIL (gioco): walking takes over at f104 and reaches 450 cm/s by f109 while foot_l is still the planted foot: it slides 179 -> 110 (69 cm) at floor height (z -78..-75) over f105-115, with dust (f108, f113). Locks again only at f115-118.
5. Readability vs skid-57-1: BETTER on everything up to f88: brake lunge present and on time, turn in place over the pivot foot, no straddle (old f082-091), no backward travel while facing right (old f66-74). WORSE on one point: time to get going. Old attempt was at full speed by f76; now it is f109 (0.93 s from the reversal), with the dead stretch f88-103.

## Parity
Smash Bros / AAA: no, but close for the first time. Biggest gap: the restart. After the pivot the character idles in a run pose at 84 cm/s for ~0.27 s, then the capsule jumps to full speed under a planted foot (69 cm slide, f105-115). A Smash skid turn is a hard brake, a pivot, and an immediate push-off.
Next to check: end the skid at the push-off (about f88-90, clip t ~0.58-0.62 s, when foot_r leaves the floor) and hand over to walking there, or make the walking acceleration start on a foot lift instead of mid-plant; check whether the clip's last 16 frames really carry only ~84 cm/s of root motion or the baked samples are clipped. Then look at the f52/f53 frozen frame.
