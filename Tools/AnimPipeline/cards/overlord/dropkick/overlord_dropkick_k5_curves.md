# Dropkick K5: curves sheet (rule 13c), startup 0-15 + loop 0-11

Identity traits (checked first): legs together and straight from f15 on; arms crossed (Bison fold: forearms flat across the chest,
left over right) from f3 to the landing; while falling he looks smug (owner 11-21), in that same pose.
Gameplay unchanged: startup 16 f, active from f15 (timing csv), loop 12 f x LOOP_REPS, hitbox/damage/hitstop as TL_Overlord_DropkickTBR4.

| frames | beat | timing / easing | what moves |
|---|---|---|---|
| f0 -> f3 | arms SNAP crossed | ease-out, 3 f, 75% of the arm travel by f2 | arms from the air guard to the fold; pelvis 5 -> 12 deg; knees start up |
| f3 -> f7 | tuck + lay back | ease-in-out (charge_long) | pelvis pitch 12 -> 50, knees to the chest, feet to the seat (ball 32 cm ahead, 13 above the pelvis); spine curls -22, chin tucked |
| f7 -> f13 | CHAMBER, the load | slow-in, near hold (moving hold, never frozen) | pitch 50 -> 66 -> 70, feet 3-5 cm tighter, arms squeeze 1.5 cm; eyes on the target (head -40) |
| f13 -> f14 | SMEAR | linear, 1 f | legs 2/3 out: ball travels 63 cm in one frame (biggest step of the clip) |
| f14 -> f15 | STRIKE (first active frame) | 1 f, arrives hard, no ease-in | legs locked straight, toes pointed (-50), pitch 82 (2 deg past the loop's 80-81), chest out (spine -8), chin up (head -40 -> -20 = the smug flick) |
| hit | hitstop holds f15 | 4 f attacker / 6 f victim (TL, unchanged) | the frozen pose is the strongest silhouette of the move |
| loop f0 -> f6 -> f11 | SMUG fall, period 12 | sine, +-1 deg pelvis, 0.5 cm | chest out more (spine -5), chin up (head -15), slow pleased head tilt 6 -> 12 deg, arms settle 1 cm; legs stay locked |

Overshoot / follow-through: the strike overshoots in pitch (82 vs 81 in the loop) and in the head flick; part_delays are 0 on purpose
(r0's chest1/head2/forearm2/hand3 put the head 2 frames after the active frame and blurred the snap).
Base for comparison: r4e (AM_OverlordDropkick, r0 restore + a0c7bf9f arms) frames v122-v156 of dropkick_low_hit.
