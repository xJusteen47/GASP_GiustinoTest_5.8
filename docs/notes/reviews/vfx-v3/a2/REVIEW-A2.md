# VFX review: landing dust A2 r1 — FAIL — 2026-11-19

Capture chain: **incomplete, enough for a FAIL, not for a PASS** (skill §2). Present: game camera, light and dark floor, 60 fps frames with labels, real-speed + x0.25 video (`video/A2_atterraggio_A1b_vs_A2_luceGASP.mp4`, read as sampled frames). Missing: side view, VFX-off pass, low preset (preset not stated), A1b under the same GASP lighting (pairs are valid for size and look continuity only), a blind critic on r1 (only the r0 critic exists). Frame offsets below use touchdown = f158 (brief); 1 px = raw 1920x1080.
Note: dust is first visible at f156 in both A1b and A2 (sim "land on v155"), so the A2 labels look one frame late; negligible.

Real-speed impression: on the dark floor a small, soft, light-grey skirt at the feet, gone in ~0.45 s, pleasant but it reads as white smoke; on the light floor nothing reads, the landing looks like it has no effect.

| question | yes/no | frame and view | what is seen |
|---|---|---|---|
| 1 message | NO (light) | light f161-f178 | darkest dust pixel 204-208 on floor 218 (5-7 %), typical <= 2 %, left half absent: no landing read |
| 2 character readable | yes | dark f166, light f161-f173 | feet and shins read on both floors (no VFX-off pass to confirm) |
| 5 orientation / clipping | NO | dark f159, f161, f163 (zoom) | straight-edged dark rectangle and a hard horizontal line across the dust base under the feet |
| 7 life curve | yes | dark f158-f184 | width 190 px (+0) -> 276 (+15) while value drops 112 -> 91; then width frozen 276-279 to +24 (minor) |
| 8 clean death | yes | dark f178-f196 | alpha reaches 0, no veil, no opaque shrink; a small right wisp lives to +38 |
| 10 value / background | NO | dark f166, light f166 | dark: dust (112,114,123) on floor (29,31,43) = 3.3x; light: 0.95-1.0x. Strong on one floor, absent on the other |
| 11 matter and light | NO | dark f161-f173 | two-tone shading now present, but the dust is neutral light grey (B/R 1.10) on a navy floor (B/R 1.42): white smoke, not that floor's dust |
| 12 quality bar | NO | both views | vs DK Bananza landing puffs: holds = small, low, soft, disperses; does not hold = not coloured by the floor, lost on the light floor |

Size target (owner_size_target, A1b early frame = 260 px at +0, ~300 px at +1, top at mid-shin ~45-55 px): **width met**, A2 starts at 190 px (0.7x) and peaks at 276-279 px (+15..+20) = 0.93-1.07x, never clearly above. **Height met until +11** (30-60 px), then the right lobe rises to the knee line (85-95 px, standing knee = 90 px) at +15..+26 while still at ~70 % of peak value at +15: over the rule late.

Comparison: DK Bananza (`dk_sheet_c`, TV capture; landing puffs barely resolved, so the bar rests on §7.1 and the owner notes): there the puff is the snow's own colour and stays below the knee. A2 matches the scale and softness, not the material colour. A1b continuity: same family and softness, clearly smaller and lower than A1b at every pair (A1b +8..+20 is 1.5-2x wider); look kept on the dark floor.

Critic r0, answered on r1 frames:
1. major, legs melt into the floor (light): **fixed** (light f161-f173 shins and feet fully clear; dark f166 feet lighter than dust). Overshoot: see gap 1.
2. major, single-tone unlit haze / rim-lit shell: **fixed in part** (dark f166, f169: lit tops, darker undersides, base no longer the brightest). Residual colour issue = gap 1; new base artefact = gap 2.
3. medium, growth then fade in sequence: **fixed in part** (fade overlaps growth to +15); accepted as minor: last 10 frames are a still patch dimming.
4. medium, white pellets: **reduced, accepted as minor** (dark f159-f163: ~5-6 px, ankle height, gone by +11, still pure white and brighter than the dust).
5. medium, two mirrored lobes: **rejected for +3..+11** (dark f163, f166: front band joins unequal lobes, reads as a skirt); from f173 it is two side lobes again (minor).
6. minor, dust/floor value ratio differs per floor: **accepted, now major** under GASP light (3.3x dark, ~1.0x light) = gap 1.
7. minor, noise sharper while fading: **accepted, minor** (dark f178, f184, f188: thin crisp filaments in the right wisp).
Evidence gap of r0 (both floors same exposure): closed, floors now expose at L 30-35 and L 218.

Gaps (max 3, gravest first):
1. **colour-value / material** — dark f161-f173, light f161-f178, game view; §6.7, §7.1, §8.6, §5.6-bis. Dust value is not tied to the floor. Fix (colour only, no size or opacity change): dark = lower `DarkSnow` (and `Brightness` on dark) until the lit top is ~1.8-2.0x the floor (L 60-70 here, now 93-116) and keeps the floor hue (B/R >= 1.3); light = darker `ShadeColor` so the shaded underside composites at ~0.80-0.85x the floor (L 175-185, now 204-218) on both lobes for +3..+11. Keep `OpacityMax` 0.75.
2. **material (clipping)** — dark f159, f161, f163 zoom; §8.7, §2.12. Hard rectangle / straight line in the dust base. Fix: find which layer draws the straight edge (contact disc / contact shadow quad over the puffs, or puffs cut by the floor), then depth-fade the puffs at the floor or soften that layer's mask so no quad edge sits inside the dust at +1..+5. Question for the UE assistant: sort order and depth fade between the contact layers and the puff sprites.
3. **shape / size** — dark f173, f178, f184; owner rule "only grow slightly while fading", §8.3. Late rise to the knee line. Fix: cap the late growth so the top stays <= ~60 px (~35 cm): puff size max 90 -> ~65 cm (ring 26 -> ~30 cm if width must hold), or no upward drift after +8; all puffs at alpha 0 by +27 (right wisp now lasts to +38).

Game side: dust spawns at the sim landing frame (f156), 1-2 frames before the foot is flat (f158); inherited from A1b, check that the landing event vs foot plant offset is intended. Not a round.

Decisions for the approver:
- The light test floor exposes near white (L 218) under GASP light. Accept a subtle shaded read there (real dust on a white floor is faint), or lower the light floor albedo of the test spot? Gap 1 light target assumes the first.
- Next capture (r2): A1b under GASP light on the same frames, a side view, a VFX-off pass, and a blind critic on r2.
