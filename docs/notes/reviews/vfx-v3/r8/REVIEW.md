# VFX v3 r8 review (Fable, 2026-11-19)

Input: the 12 `*_vs_astro_N.png` sheets in this folder and 6 ref-clip key frames (hit_sparkle, hit_ring, dust_ring, landing_puffs, landing_dust, landing_ring). Stills only, each seen once, at sheet scale (one game frame is about 340 px wide). The .webp videos were not opened. Depth marker, hub props and parked mannequins ignored.

## Verdict: FAIL (round), very close. 11 of 12 sequences pass. Sprint lines are back and read on both floors; the dark cloud is now lumpy. One blocker: the big bounce cloud on the light floor reads grey, darker than the floor.

| Sequence | r7 | r8 | Same game as astro_N? |
|---|---|---|---|
| light_hit_debris (astro_1) | PASS, note | PASS, note (streaks barely shrink) | Yes |
| dark_hit_debris (astro_1) | PASS, note (streak fan) | PASS, note (fan shorter in time, not in count) | Yes |
| light_hit_medium (astro_1) | PASS | PASS | Yes |
| dark_hit_medium (astro_1) | PASS | PASS | Yes |
| light_run_skid (astro_3) | PASS, thin | PASS | Close: pale puff, lighter than the floor |
| dark_run_skid (astro_3) | PASS, note | PASS, note (slight warm cast) | Close |
| light_ground_bounce (astro_3) | FAIL (shape, capture) | FAIL (colour) | No: grey haze on a white floor |
| dark_ground_bounce (astro_3) | FAIL (shape, colour) | PASS, note (2 frames late) | Close: 3-4 separate lobes |
| light_jump (astro_3) | PASS | PASS | Yes |
| dark_jump (astro_3) | PASS, note | PASS | Close |
| light_sprint (astro_2) | FAIL | PASS, thin | Close: 3-4 thin lines, low contrast |
| dark_sprint (astro_2) | FAIL | PASS | Yes |

## 1. Reference comparison

**Sprint (change 1).** Verified. Lines are visible at v205, v242 and v279 on both floors, none at v168: 3-4 thin white-cyan horizontal lines trailing about one body length behind the torso. On dark they read at once. On light they are visible because of the cyan tint, but they are thin and pale: this is the lower limit. Same family as the thin blue-white arcs in ref-clip dust_ring and the thin white line in astro_2. The lines are still there at v279, 9 frames after the stated end at v270 (linger or log offset, not a fault).

**Impact / bounce cloud on dark (change 2).** Verified. v77 shows 3-4 lobes of different size, v120 shows 4 lobes with a darker underside, v108 and v112 show 2-3 lobes next to the ring. This reaches the r7 target (old dark v76, separate lumps). Size is body-sized or larger. Each lobe is still a soft fill, so the volume comes from the overlap, not from the lobe itself: acceptable. The cloud is not visible on the logged contact frame (v106); it first appears at v108.

**Impact / bounce cloud on light (changes 2 and 4).** Fails. At v76 and v119 the cloud is a grey haze that is darker than the floor, with lobes that are hard to separate; at v107-v111 it is close to invisible. The deeper self-shading (ShadeMin 0.62, rim 0.68) is my guess at the cause: on a floor this bright the lit part equals the floor and only the shaded part shows, so the whole cloud reads as grey smoke. It is mild, not charcoal, but it is the opposite of astro_3, where the burst is brighter than the snow. Confidence is medium at this scale: confirm on `light_ground_bounce_close.webp`.

**Dust on dark (change 3).** Verified. Run start v44, skid v142, turn v167, landing v157-v169: grey-khaki, clearly lighter than the floor, no longer cream, no soot. A slight warm cast remains against the cool grey floor. The sticker look of r7 v119 is gone.

**Dust on light (change 4).** Verified, no regression. Run start v44 is a pale round puff, lighter than the floor, with a readable shape (better than r7). Skid v142 and landing v161-v169 are faint pale haze, low and wide. No soot in any dust puff. Matches ref-clip landing_dust and dust_ring (thin, floor-toned).

**Strong-hit streaks (change 5).** Partly verified. Dark: 6-8 streaks at v37-v40 at about 80-100% length, about 60% at v42, 4-5 short thin ones at v45. The fan now clears in about 8 frames with a visible shrink, but the count is not lower and it still crosses the victim's thighs for 4-5 frames. Light: 4-5 streaks, still long at v41 and only a little shorter at v44; the earlier shrink is not visible here. Star: full size with white core and orange fringe on frame 1 (light v35, dark v36), gone on frame 2, as in r7.

