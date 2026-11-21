PASS (posa, narrow) - generic run start g3, round 3 of 3. Both g2 pose FAILs are fixed; 2 non-pose items stay open (6, 8).

Coverage: 3 grids + Cascadeur sheet once, 8 video frames (v88 v94 v100 v106 v112 / v156 v166 v175, all 6 views).
Cadence and smoothness in motion are inferred from stills 6 frames apart: one human look at the mp4 at v84-v120 is still due.

## Life
1. Rev-up lasts and releases - casc f4-f14, video v94-v112, grid az90 v112-v136. OK (was g2 FAIL).
   Lean, crouch and arm pump are at full from v94 to v112 (clip f14); at v112 g3 is still clearly not the base row.
   Release v116-v132 is gradual, by v136 the two rows match. Steps land about every 6-8 video frames vs 19 in the loop:
   cadence reads about 2.5x, as "engine", not as a glitch, in the stills.
2. Entry - casc f0-f5, video v88. OK (was minor).
   Drop is spread over f1-f4, knee is up at f2 before the chest is fully down at f4-f5. No pop, no planted pause.
3. Weight during the plateau - video v100-v112 az90/az270. Posa, minor, not blocking.
   With bounce x0.3 the pelvis stays at one low height for about 18 frames and the head points at the floor (v100, v106):
   close to a crouched glide. Fix if the owner reads it as stiff: lean 22 -> 18 deg and bounce x0.3 -> x0.5; nothing else.
4. Release speed - casc f14-f17. Posa, minor.
   Most of the torso rise happens in f14-f16, the last frames to f21 add little. Fix: plateau end f14 -> f13 with the same
   f21 end (longer ease), or leave as is: in game (v116-v132) it does not read as a bob.
5. Root vs capsule - grid_game v84-v180. No drift off the shadow, no visible lag in the picture. But see item 8.

## Identity traits
- Trait 1, frantic from frame 1: PASS. v88 already crouched with the knee up while the base stands until v96.
  Known game item unchanged (weight 0.29 / 0.5 / 0.87 at v85 / v90 / v95): gioco, already queued, not counted.
- Trait 2, short steps + high knees that settle: PASS. High knees at v88, v94, casc f7 and f11-f12; heel folds back at v112;
  strides open up v116-v136. Optional: if the mp4 flickers at full speed, play rate 2.6 -> 2.3 (keep the plateau).
- Trait 3, no clash: PASS. v156 and v175 are the same loop pose 19 frames apart, v166 continuous, weight 1 -> 0 over
  v160-v175 with no hitch and no wrong-foot step. Idle -> start: no pop in grids v84-v92.

## Feet vs ground
6. Planted-feet travel f80-240: g3 L 114.1 / R 71.8 cm (sum 185.9) vs g2 102.9 / 78.6 (181.5) vs base 43.3 / 93.5 (136.8).
   Dati montage, OPEN for the second round: left foot got worse again (+11 cm) and there is still no per-contact
   breakdown. Same rule as g2: under about 25 cm per contact = intended churn; one contact over about 40 cm = treadmill,
   then it becomes posa (shorten that stride or raise knee deg on that step so the foot leaves earlier).

## Technical
7. ASSERT consistent with metrics (weight 1.0 from v100 to v155). ss180_max_w 0. OK.
8. actor_x_f200: g3 968.2 vs base 1017.0 (-48.8 cm); g2 was 1017.0 = base. Dati montage / gioco, v84-v200.
   The brief says stock root motion, so the travel should match. Either the capture input timing changed or the g3 clip
   root ramp is shorter than stock. Needs an explanation before this is called final: if the capsule is slower while the
   legs spin 2.6x for longer, that is also where the extra left-foot travel in item 6 would come from.

## Follow-up checks (thread, after the review; metrics_*.json, per-frame records, python one-off in the thread)
- Item 1 (actor_x_f200 -48.8): NOT root motion. g3 capture spawned at actor_x -48.8 at f0 (base and g2 at 0.0); travel f0-f200
  = 1017.0 cm in base, g2 and g3 alike. Closed.
- Item 2 (per-contact L foot travel, foot z < 13 cm, f80-240): g3 13.6 / 12.3 / 31.9 / 33.3 / 23.3 / 28.7 cm; base 2.2 / 27.1 /
  14.3 / 11.0 / 23.4; g2 12.8 / 16.6 / 30.2 / 22.0 / 21.6. Max 33 cm, below the 40 cm treadmill bar; the two 30+ contacts are at
  v124-168 (end of the release / loop). Open as dati, not posa.
- Thread look at the start (mp4 frame n = v(n+60), az45 strip v87-v120 step 3): knees and arms pump from v90, lean held through
  ~v108, release v111-v117, upright loop by v120 with no hitch. Agrees with the PASS.
