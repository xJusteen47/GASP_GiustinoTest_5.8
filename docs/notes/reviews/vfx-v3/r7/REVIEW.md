# VFX v3 r7 review (Fable, 2026-11-19)

Input: the 12 `*_vs_astro_N.png` sheets in this folder and 6 of the ref-clip key frames (hit_sparkle, hit_ring, dust_ring, landing_puffs, landing_dust, landing_ring). Stills only, each seen once. Depth marker and hub props ignored. Note: `ref-clip/NOTE.md` holds the astro_1-13 notes, not the clip timings.

## Verdict: FAIL (round), close. Hits, landing and dust colour on light pass; sprint lines are missing and the big cloud is a flat blob.

| Sequence | r5 | r7 | Same game as astro_N? |
|---|---|---|---|
| light_hit_debris (astro_1) | PASS, note | PASS, note | Yes |
| dark_hit_debris (astro_1) | PASS, note | PASS, note (streak fan) | Yes |
| light_hit_medium (astro_1) | not in r5 | PASS | Yes |
| dark_hit_medium (astro_1) | PASS | PASS | Yes |
| light_run_skid (astro_3) | FAIL (colour) | PASS, thin | Close: right colour, almost too faint |
| dark_run_skid (astro_3) | PASS, notes | PASS, note | Close |
| light_ground_bounce (astro_3) | FAIL | FAIL (shape, capture) | No: faint flat disc, contact not shown |
| dark_ground_bounce (astro_3) | not in r5 | FAIL (shape, colour) | No: opaque beige blob on charcoal |
| light_jump (astro_3) | FAIL capture | PASS | Yes (matches ref-clip landing dust) |
| dark_jump (astro_3) | not in r5 | PASS, note | Close |
| light_sprint (astro_2) | not in r5 | FAIL | No: no line in any of the 8 frames |
| dark_sprint (astro_2) | PASS on blue | FAIL (regression or capture) | No: no line visible, also on blue |

## 1. Reference comparison

**Hit.** Strong star: full size, white core, orange fringe on frame 1 (light v36, dark v35), gone on frame 2. Change 3 is verified and goes further than the goal (30% by frame 4): the star is a one-frame flash. It still reads, and both torsos are now clear during hitstop, so I accept it. New note: on the dark sheet a fan of 5-7 long streaks stays at full length from v36 to v44 and covers the victim's hips and legs; the light sheet shows 3 streaks and is fine. Medium star is chest-sized, one frame, small streaks to v55: tiers are distinct. The white ring at v48 (dark) is in the family of ref-clip hit_ring. Small white sparkle dots (ref-clip hit_ring, landing_dust) have only a weak counterpart at light v49.

**Dust on light.** Change 1 verified: the puffs (run start v44, skid v142, landing v156-v168, hit cloud v49) are now the floor's colour, no soot. They are close to invisible: only a slightly lighter haze, no visible darker rim. The ref-clip dust is equally thin, so this is acceptable, but there is no margin left.

**Dust on dark.** The puffs are cream/tan and much brighter than the charcoal floor. They read very well, but "the floor's colour" is not met here: EmissiveLift 0.5 (my guess at the cause) lifts them too far. At v119 the cloud looks like an opaque sticker.

**Impact / bounce cloud.** Change 2 verified for size: the cloud is now body-sized or wider (about 150-200 cm on screen at dark v48, v111, v119). Shape fails the owner's 11-19 note: at v107-v119 it is one flat blob with a uniform fill, no lobes and no internal shading. Dark v76 shows 4-5 separate lumps and is the closest to billowy. Grains: one or two dots at most; the 10 grains are not visible at sheet scale. At v105 the dummy is horizontal at waist height next to the attacker and the cloud appears beside them at v107: the floor contact itself is not visible (see section 4).

**Landing dust.** In frame now (change 6 verified: walls and floor visible, 4 frames around v154). Dark: low, wide, tan, about 1.5 body widths at v160-v168, good shape. Light: very faint light haze. This matches "keep it, but light" and ref-clip landing_dust. PASS.