**Grains and sparkles (change 6).** 2-4 pale dots are visible near the ring at light v48 and dark v49, plus the red debris dots. Better than r7 (1-2), still far from the 10-15 white dots of ref-clip hit_ring and landing_dust.

**Hits medium, landing, skid.** No regression. Medium star chest-sized for one frame, short streaks to v52-v55. Landing ring at v154, dust from v157, wide and low at v161-v169. Skid still has one sample (v142).

## 2. Remaining fixes

1. **posa/look, bounce cloud grey on light (blocker).** Scale the shade depth of the cloud instance with floor luminance: on floors above 0.6 use ShadeMin 0.62 -> about 0.78 and rim 0.68 -> about 0.76, emissive lift 0.4 -> about 0.55, so the core is at or above the floor's brightness. Keep the dark-floor values. Target: light v119 as pale as the run puff at light v44, with the 3-4 lobes of dark v120.
2. **posa/look, strong-hit streaks.** Cap the count at 4 (dark shows 6-8). Length 50% by frame 4 (now about 80% at dark v40 and at light v44). Check that the shrink curve runs on normalised age for every streak: the light sheet shows almost no shrink in 9 frames.
3. **posa/look, sprint lines thin on light.** Width x1.5-2 or cyan saturation +20%; 5-6 lines instead of 3-4 would sit closer to astro_2. Optional if the owner accepts them at real speed.
4. **gioco/cattura, bounce timing.** The cloud is first visible 2 frames after the logged contact (v107 against v105 light, v108 against v106 dark). Check the spawn frame, or sample contact +1.
5. **posa/look, sparkles.** Add 6-10 white sparkle dots, 4-6 cm, 6-8 frames, on strong hits and landings (ref-clip hit_ring, landing_dust). Carried over from r7.
6. **posa/look, optional.** Dust on dark: desaturate 10-15% to remove the khaki cast, if the cast does not come from the light.
7. **gioco/cattura, skid.** Sample 4 frames in v128-v143 (carried over).
8. **gioco/cattura, sprint.** End the sampling at v270: dark v279 shows parked mannequins and the backdrop edge.

## 3. D1-D21 (judged against v3)

| D | Score | Reason |
|---|---|---|
| 1 | NO (gerarchia) | Dark strong hit: 6-8 streaks cross the victim's thighs at v37-v40. Shorter than r7 (9 frames), same count. |
| 2 | YES | Star, streaks, soft lobed smoke, ring, thin lines; no noise. |
| 3 | YES | Few, large elements; the cloud is 3-4 countable lobes on dark. |
| 4 | YES | Star at full size on the first event frame. Note: bounce cloud 2 frames after the logged contact. |
| 5 | YES | Frame 1 is the biggest and brightest; streaks end within about 10 frames. |
| 6 | N/A | Fade not judgeable from these stills. |
| 7 | N/A | Motion easing needs video. |
| 8 | YES | White core, one orange tint; dust neutral, hit warm. Dark dust no longer cream. |
| 9 | YES (2 of 3) | Strong and medium clearly differ; small tier not captured. |
| 10 | YES (marginal) | Dark cloud reads as overlapping lumps with shaded undersides; on light the volume is not readable. |
| 11 | N/A | Toad .mov not in the inputs. |
| 12 | YES (marginal) | The single uniform blob of r7 is gone; each lobe is still a soft translucent fill. |
| 13 | YES | Dust never competes with the star; the cloud comes after the hit. |
| 14 | NO (colore) | Hits, dust and sprint lines read on both floors; the bounce cloud turns grey on the light floor. |
| 15 | N/A | No hitbox debug in the captures. |
| 16 | N/A | Side camera only. |
| 17 | N/A | No far-camera capture. |
| 18 | N/A | Needs video. |
| 19 | N/A | One kit shown. |
| 20 | YES | Run start puff at v44, landing ring at v154 and dust by v157. |
| 21 | N/A | Debris too small to follow in stills. |

## 4. Not judged

- The floor contact of the bounce dummy (known capture limit); whether v105/v106 is the real contact frame.
- The exact tone of the light bounce cloud: judged at sheet scale only, medium confidence.
- Skid burst of 3 and steady rate; grain size 9-12 cm; sprint line fringe colour and exact start/stop frames (v170-v270).
- Motion at real speed: lobe stagger (2-3 and 5 frames), smoke growth and fade, streak shrink curve, hitstop sync.
- Small hit tier, far camera, back and 3/4 views, hitbox match, performance.
- ref-clip light_beam_t15 and sparkle_t16 were not opened.
