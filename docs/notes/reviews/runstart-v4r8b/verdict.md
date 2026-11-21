# v4r8b run start: VERDICT FAIL (round 1 of b)
Seen: grid_az90, 2 side-view sheets from the mp4 (v88-v99 every frame, v110-v132 step 2), per-frame metrics both variants. Not opened: grid_az0, grid_game, Cascadeur png, Luigi ref.

Traits
1. Frantic multi-pump wheel from frame 1: KO. v90-v95 reads as ONE tucked hop/hover (L ankle up to 60 cm at v92, down and planted v98), not a blur of several pumps.
2. First contact clean into normal strides: KO. First contact L v98-v104 is fine (5 cm drift, v4r7 3.8 cm). The stride after it collapses (FAIL 2).
3. Arms at rest / forward lean / no stomp or standstill: OK. Arms relaxed chest-waist height, deep lean from v96, no pause; actor_x identical to v4r7 (795.6 at f200), no root motion lost.

Life
- Energy: the start is quick (moving from ~v86, lean + dust by v96) but the "engine revving" is missing: visible scramble is ~6 game frames (0.10 s) vs the 0.20-0.27 s target.
- Weight: while both feet are off the ground v92-v95 the body floats level over its shadow; it reads as a light toon hop, not a brute.
- Landing into loop: weak, see FAIL 2. No knee pops seen at this resolution.

FAIL items
1. dati montage, v84-v96: clip time 0 is at ~v82, so wheel frames f1-f7 play v84-v96, but clip weight is 0 until v90 and only 0.68 at v95 (0.99 at v100). Half the wheel is invisible and the rest is diluted by the idle blend. Fix by starting/blending the clip earlier or shifting the wheel later, not by cutting amplitude.
2. dati montage, v121-v135: the clip blends out at v118-v130 (v4r7 holds weight 1.0 to v155) into a run loop that is out of phase. L foot "plants" at x 207-212 with ankle at 21-27 cm (planted height is ~10), R swing peaks at 28 cm (normal 55-60). Seen on the sheet v128-v132: feet together, low stutter step. This also explains planted travel 44.1/47.0 vs 36.6/33.0.
3. posa (minor, would not fail alone), v112-v116: R foot skims in on landing, 18.7 cm forward with the ankle within 6 cm of plant height (6.9, 6.5, 5.6, 3.7, 2.9 cm/frame); v4r7's matching landing stops dead.
4. posa/dati (minor), v90-v92: R foot x jumps 1.3 -> -7.2 -> 3.5 in two frames at clip blend-in, a 1-frame flick while airborne.

Not a defect: actor_x double steps at v102/v126/v150 also exist in v4r7 (v97/v121/v145), capture hitch.

What the owner will notice: he does a quick floaty hop instead of Luigi's spinning legs, then takes one small mushy stutter step before the run locks in.
