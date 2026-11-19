# VFX review: landing dust A2 r3 — PASS (narrow, to the owner) — 2026-11-19

Capture chain: **enough for a look PASS, with holes named below**. Present: game and side camera, light and dark floor, 24 consecutive 60 fps game frames per floor (f156-f179), mp4 for the owner, A2r2 / OFF / A1b rows under the same light, blind critic (`CRITIC-A2r3.md`: NOT YET on both floors, answered below). Missing: low preset; an OFF pass from the same run (the r2 one is used, its pose drifts).
Touchdown = f158; 1 px = raw 1920x1080. Numbers are mine, from the PNGs: on/off difference, OFF body and shadow masked and dilated, A2r2 (`a2r2d`) and A1b measured the same way in the same run. They read lower than the r2 review's absolute values, so every comparison below is r3 against r2 **by the same method**. Extents in game view are polluted by the pose drift (+/- 8 px); the side view is cleaner. Foot line: game y 888, side y 844; knee line in game view = 90 px above it.

Real-speed impression: from the consecutive frames only (I cannot play the mp4). Dark: 4-5 small soft bluish puffs around the feet, ankle high, there for about 20 frames, quiet. Light: grey-brown puffs, clearly seen for about 12 frames, then a pale fade. Not verified: flipbook flicker and noise crawl in motion, the low preset.

| question | yes/no | frame and view | what is seen |
|---|---|---|---|
| 1 message | yes (both) | consec dark f158-f175, light f158-f170 | dark: median 1.46-1.59x the floor for +3..+8, reads as low dust. Light: median 0.88-0.89x for +0..+8, 0.91x at +11, 0.94x at +14: reads to about +12 (r2: +8) |
| 2 character readable (D1) | yes | zoom dark f161, f170; zoom light f161, f166 | feet, shins and hand clear on both floors; dust never covers the body |
| 5 orientation / clipping | yes | zoom dark f161, zoom light f161 | no rectangle, line or ring from the dust. The oval under the feet is the character's blob shadow (also in OFF); it flickers frame to frame (light f158 / f160 / f167), not this effect |
| 7 life curve (D7) | yes | side dark f158-f175 | width 302 px (+1) -> 349 (+6) -> 387 (+14), slowing; value falls 2.9x -> 2.2x (side) over the same frames: grows while it fades, as the owner rule asks |
| 8 clean death (D6, fade to alpha 0) | yes | consec dark f175-f179, light f174-f178 | dark: 45 px left at +20, nothing at +23. Light: nothing at +20. One lone puff on the right outlives the others by about 3 frames on dark |
| 10 value / background (D14) | yes | game dark f161-f166, game light f158-f169 | dark: median L 58-63 on floor L 39 (1.46-1.59x; r2 2.4-2.8x), brightest 5 % 1.9-2.05x (r2 2.9-3.3x). Light: 0.88-0.89x |
| 11 matter and light (D8, D12) | yes, with two leftovers | zoom dark f161, f170; side dark f160-f167 | dark: soft, no cauliflower up to +10. From +12 to +15 dark creases open inside the puffs (zoom dark f170): crunchy for 4 frames. Hue: dust B/R 1.61-1.64 on a floor at 1.45-1.47, a little bluer than the floor |
| 12 quality bar (D11) | yes | all | vs A1b: small, low, short, floor-coloured on dark, visible on light. Does not hold: reads as separate round puffs more than one skirt; slight "cold breath" tint on dark |
| D3 countable | yes | zoom dark f161 | 4-5 puffs, 3-5 pellets |
| D4 frame 1 | yes | consec dark f158, light f158 | present at +0 on both floors (and 2 frames before, see game side) |
| D2, D10 | n/a | | realistic dust by owner decision |
| D5, D9, D13, D15-D21 | n/a | | landing-only capture |

