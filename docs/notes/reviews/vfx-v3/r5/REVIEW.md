# VFX v3 r5 review (Fable, 2026-11-18)

Input: 8 sheets in this folder (light/dark hit_debris, dark_hit_medium, light_ground_bounce, light/dark run_skid, light_jump, dark_sprint). Stills only. The GSTNDepthMarker ring and line are ignored as agreed.

## Verdict: FAIL (round), close. Hits PASS; dust on light floor and two captures still open.

| Sequence | r4 | r5 | Same game as astro_N? |
|---|---|---|---|
| light_hit_debris (astro_1) | FAIL | PASS with one note | Yes |
| dark_hit_debris (astro_1) | flash only | PASS with one note | Yes |
| dark_hit_medium (astro_1) | PASS | PASS | Yes |
| dark_run_skid (astro_3) | FAIL | PASS with notes | Close, cloud still thin |
| light_run_skid (astro_3) | FAIL | FAIL (colour) | No: reads, but as dark soot on a white floor |
| light_ground_bounce (astro_3) | FAIL | FAIL | No: small dark blobs, bounce impact not in frame |
| light_jump (astro_3) | FAIL capture | FAIL capture | Landing not in the 8 frames |
| dark_sprint (astro_2) | PASS on blue | PASS on blue, otherwise not judged | Yes on blue |

## 1. Reference comparison

**Hit (C1): fixed.** The strong hit is now character-sized, white core, clear orange fringe, long tapered streaks. It reads on the light and on the dark floor, and it sits next to astro_1 without looking like another game. Strong and medium are now clearly different tiers (medium is chest-sized, v46-v52). After the strong hit (v48) there are sparks, a few small red/blue shards and a puff: the debris tier exists.
- Note (posa/look): the strong star stays near full size from v35 to v44, 10 frames, and hides both torsos for the whole hitstop. The sheet asks large to zero in 3-4 frames. Keep frame 1 as is, then collapse the star to about 30% by frame 4 and zero by frame 6; let the streaks and sparks carry the rest of the hitstop. On the medium hit the timing is already fine.
- Note: the medium fringe is paler than the strong one (cream more than orange). Minor.

**Dust (B2), dark floor: acceptable.** Run start (v44) and skid (v142, v167) show a tan puff about 50-70 cm, soft, material-coloured. It is still thin next to astro_3 (opacity looks about 0.3-0.4 on screen) and no fine grains are visible.

**Dust (B2), light floor: readable but wrong colour.** The puff at v44 and v167 is dark grey to near black on a 0.95 floor. It reads, but it looks like soot or exhaust, not dust lifted from a white surface. astro_3 keeps the cloud lighter than or equal to the ground and gets contrast from soft shading and density, not from darkening. ShadeMin 0.4 over-corrects.
- Skid timing: at v142 (deepest slide pose) there is no dust; the puff appears at v167 when the character is already upright. The skid should dust while the feet slide.

**Ground bounce / strong-hit cloud (B1).** Puffs are 2-3 dark blobs of 20-40 cm near the attacker (v49, v63, v121). astro_3 is one cloud wider than the character. In the 8 frames the dummy is always airborne, so the bounce impact itself is not shown.

**Sprint (C2).** Thin white lines read on the blue floor (v205, v242). No line appears over the dark floor (v20-v168), so the "dark" proof is still missing, as the "light" proof was in r4. No blue fringe visible.

**Landing dust (owner: keep it, light).** Not judged: light_jump ends at v104 with the character still in the air, and the dark mannequin is almost invisible against the black sky. Take-off (v32) shows no dust, which is correct.

## 2. Remaining fixes

1. **posa/look, smoke too dark on light surfaces.** `M_GSTN_VFX_Smoke` / SmokeLight: ShadeMin 0.4 -> about 0.65-0.7, EmissiveLift 0.06 -> about 0.12. Get the contrast from a thin slightly darker soft rim and from opacity (keep 0.7), so the puff stays the colour of the floor.
2. **posa/look, B1 cloud too small.** Strong-hit and bounce cloud: 5-8 sprites of 120-180 cm at peak (now about 30-40 cm), low and wide on the ground, plus 6-10 fine grains.
3. **posa/look, dust on dark still thin.** SmokeDust on-screen opacity up by about a third (check the alpha curve: peak is not reaching OpacityMax 0.85), add 3-4 grains of at least 2 px.
4. **posa/look, strong star dwell.** Scale curve as in section 1 (30% by frame 4, zero by frame 6).
5. **dati, skid dust timing.** Fire the skid cue at skid start (or loop a small emission while sliding), not at recovery.
6. **posa/look, sprint lines.** Thin blue fringe, as in r4 (not addressed).
7. **gioco/cattura, landing.** Extend the jump capture to at least 30 frames after touchdown and sample 4 of the 8 frames around the landing. Use the light mannequin or a non-black sky so the character is visible in the air.
8. **gioco/cattura, bounce.** The sheet must include the frames where the dummy hits the ground (sample around the first ground contact).
9. **gioco/cattura, sprint.** Start the sprint over the dark (and light) floor, at least 3 frames with lines there.
10. **gioco/cattura, run_skid light.** A second mannequin stands in the frame at v20-v69 and overlaps the run-start puff; move it out.

## 3. D1-D20

Not scored: `docs/notes/stile/stile-sheet-vfx-review.md` does not exist in `docs/notes` (glob and grep found nothing). Give the real path and it can be scored next round. General clarity: hit position and direction read; the only clarity issue seen is the strong star covering both characters for 10 frames (fix 4).

## 4. Not judged

- Whether the landing dust reads as light (not in frame).
- The bounce impact cloud at ground contact.
- Motion at real speed (WebP): smoke growth and fade, streak travel.
- Sprint lines on light and dark floors; contact shadow fade with height; performance.
