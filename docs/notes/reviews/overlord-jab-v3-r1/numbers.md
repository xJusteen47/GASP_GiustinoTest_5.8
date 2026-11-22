# Overlord jab v3 round 1: numbers (base = v3 before this round, variant = r1; Play captures, Saved/AnimJabR1/{base,variant})
Sequences: jab1, chain (= jab1_jab2 whiff), chain_hit (= jab1_jab2_hit, dummy 95 cm ahead). Views game and az270 (profile, left side; the existing compare script's "side").
Build worst vs source: jab1 10.32 cm (hand_r), at the poses 2.09 cm; jab2 23.87 cm (hand_r), at the poses 1.90 cm.

## Capsule, jab 2 (z, displacement from the chain start, cm), variant | base
variant chain, f0/3/6/9/12/20/26/38/52: z 100.0 and displacement 0.0 on every frame. Chain end: d 0.0, z 100.0. Same in chain_hit (0.0 / 100.0 all frames).
base chain: z 100 until f3, 88.1 from f6; d 0 / 0 / 3.0 / 29.9 / 30.0 ... 30.0 at the end. base chain_hit: d 25.9 at the end, z 88.1.
So the step is now in the pose: capsule stays at 100 and 0.

## check gates (Tools/AnimPipeline check, recorded not fixed)
jab1: FAIL (blocking legibility: fist not distinct at contact 6, rear arm hides the fist 3, attacking arm hidden from the game camera 6); contact_faf PASS (reach peak f6, window 6-8, FAF 22); clearance, bone_lengths, planted_feet (0.16 cm), hitstop_snap (max 8.4 cm 6->7), co_motion, fists PASS; motion_check FAIL (stiff chain 3, no overlap 4, unnatural joints).
jab2: FAIL (legibility: rear arm hides the fist 27, rear elbow behind the back 1, hand_l/hand_r open 2); contact_faf FAIL (reach peak f12, window 9-11, FAF 26); hitstop_snap PASS (max 7.9 cm 11->12); clearance, bone_lengths, planted_feet n/a, co_motion, fists PASS; motion_check FAIL (stiff chain 4, no overlap 4, unnatural joints).
Note: the first `check` call of jab 1 failed on a missing Saved/AnimPipeline/refs/ufd/OverlordJab1V3_timing.csv; I copied cards/overlord/jab/timing/*.csv there and reran (second attempt on that step).

## Hand (hand_r, component space of the built game copies; +X character left, +Y forward, dummy centre 95 cm ahead = y 95; root motion 0 so no offset)
- jab 1 contact f6: (-44.5, 60.4, 138.6): 44.5 cm to the character's right, 34.6 cm short of the dummy centre plane (y 95). f8: (-49.7, 56.3, 137.3).
- jab 2 contact f9: (-16.8, 78.3, 125.7): 16.7 cm short of the dummy centre plane; pelvis y 25.5 (idle -2.5).
- jab 2 f0 hand (-46.2, 57.1, 139.7) = jab 1 f8 region; f52 = idle pose (-24.9, -17.6, 85.5).

## One grid (cmp_jab1_game.jpg, looked at once)
The game camera cells are small (character about 60 px tall). Base and variant differ clearly from f1 on (variant: a0.4 on f1, base a2.0, the earlier-start sync shift), and the variant shows the lead arm extended out to the side during f5-f7 and no idle on those frames, so the new clip is playing. I cannot resolve the arm direction (forward-right, outside the body) at this size: that needs a zoom. Oddity: variant f25-f26 (v45-v46) show the figure bent low, base shows idle there; probably the blend-out/hit of a different pose, not checked.