Size target (max = A1b early frame): **met**. Side view top: 106-118 px at +0..+3, 147-150 px at +11..+17, against A1b 138 px at +0 and 149 px at +1 (A1b reaches 219 px by +8). Game view width 229-263 px against A1b 291-313 px at +0..+1. Game view top 44-52 px at +6..+11, 67-74 px at +14..+17 (knee line 90 px).

Previous gaps:
1. colour-value / material, dark floor: **closed.** Targets were median L 55-65, lit tops L <= 90, B/R >= 1.3; r3 gives median L 58-63, brightest 5 % L 74-81, B/R 1.61-1.64 (r2 by the pack file: B/R 1.22-1.27, p90 L 114-123; r3 p90 L 71-77). Side view: no crunchy detail until +12. The hue now overshoots the floor by about 0.15 (gap 1 below).
2. colour / read, light floor: **closed for the read, open for the measured hue.** Median 0.88-0.89x for +0..+8 and 0.91x at +11 (target 0.88-0.90x for +3..+11); area 4100-4600 px against 2400-2900 px in r2; no longer merges with the shadow (zoom light f161, f166). Hue: median B/R 1.031-1.036 against the floor's 1.028 (r2 1.05-1.06): neutral, not yet below the floor; only the darkest quarter goes warm (1.019-1.024 at +6..+8, side). To the eye it is a grey-brown smudge, which is what decision 1A allows.
3. shape / size, height: **closed against the owner's target, not against my 60 px.** Side view top 147-150 px at peak against 158-163 px in r2 (180 px at +6 in r2, 129 px in r3). In game view the top did not drop (67-74 px against 75-76): the wider ring (36 cm) puts the back puffs higher on screen. Still under the knee line and within the A1b early frame.

Blind critic (NOT YET on both floors), point by point:
- light floor "soot, darker and browner than the floor": this is owner decision 1A. Not a gap; the owner judges the amount (decision below).
- dark floor "bluish steam blobs, separate clumps": agreed in part, gap 1.
- "crunchy blotches f170-f173", "white pebble specks read as sparks": agreed, gap 2.
- "dust at f156-f157 before contact": true, inherited (dust spawns on the sim landing frame), game side, not a look round.

Remaining gaps (none blocks the PASS; gravest first):
1. **colour, dark floor** — zoom dark f161, consec dark f160-f166. Dust is bluer than the floor (B/R 1.61-1.64 vs 1.45-1.47) and its soft halo reads a little as cold breath. Fix: take back about 40 % of the r3 blue shift on `SnowColor`, `LitColor`, `ShadeColor` and the cool edge tint (r2 colours gave 1.24, r3 gives 1.62; target 1.45-1.50, equal to the floor). Leave `DarkSnow` 0.26 and `DarkBrightness` 1.0 alone.
2. **material, dark floor, late frames** — zoom dark f170, consec dark f170-f173. Dark creases inside the puffs for +12..+15, and 3-5 near-white pellets by the left foot at +4..+12. Fix: `OccStrength` 0.65 -> 0.5, or scale it by (1 - normalised age) so the creases fade with the puff; cap the pellet colour at about 2x the floor luma on dark floors.
3. **colour, light floor** — zoom light f166 (right clump), side light f164. The right puff is darker than the others (darkest tenth 0.71x the floor at +6, the rest 0.80-0.82x) and the hue is neutral by measure. Fix only if the owner finds it sooty: `Brightness` 0.58 -> 0.64 and `ShadeColorLight` red +0.03 (0.47, 0.36, 0.28); keep `EdgeWidth` 0.6.

Game side: dust visible at f156-f157, 2 frames before the foot is flat. Inherited, not this round.

Decisions for the owner:
- Light floor: is the grey-brown smudge the right strength, or too sooty (gap 3 fix lightens it)?
- Dark floor: is the slightly blue tint fine, or should the dust match the floor hue exactly (gap 1 fix)?
- Shape: 4-5 separate puffs in a ring, or should they merge into one low skirt (smaller ring, more puffs: a new round)?
- Still to do after approval: low preset capture, the early spawn (2 frames before contact), and an OFF pass from the same run for cleaner numbers.
