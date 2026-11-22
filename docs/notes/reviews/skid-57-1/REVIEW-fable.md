# Skid 5.7 style (main ABP Skid state, SS_180_Turn_Root180 at 1x, before Offset Root Bone) - review fable: FAIL
Sources: 4 grids (once each), az90 singles f064 f072 f079 f088 f089 f097 f110 f122, frames.jsonl f40-169 (v = f; stick reversed on v50). Yaw is not in frames.jsonl: facing is read from the images and from the feet left/right swap.
The owner's main complaint (double turn) is fixed. It fails on brake pose and on clip timing against the capsule.

## Checks
1. ONE 180 turn: PASS. f064 side-on facing right, f079 back to camera (~90), f088 ~150, f097 full run pose facing left. One direction only, no second turn, no backward run. Feet y swap once (foot_r y +22 at f72 -> -13 at f100). Base turned in ~10 frames (v64-73) with the head thrown down; main turns in ~28 (f64-92), readable.
2. Snap/pop: PASS.
   - Entry f52-56: feet, pelvis, head continuous (pelvis x -31.7 -> -25.8, no one-frame step). No pop.
   - Exit f95-125: no step. Pelvis z -14.5 -> +5.7 over f100-122, max ~2 cm/frame. f110 and f122 are clean left-run poses.
   - Not counted: root offset jumps 0 -> 7-8.5 cm every 24 frames (f40, f89, f113, f137) together with a double capsule step (15 cm instead of 7.5). It also happens in the plain run, mesh world motion stays smooth. Capture/tick artefact, but worth one look by whoever owns the harness.
3. Brake pose a real lunge: FAIL (dati montage, maybe gioco). The capsule brakes f53-64 only (39 cm, 0.18 s). In those frames the clip is still a decelerating run stride: f064 is a half crouch, stance 57 cm (foot_l +35, foot_r -22), pelvis z -16 against -15 in the run, so the body is not lower than the run. f070-074 feet 10-25 cm apart, torso upright (f072). Base v58-61 shows the wide forward lunge of the same clip. At 1x the low frames come after the brake is over, or the play window starts past them.
4. Feet gliding: FAIL (gioco). The capsule reverses at f64 and is at full 450 cm/s by f76, while the clip is still braking/turning until ~f92.
   - f66-74: brake foot (foot_l) dragged back on the floor, world x 228 -> 182 (46 cm) at planted height, body moving left while still facing right (f072).
   - f76-88: body travels 90 cm sideways with its back to camera; foot_l skims 157 -> 40 at 6-9 cm off the floor (reads as flat glide in grid v79-88).
   - Good part: push-off foot (foot_r) is locked at world x 187 +/-1 for f68-81. But the capsule runs away from it: foot ends 96-104 cm behind the actor (f086-091), a frozen over-stretched straddle for ~10 frames (f088, f089).
   - Minor (gioco, exit blend): first run steps slide, foot_r 20 cm f111-117, foot_l ~80 cm at near floor height f121-128.
5. Readability vs base: BETTER on turn (single, slow enough to read, clean run out by f097 instead of base's upright glide v76-100) and on entry (no pop). WORSE on brake silhouette (lunge lost) and on the long straddle f082-091.

## Vs previous main-graph attempt (skid-main-graph-1)
Fixed: the 180 now completes and ends in a real run facing left. Still there: weak brake pose, straddle frozen ~10 frames while the body glides.

## Parity
Smash Bros / AAA: no. Biggest gap: the capsule has already reversed and is at full speed (f64-76) while the body is still braking and turning (to f92), so the brake pose comes after the brake and the turn is carried sideways.
Next to check: make the sim's speed curve follow the clip (hold near zero speed until the push-off, ~f80) or start the clip earlier/align its low frame to f58-62; entry blend may stay very short, warping on.
