FAIL (narrow; posa + gioco) - generic run start g2, round 2. Trait 3 passes, traits 1-2 are only half delivered.

Coverage limit: the mp4 starts at v60, so my 8 video frames landed on v148-v228 (late rev, hand-off, loop).
The entry and the rev-up (v84-v130) are judged from the three grids and the Cascadeur sheet only, not from video.
Cadence in motion is therefore inferred from poses; items 1-3 deserve one human look at the mp4 before the next round.

## Life
1. Rev-up is front-loaded and dies half-way - casc f2-f8 vs f12-f24; grids v112-v180. FAIL, posa.
   Casc f2-f8: deep crouch, torso far forward, big arm pump: reads as energy. By f12 the torso is almost upright and
   f13-f24 look like the plain loop. In game, from v112 on the g2 row is indistinguishable from the base row.
   So the "engine" lasts about 0.35-0.4 s and there is no settle: the extra energy is gone before the cadence is.
   Fix: keep the lean and arm envelope at full until about f14 and ease it out over f14-f21 (not a ramp from f5);
   keep 2 clearly high knees (the f12 knee is the only one that reads) on the 2nd and 3rd step.
2. Entry snap - casc f0-f2; grid az90 v84-v92. Posa, minor.
   f0 upright idle, f1 nearly the same, f2 already in the deepest crouch and lean: about 2 frames for the whole drop.
   No anticipation is correct here (owner rule), but the drop has no arc: pelvis and chest arrive together.
   Fix: spread the drop over f1-f4, pelvis leads (down at f2), chest lean peaks at f4-f5, head drags one frame.
3. Weight / follow-through - casc f8-f12. Posa, minor.
   The torso goes from far forward to upright in about 4 frames while the legs keep the same rhythm: reads as a bob,
   not as a body settling into the run. Covered by the fix in item 1 (longer ease, chest overshoots upright by 2-3 deg).
4. Root vs capsule - grid_game v84-v180, az90. OK.
   Same travel as the base (actor_x_f200 1017.0 both), no drift off the shadow, no visible lag or lead.

## Identity traits
5. Trait 1, frantic from frame 1 - grid az90/az0 v88-v96. Present in the clip, damped in game. Gioco.
   g2 has a knee up and a bent stance at v88 while the base still stands until v96: no planted pause, good.
   But the clip weight is 0.29 / 0.5 / 0.87 at v85 / v90 / v95: the fastest part of the rev (first step) plays half
   blended with the idle, so the most frantic frames are the least visible. Not a pose problem.
6. Trait 2, short steps and high knees settling - see item 1. Half delivered, posa.
7. Trait 3, no clash - video v148-v187, weight 1.0 -> 0 over v160-v175. PASS.
   v156 and v175 are the same passing pose 19 frames apart (normal loop step), v161 / v167 continuous, no hitch,
   no wrong-foot step, no pop on the blend out. Idle -> start shows no pop in the grids (v84-v92).

## Feet vs ground
8. Planted-feet travel f80-240: g2 L 102.9 / R 78.6 cm vs base 43.3 / 93.5. Total 181 vs 137 (+33%), left x2.4.
   Dati montage: the summary does not say where the left-foot travel sits. Some spin is intended, a long slide is not.
   Needed: per-contact travel for the left foot. Under about 25 cm per contact = intended churn; one contact over
   about 40 cm = treadmill, then shorten that stride or lift the foot earlier (posa).

## Technical
9. ASSERT as reported in the brief (weight >= 0.8 on f97-140): consistent with metrics (1.0 from v100 to v155). OK.
   ss180_max_w 0 on both runs. The g2 clip leaves at v160-v175, the base holds until v200-v210: expected, shorter clip.

## Round-2 check
Hand-off and the missing planted pause are fine. The added lean and arm swing work in f2-f8 but the envelope makes
them fade too early (item 1); the left-foot travel is the one number that got worse and is not explained (item 8).
