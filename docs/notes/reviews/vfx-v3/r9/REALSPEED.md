# VFX v3 r9 real-speed check (Fable, 2026-11-19)

## Verdict: FAIL (close). 6 of 8 sheets pass, dark_sprint is marginal, light_sprint fails: the sprint lines are not readable from the game camera on the light floor. No popping, no one-frame flicker, no soot, no cream in any sheet.

Input: the 8 sheets in `r9/realspeed/`, each seen once at sheet scale (one cell is 240 px wide, the character about 35-40 px tall, so roughly 1/8 of a 1080p frame). Thin elements can vanish at this scale: the sprint result has medium confidence. Depth marker, hub props and parked mannequins ignored.

## Per sheet

- **light_hit_debris (v34-v45): PASS, note.** Nothing at v34, star at full size on v35 (white core, orange fringe, chest-sized, not hidden by the victim), gone on v36. 4-6 peach streaks fan down from the hips at v36-v43 with almost no change (hitstop), shorter at v44, 2-3 stubs at v45. Steady, no flicker.
- **dark_hit_debris (v33-v44): PASS, note.** Star on v36 for one frame, 5-6 streaks at v37-v44, still near full length on the last cell: the sheet ends before they clear (8 frames or more over the victim's legs).
- **light_ground_bounce (v105-v127, step 2): PASS, note.** Nothing at v105-v109, faint pale puff at the feet at v111, clear lobes at v113, smooth growth to a body-sized cloud of 4-5 lobes at v121-v127. Lighter than the floor in every cell, billowy, no lobe pops in or out. Still present at v127.
- **dark_ground_bounce (v105-v127, step 2): PASS, note.** Nothing at v105-v109, white ring at the feet from v111 and steady to v127, faint puff at v113, grey-khaki lobes at hip height at v119-v127, growing smoothly. Lighter than the floor, low contrast, about half the size of the light cloud. Slight khaki cast, no cream, no soot.
- **light_jump_land (v153-v164): PASS, thin.** Faint pale puff at the feet at v157-v158, low haze widening at v159-v164. Lighter than the floor, smooth, small (ankle to knee height).
- **dark_jump_land (v153-v164): PASS.** Small khaki-grey puff at the feet at v156 (logged landing +1), grows slowly and stays low to v164. Smooth, no flicker, slight warm cast.
- **light_sprint (v200-v211): FAIL.** No line is readable in any of the 12 cells; at most 1-2 pale specks beside the body at v203-v204. From behind, on a near white floor, the effect is absent at this scale.
- **dark_sprint (v200-v211): PASS, marginal.** 2-4 short pale dashes beside the torso in every cell at v200-v205, 1-2 at v206-v211. They change place each frame and read as flecks, not as lines trailing the body.

## Notes

1. **posa/look, sprint lines from behind (blocker on light).** The lines run along the travel direction, so the rear camera foreshortens them into dashes. Give them screen-space length from behind (longer, or a slight outward fan), width x2, 5-6 lines, and a darker or more saturated cyan edge on floors above luminance 0.6. Target: lines that hold their place for 3-4 frames on both floors.
2. **gioco/cattura, sprint on light.** Before changing the look, confirm on a native-resolution crop of v200-v211: the 8x downscale may hide a 2-3 px line. If the lines read at 1080p, this sheet is a PASS, thin.
3. **gioco/cattura, blue streak in dark_sprint.** A long cyan-blue line sits far left at the floor edge at v202-v204 (growing), is gone at v205 and is back, short, at v207. If it is a sprint line it spawns metres from the body and blinks; if it is level art, hide it.
4. **posa/look, bounce onset.** The cloud is first readable at v111, 5 frames after the logged contact v106, on both floors. It does not pop and it arrives with the visible rebound, so it is not a fail; the r8 stills put the spawn at v107-v108. Front-load the growth (half size by frame 2-3) so it reads on contact.
5. **gioco/cattura, bounce sampling.** Step 2 skips v106, v108 and v110: capture v105-v112 at step 1 to settle the onset, and check whether hitstop holds the cloud small in those frames (my guess).
6. **posa/look, star hold.** The star lives 1 frame (16 ms) on both floors: the minimum. Hold it 2-3 frames, or let it live into the hitstop like the streaks.
7. **posa/look, strong-hit streaks.** Carried from r8: cap at 4, 50% length by frame 4. At real speed they read as a frozen fan across the legs for 8-9 frames.
8. **gioco/cattura, landing ring.** A ring lies on the floor at v153-v154, before the logged landing v155, and is gone from v155 (very clear on dark). Check which event owns it, or whether the landing log is 2 frames late.
9. **posa/look, optional.** Landing dust on light is thin at the game camera; dark bounce cloud is smaller and flatter than the light one; khaki cast on dark dust (r8 note 6).

## Not judged

- Native-resolution detail, fade-out after the last cell of each sheet (bounce cloud after v127, dark streaks after v44, landing dust after v164).
- Even frames of the two bounce sheets, the exact spawn frame of the cloud, hitstop sync, sound.