**Skid.** At v142, inside the skid window, a puff sits at the feet on both floors: dust during the slide is verified. The burst of 3 and the steady rate cannot be verified with one sample in v128-v143; what is visible is one puff of about 50 cm. The v167 puff (run start of the turn) is still there on dark.

**Sprint.** No white line in any frame of either sheet, on grey, dark or blue floor. r5 showed lines on blue at v205 and v242; here those frames show only a small dust puff at v205. The deeper blue fringe (change 5) cannot be verified. The second mannequin is still in frame at v168-v242.

## 2. Remaining fixes

1. **gioco/cattura (or dati), sprint lines missing.** Check that the speed-line cue still fires in the sprint sequence (it did in r5). Sample at least 3 frames with lines on each floor; move the second mannequin out as done for run_skid.
2. **posa/look, cloud shape.** Impact/bounce cloud: break the single blob into 5-7 overlapping lobes of different size (100-240 cm), stagger birth over 2-3 frames, add per-puff shading (lit top, rim 0.8 on the underside) so lobes separate. Target: dark v76, not dark v119.
3. **posa/look, dust too bright on dark.** EmissiveLift 0.5 -> about 0.2-0.25, or scale the lift with floor luminance; OpacityMax 1.0 -> about 0.8 for the big cloud. On light, raise the rim contrast slightly (0.8 -> about 0.7) so the puff has an edge.
4. **posa/look, streak fan on strong hit (dark).** 5-7 full-length streaks for 9 frames over the victim's legs: cap at 3-4, shrink to 50% by frame 4.
5. **posa/look, grains.** Not visible at 5-8 cm: try 8-12 cm or brighter than the cloud by 15-20%; add 6-10 white sparkle dots on strong hits and landings (ref-clip).
6. **gioco/cattura, bounce.** Show the frame where the dummy touches the floor; if v105 is correct, the cloud spawns 2 frames late and beside the body.
7. **gioco/cattura, skid.** Sample 4 frames in v128-v143 so burst and rate can be judged.
8. **posa/look, optional.** Star life 0.042 -> about 0.06 s if the owner finds the one-frame star too short at real speed.

## 3. D1-D21 (judged against v3)

| D | Score | Reason |
|---|---|---|
| 1 | NO (gerarchia) | Star is fixed; the dark strong-hit streak fan covers the victim's legs for 9 frames. |
| 2 | YES | v3 vocabulary: star, streaks, soft smoke, ring; no noise. |
| 3 | YES | Few, large elements everywhere. |
| 4 | YES | Star at full size on the first event frame. |
| 5 | YES | Frame 1 is the biggest and brightest; flash and streaks end within about 10 frames. |
| 6 | N/A | Fade not judgeable from these stills. |
| 7 | N/A | Motion easing needs video. |
| 8 | YES | White core, one orange tint; dust neutral, hit warm. Dark-floor dust is too cream (fix 3). |
| 9 | YES (2 of 3) | Strong and medium clearly differ; small tier not captured. |
| 10 | NO (forma) | v3 reading: smoke should look volumetric; the big cloud is a flat blob. |
| 11 | N/A | Toad .mov not in the inputs. |
| 12 | NO (materiale) | The uniform soft translucent blob at dark v119 is the "generic" signal. |
| 13 | YES | Dust never competes with the star; the cloud comes after the hit. |
| 14 | NO (colore) | Hits read on both; dust barely reads on light; sprint lines on neither. |
| 15 | N/A | No hitbox debug in the captures. |
| 16 | N/A | Side camera only. |
| 17 | N/A | No far-camera capture. |
| 18 | N/A | Needs video. Streaks hold during hitstop, which is consistent (guess). |
| 19 | N/A | One kit shown. |
| 20 | YES | Run start puff at v44, landing dust within 2 frames. |
| 21 | N/A | Debris too small to follow in stills. |

## 4. Not judged

- Whether the dummy touches the floor at v105 (bounce); the contact frame is not readable.
- Skid burst of 3 and steady rate; grain size 5-8 cm; sprint fringe colour.
- Motion at real speed: smoke growth and fade, star at 0.042 s, streak travel, hitstop sync.
- Small hit tier, far camera, back and 3/4 views, hitbox match, performance.
- ref-clip light_beam_t15 and sparkle_t16 were not opened.
