# VFX v3 r9 review (Fable, 2026-11-19)

## Verdict: PASS (round). light_ground_bounce PASS: the cloud is now lighter than the white floor, no soot. The other 11 sequences stand from r8.

Input: `r9/light_ground_bounce_vs_astro_3.png` and the r8 sheet of the same name, each seen once at sheet scale (one game frame is about 340 px wide), plus a luminance sample of both sheets (12 px cells, 0-255). The ref-clip still and the .webp videos were not opened. Only light_ground_bounce was judged; the dark result is taken from r8 because the dark values did not change.

## What I saw (r9 frames v20, v48, v77, v105, v106, v108, v112, v120)

- **Colour (the r8 blocker): fixed.** Measured against the floor next to it:
  - v77: cloud 145-151, floor 138-141.
  - v112: cloud 150-162, floor 145-148.
  - v120: cloud 150-161, floor 144-148.
  - r8 for comparison: v76 cloud 125-133 on a 135-140 floor, v119 cloud 133-140 mixed with 148-150 on a 145 floor.
  The cloud is now 6-10% brighter than the floor everywhere it shows, and no cell of it is darker than the floor. It reads as pale floor-coloured dust, like the run puff, not as grey smoke. Same direction as astro_3 (burst brighter than the snow).
- **Soot: none.** The only dark pixels near the cloud are the mannequins and their cast shadows.
- **Shape: still lobed, but soft.** v77 shows 2-3 separate pale lumps low and wide to the left of the attacker; v120 shows a rounded mass with a bumpy outline beside the torso. With only 6-10% contrast the lobes are harder to count than the 3-4 lobes of dark v120. Medium confidence at this scale.
- **Size: body-sized.** v77 about 120 px wide (wider than the body), v120 about 50 x 85 px against a body about 75 px tall.
- **Timing: unchanged.** Nothing visible at the logged contact v106, close to invisible at v108, clear at v112 and v120.
- Hit at v48 (ring, red debris, pale dots) and the rest of the sheet: no regression.

## Notes carried from r8 (none blocking)

1. **posa/look, light cloud contrast (new, optional).** The lobes are only 6-10% over the floor. If the owner wants them to read more at real speed, darken the lobe undersides slightly on the rim only, never below the floor value.
2. **posa/look, strong-hit streaks.** Cap the count at 4, length 50% by frame 4, shrink on normalised age (light sheet showed almost no shrink).
3. **posa/look, sprint lines thin on light.** Width x1.5-2 or cyan +20%, 5-6 lines.
4. **posa/look, sparkles.** 6-10 white dots, 4-6 cm, 6-8 frames, on strong hits and landings.
5. **posa/look, optional.** Dust on dark: desaturate 10-15% (khaki cast).
6. **gioco/cattura, bounce timing.** Cloud first readable 2 or more frames after the logged contact (v108-v112 against v106). Check the spawn frame or sample contact +1.
7. **gioco/cattura, skid.** Sample 4 frames in v128-v143.
8. **gioco/cattura, sprint.** End the sampling at v270.

## Not judged

- Motion at real speed (lobe stagger, growth, fade), the close-up .webp, the floor contact of the bounce dummy.
- The dark floor in r9 captures: not in the inputs; accepted on the stated maths (blend weight 0 below surface G 0.4).
- D1-D21 were not re-scored. D14 (colour on both floors) was the r8 NO tied to this blocker and should now be YES; D1 (streak count) stays NO as a note.
