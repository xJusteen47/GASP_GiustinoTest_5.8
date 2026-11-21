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

## Round 3 (2026-11-21, review k5r2b "Round 3"): what changed against the table above
| frames | beat | what moves now |
|---|---|---|
| f3 -> f13 | upright fold | unchanged (round 2, passed): elbows forward and in, forearms level, left over right |
| f13 -> f14 | SMEAR + arms lie down | 1 f: the fold switches to the LYING fold (k5/fold3.py, fold_r3.json) while the legs smear: upper arms down along the ribs, elbows out at the sides (18-22 cm) and 8-16 cm forward of spine_05, forearms on the chest, left (on top, toward the chin) over right, hands on the other arm. Arm block <= 11.3 cm above the sternum (round 2: 18.7), below the top of the head in the side view (offline 28 vs 33 cm above the pelvis; round 2: 41 vs 31). Head still on the target (-36). |
| f14 -> f15 | STRIKE + smug flick | head: neck_01 flexed 12 deg (head lifted off the body line), head tipped back 12 deg on top (chin up), turned ~25 deg to his right (toward the az90 camera) and tilted ~25 deg: face 35 deg above the horizon, looking down the body at the target (round 2: 54 deg, at the sky). Arms held. |
| loop f0 -> f6 -> f11 | smug hold, period 12 (0.2 s) | a held attitude, not a cycle: f6 chin up 3 deg more, tilt 3 deg more, chest out (spine -8 -> -5); arms identical on every loop key (a cycle of 0.2 s would read as a 5 Hz jitter). |
Overshoot at f15-f17 (owner's OOMPH note): NOT done. Startup (16 f) and loop (12 f) lengths are gameplay timing; an overshoot settling
after f15 would sit in the loop and pulse every 0.2 s. It needs a one-shot segment after the strike (a montage change): owner's call.

## K6 (2026-11-21, owner on the K5 round 3 videos): weight, follow-through, force in the arms. K5 stays as it is
Owner: more follow-through, secondary motion and weight (head, limbs); the arms PRESS in toward the body into the cross and then relax
a little; yes to an overshoot after the strike (new montage segment). Antigravity/Gemini not on the laptop: our own proposal.
Identity traits unchanged. Cards: k6/make_k6_cards.py (startup re-keyed on its build frames 0/4/8/11/14/15 = K5 sampled + K6 layers).
Montage AM_OverlordDropkickK6: Startup K6 (16 f) + SETTLE K6 (12 f, once) + Loop K5 x (LOOP_REPS-1) + Land: same length, same timeline frames.

| frames | beat | timing / easing | what moves (K6 layer on K5) |
|---|---|---|---|
| f0 -> f4 | arms SLAM into the fold | charge_long, arrive hard | PRESS at f4: clavicles +12 deg protraction, hands 3 cm further across, spine curls 3 deg: elbows 3-4 cm toward the midline, chest hollows around the arms. Head LAGS: 6 deg less tucked than the body |
| f4 -> f8 | arms let go a little | charge_long | RELAX at f8: clavicles -4 deg vs K5, wrap 1 cm looser, chest opens 1 deg. Head OVERSHOOTS the tuck by 6 deg; feet 2 cm higher (the legs' weight arriving in the ball) |
| f8 -> f11 | the load | charge_long | arms gather again (half a press), head back on K5's -40 |
| f14 -> f15 | smear, STRIKE | 1 f each | legs, pitch, smug flick = K5 (crisp, no delay). The lying fold CLAMPS on the impact: clavicles +8, wrap 2 cm deeper |
| settle f0 -> f2 | body carried PAST the lock | arrive_1_2 (fast leave, slow-in) | pelvis 82 -> 87, hips 2 cm on, chest out 4 deg, toes -50 -> -60; head held (lag); arms clamped |
| settle f2 -> f4 | head follows through | hold_recoil | head flicks past the smug attitude: chin +6, turn +6, tilt +3; body starts back; arms 35% loose |
| settle f4 -> f7 | rebound | hold_recoil | pelvis under the hold (79.5), toes -46; head dips 3 deg under the smug attitude (a nod); arms LOOSE (clav -3, wrap 1 cm looser) |
| settle f7 -> f11 | settle | arrive_1_2 | = loop f11 of K5, so settle f11 -> loop f0 is the loop's own wrap step |
Order of the follow-through: pelvis and legs first (f2), head 2 frames later (f4), arms last (f4-f7). Legs stay locked straight (only pitch and toes move).
Offline (make_k6_cards.py, fold3 shapes): min clearance 0.22 cm at f4 (press), every key and blend >= 0.22 except the f0 -> f4 blend,
which the build does not take (the build interpolates f0 -> f4 itself, as in K5).
OWNER POSE DIRECTION (2026-11-21 18:39Z, rule 13a; scope: the strike / extended pose = startup f14 (2/3) and f15, the settle and the
fall loop; not the tuck, not the landing): torso pushed BACK and bent, legs FORWARD, never lower than the body's plane, like Big Band's
j.HK. K6 line: pelvis +8 deg further back, hips flexed 12 deg (legs still locked straight and together), trunk curled 8 deg up toward
the legs: a shallow V with the hips lowest. Offline (Poser): legs 6.7 deg ABOVE the horizontal (K5: 13.6 deg below), balls 34 cm higher
relative to the hips (96 vs 62 cm, hips at 81), lower trunk 172 deg (K5 168), chest-head line unchanged (smug head kept). Settle peak:
legs +11.7 deg; rebound +4.2 deg: never below the horizontal. Fall loop = K6 loop (K5 smug hold on the new line).
HITBOX NOTE for the owner (not touched): the feet at the strike are ~34 cm higher relative to the hips than K5.
Gemini notes (docs/notes/reviews/dropkick-kick-k5/gemini-k6.md) folded in: settle peak 1-2 f after the strike and rebound sooner, toes
overshoot then relax, head chin +6 peak; not taken: spine wave on the tuck (startup has only 6 build keys), forearm drag at f14
(clearance), loop sway with a 24 f period (the loop is 12 f), landing (other clip, code queue).
Landing: a 480 cm press lands at a19-a20 = settle f3-f4: the overshoot peak shows, the rebound only on a high press.
