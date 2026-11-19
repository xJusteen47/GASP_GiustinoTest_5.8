# VFX v3 r4 review (Fable, 2026-11-18)

Input: NOTE.md (owner refs), v3 design sheet, 8 side-by-side sheets in this folder. Stills only, 60 fps frame numbers.

## Verdict: FAIL (round)

The hit flash is close to the reference. Dust, the ground ring and the capture framing are not.

| Sequence | Verdict | Same game as astro_N? |
|---|---|---|
| light_hit_medium (astro_1) | PASS with notes | Yes for the star, weak on colour and size |
| dark_hit_debris (astro_1) | PASS for the flash, FAIL for the debris | Star yes, no debris or dust visible |
| light_hit_debris (astro_1) | FAIL | Identical to hit_medium, nothing "debris" in it |
| dark_ground_bounce (astro_3) | FAIL | No: white ring + tiny grey puff vs big pink cloud |
| light_run_skid (astro_3) | FAIL | No: dust nearly invisible |
| dark_run_skid (astro_3) | FAIL | No: one faint puff, far too small |
| light_sprint (astro_2) | PASS on blue floor, not judged on light floor | Yes for the lines on blue |
| dark_jump (astro_3) | FAIL (capture) / rule "no jump dust" respected | Not comparable |

## 1. Reference comparison

**Hit (C1).** Good: crisp white 8-ray star at full size on the first frame (v26/v46), short streaks, sparks at +9 frames, no ring, no balls. It reads on both floors. Gaps against astro_1:
- Star is about hand-to-chest size; in astro_1 the flash is as large as the character.
- Fringe and streaks are pale cream/peach. astro_1 has saturated pink/orange streaks around a white core. On the light floor the streaks almost merge with the core.
- Star still about 50% size at +4/+6 frames (v30, v32): the sheet asks large to zero in 3-4 frames.
- hit_debris and hit_medium look the same: no chunks, no dust cloud in any of the 8 frames.

**Dust (B1/B2).** Not the same game. astro_3 is a cloud wider than the character, material-coloured, with fine grains. Ours is 1-2 small grey smudges (roughly 20-30 cm), low opacity, sitting 1-1.5 m behind the character at run start (v44), and almost nothing at the skid (v142 none, v167 a faint puff). On the light floor it is practically invisible; on dark it is visible but tiny. No grains visible.

**Ground bounce.** A white ring is on the floor at v49 and v121. Rings were removed by owner request. The puff is small, grey and floats at chest height rather than spreading low on the ground.

**Sprint lines (C2).** Thin bright white lines read well on the blue floor (v205-v279). They are pure white: no blue fringe. No line appears in the frames on the light floor (v20-v168), so light-background readability is unproven.

**Contact shadow (D).** A dark ellipse is under the feet in dark_jump v20. In the other sheets it cannot be told apart from the directional cast shadow. In the jump, the ground shows a white ring with a pink disc and a white vertical line for 70+ frames: if that is the airborne contact shadow, it is the wrong look (should be a dark blue-violet disc fading with height).

## 2. FAIL items and fixes

1. **posa/look, dust too small and too transparent (run start, skid, bounce).** NS dust systems (B1/B2): sprite size x3-4 (target 80-120 cm at peak for B2, 150-200 cm for B1), peak opacity up to about 0.6-0.7, count 3-5 puffs + 3-4 grains as in the sheet, grain size up to stay above 2 px at game distance. Spawn at the foot, not behind: spawn offset to 0, initial velocity backwards with strong drag.
2. **posa/look, dust does not read on the light floor.** Smoke at 0.88,0.91,0.97 on a floor of the same colour has no contrast. `M_GSTN_VFX_Smoke`: darken the shaded underside (multiply surface colour by about 0.6-0.7 on the lower half or by the normal away from light) and add a slightly darker soft rim, so the puff reads by shading as in astro_3.
3. **dati, white ring on ground bounce / landing.** Remove the ring emitter or cue row still bound to bounce/landing in `DA_GSTN_Presentation_Default` (`setup_presentation_table.py`); replace with the low radial puff of B2 scaled to B1 for the bounce.
4. **dati, hit_debris has no debris and no dust.** The strong-hit row must add the A debris emitter (3-5 small shards, surface colour) and the B1 cloud. Check that the row for the debris tier points at a different system from the medium one.
5. **posa/look, hit colour and size.** `MI_GSTN_VFX_Flash`/`Streak`: fringe saturation up to about 0.9 with the kit accent colour, fringe width about +50%; streak colour fully accent, not cream. Star start scale x1.5 (M) and x2 (L); scale curve to zero by frame 4. Give S/M/L visibly different sizes.
6. **posa/look, sprint lines.** Add a thin blue fringe (about 1 px each side) so they survive on white/snow.
7. **gioco/cattura, jump and bounce framing.** The character leaves the frame (jump v32-v104, bounce v63-v92), so landing and bounce impact cannot be reviewed. Camera must follow Z or be pulled back to hold the full arc. Hide debug draws (dashed vertical line, level labels) or confirm they are VFX.
8. **gioco/cattura, sprint on light floor.** Start the sprint earlier or move the lane, so at least 3 frames show the lines over the light floor.
9. **gioco/cattura, "light" floor is mid grey in the hit sheets.** The hit is tested on grey and dark only. Add a truly light surface (snow-like, about 0.9) for the hit, since astro_1 is on snow.

## 3. D1-D20

The D1-D20 list is in `stile-sheet-vfx-review.md`, which was not in the reading list, so the items are not scored one by one. General clarity points seen: hit position and direction read clearly; the effect does not cover the characters; first frame is the strongest (good). Dust fails any "reads at game distance" item.

## 4. Not judged

- Motion at real speed (WebP not playable): smoke expansion and fade, streak travel, hitstop feel.
- Landing dust (must be absent on jump by owner rule): no dust seen at take-off, landing not in frame.
- Sprint lines and contact shadow on a light floor; contact shadow fade with height.
- Performance (particle counts, overdraw).
