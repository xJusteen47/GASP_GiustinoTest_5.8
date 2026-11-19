# VFX review: landing dust A2 r5 (build r5b) — FAIL — 2026-11-19

Verdict in one line: the r4 failure is fixed (it is now one radial burst along the floor, inside the size target), but on the dark floor every puff **visibly shrinks** in its last 5-6 frames (owner rule 2), the ring opens into separate hard lumps instead of merging, and the brake is half as strong as the reference.

Capture chain: **enough to judge, holes named**. Present: game and side camera, light and dark floor, 24 consecutive 60 fps frames per floor and view, zooms, the r4 failure, the owner reference sheets, the size tool output. Missing: an OFF pass from this run, the low preset, an mp4 and a blind critic for r5.
Touchdown = f157 (capture log: land on v156; first full dust frame f157), `+n` = frames after f157; 1 px = raw 1920x1080.
Method (mine, from the PNGs in `Saved/VfxV3/a2r5b`): no OFF pass, so the floor is a robust quadratic fit of the luma in a region round the feet, per frame; dust = luma above the fit by more than max(4, 3 sigma) and by more than 1.15x, body masked (luma > 125 or saturated blue, dilated 5 px), hue gate on B/R; extents = 0.5-99.5 percentiles of the mask. The same script was run on `a2r3/dark2`, `a2r4/dark2` and `a2r2b/a1b_dark`, so "r3", "r4" and "A1b" below are **by the same method**; they are not the numbers printed in the r3 review (on/off difference). Light floor: only puffs clear of the body and its shadow (left side of the side view). Foot line: side y 845.

Real-speed impression: from consecutive frames only (no video played). Dark: a low blue-grey band bursts out from the feet in 5-6 frames, then stands still as a ring of 8-10 separate lumps that look like small rocks or crumpled paper; they get smaller one by one and are gone at +26. Light: the same burst as soft grey-brown smoke balls that fade in place; reads as dust. Not verified: flipbook flicker in motion, the low preset.

| question | yes/no | frame and view | what is seen |
|---|---|---|---|
| **Owner 1: one burst from the centre, radial, along the floor** | yes (narrow) | side dark f157-f166, consec dark f157-f166 | born in one frame as one band (1 component of 8430 px out of 8995 at +1). Side width 163 px (+0) -> 283 (+3) -> 407 (+9) -> 466 (+19); reach from the centre 231 px left / 226 px right at +17 (1.02). Top rises 55 px while the base drops 37 px: that is the ring seen in perspective, not a rise. r4: width 110 -> 259 px with the top climbing 139 px (a column). From +9 the band opens into separate lumps: row "ref 8" |
| **Owner 2: no particle visibly shrinks** | **no on dark**, yes on light | side dark f172-f182 (raw crops x3) | dark, lower-left puff (window x 744-802, y 801-848): width >= 58 px (f175) -> 46 (f176) -> 40 (f178) -> 28 (f180) -> 15 (f182), while what is left stays at 2.16x -> 2.01x -> 1.81x -> 1.50x the floor luma (median). Puff above it: area 1762 px (f174) -> 612 (f178) -> 81 (f180) at 2.38x -> 1.65x -> 1.31x; area above half of its own peak (does not change under a pure opacity fade) 1073 px (f172) -> 333 (f178). Game view, whole dust: 5454 px (f172) -> 2695 (f177) -> 617 (f180) with the median only 1.68x -> 1.41x -> 1.34x. Light, same windows: weighted radius grows 29 -> 37 px (f166 -> f177), the puff pales in place |
| **Owner 3: size inside the skirt (A1b early frame)** | yes, at the limit | game dark f174, side dark f176 | game width max 283 px (+17) against A1b f158 307 px = 92 %; side width max 466 px (+19) against A1b 475 = 98 %; r3: 276 / 402. Side top 117-145 px above the foot line at +10..+17 (r3 153-156). Game top 75-80 px above the feet at +9..+19 (knee line 90 px, r3's figure). The literal "as wide as the shoulders" was not measured: no reliable shoulder landmark from behind |
| ref 1 origin | yes | consec dark f157, side dark f158 | at +0 a band at the feet, 163 px wide (side), 103 px (game); radial and flat (row owner 1) |
| ref 2 count / size | yes | side dark f164-f168 | 8-10 lumps (5-10 components), each 58-70 px wide = about half of the shoulder-to-belt height (120 px, side view); 3-5 pellets |
| ref 3 velocity and brake (D7) | **no** (ease-out yes, too soft) | side dark f157-f172 | width step per frame: +34, +51, +35, +28, +26, +22, +17, +17, +14, +12, +11, +9, +8, +4. 49 % of the travel at +4 (0.067 s), 81 % at +9 (0.15 s), 91 % at +12; still +12..+14 px/frame at +9 (peak 51). Game view: 50 % at +4, 82 % at +9. This is drag 10 exactly (1 - e^-10t = 49 %, 78 %). Reference: most of the travel by 0.067 s, nearly still by 0.15 s |
| ref 4 growth | yes while opaque | side dark f161-f169 | left lump 2607 px (+4) -> 3439 (+12): area x1.32, radius x1.15; the lump behind 2078 (+5) -> 2593 (+10). Reference about x1.5 |
| ref 5 fade (D6 as the owner rule: alpha 0, no shrink) | timing yes, shape no on dark | side dark f168-f183 | peak flat at 3.9-4.4x the floor to +11, then linear to 0 at +25/+26: 14 frames = 0.23 s (reference about 0.15 s). On dark the fade reads as shrinking (row owner 2) |
| ref 6 colour / light (D14) | noted, not judged | game dark f157-f168, side light f161-f175 | hue as r3: dust B/R 1.61-1.65 on a floor at 1.48 (r3 1.61-1.68). **Value on dark is up** with the same colour parameters: game median 1.92-2.28x the floor at +0..+11 (r3 1.62-1.81x), brightest 5 % 2.6-3.2x (r3 2.2-2.9x); side median 2.44-2.87x (r3 1.89-2.07x). Light: left puff 0.84x the floor at +4..+10, 0.90x at +14, 0.95x at +18, darkest 1 % 0.53x; B/R 1.027-1.034 on a floor at 1.033 (neutral). Reads on both floors |
| ref 7 edges / matter (D12) | **no on dark**, yes on light | zoom dark f162, f166, f170; zoom light f162, f166 | dark: crisp outline, flat lit facets and black creases from +3 on: rock or crumpled paper, not dust. From the outline to half of the peak: 8 px on a 32 px radius (dark), 13 px on 31 px (light, soft round balls). No hair, no threads |
| ref 8 merging | **no** (reverse of the reference) | consec dark f159-f175, side dark f164-f176 | one band at +0..+2, then it opens: share of the ring width covered in game view 0.92 (+2) -> 0.87 (+6) -> 0.78 (+9) -> 0.73 (+11) -> 0.65 (+18); side components 2 (+0) -> 5-8 (+3..+9) -> 7-10 (+10..+19). Bare floor between the lumps, no veil joins them; they die one after another between f178 and f183 |
| ref 9 duration | yes | side dark f157-f183, side light f157-f179 | dark: f157 to f182 (594 px left at f182, 0 at f184) = 26 frames = 0.43 s; light: gone by f178-f179, about 0.37 s. Reference 0.40-0.45 s |
| D1 character readable | yes | zoom dark f162, f166; zoom light f162, f166 | feet, shins and hands clear on both floors; dust never covers the body |
| D3 countable | yes | side dark f166 | 8-10 lumps, 3-5 pellets |
| D4 frame 1 | yes | consec dark f157, consec light f157 | present at +0 on both floors |
| D2, D8, D10 | n/a | | realistic dust by owner decision |
| D5, D9, D13, D15-D21 | n/a | | landing-only capture |

Size rule tool: `size_r5b.txt` re-run, same result (PASS, silhouette max drop 0.2 % per step). It does not cover what fails here: it checks the alpha silhouette (> 0.06) and reports the visible extent only to 80 % of life with a threshold of 0.03. The shrink in the frames is at 75-95 % of life (f176-f181 of a life f157-f182.5). The tool's own SmokeReal column already shows it: visible radius 0.270 (t 0.80) -> 0.212 (t 0.95) -> 0.157 (t 0.967).

Previous gaps:
1. r4 "column between the legs, no spread": **closed** (row owner 1).
2. r3 owner decision "separate puffs or one skirt": **half closed.** One burst yes; one skirt no (row ref 8).
3. r3 gap 1, hue on dark: open by decision, unchanged; the value went up (row ref 6).
4. r3 gap 2, creases and white pellets on dark: **open, worse.** Creases from +3 instead of +12; `OccStrength` is still 0.65; 3-4 near-white pellets by the left foot (zoom dark f162).
5. r3 gap 3, light floor strength: open by decision.
6. Early spawn: 42 px of dust at f156, one frame before f157 (r3: two frames).

Remaining gaps (gravest first):
1. **look (material)** — dark floor, puffs shrink before they vanish. side dark f176-f182. Probable cause, not proven: on dark the shaded side sits at the floor's value, so only the part that is both lit and dense is seen, and the fade removes it from the thin lit rim inward; on light every part is darker than the floor and the same fade reads as a fade. Fix, MI SmokeReal: `RampLo` 0.25 -> 0.05, `RampHi` 0.6 -> 0.9, `OccStrength` 0.65 -> 0.4 (the visible body follows the alpha, not the lit rim); M_GSTN_VFX_Puff: fade = 1 - ramp^2.5 (new `FadePower`, 2.5) with `FadeStart` 0.45 -> 0.40 (time between 50 % and 10 % opacity from 5.6 to about 3 frames); flipbook soft edge `th + 0.55` -> `th + 0.35`. Accept when, in the side dark frames, each lump keeps >= 85 % of its +15 width for as long as its median is >= 1.3x the floor.
2. **emitter data** — ring opens, no merge. consec dark f166-f175. `land_shockwave`: `speed` 600 -> 950 with `drag` 10 -> 22 (reach 60 -> 43 cm, ring radius 72 -> 55 cm: 14 lumps then cover about 0.9 of the ring instead of 0.65-0.73; game width about 225 px, margin under the 307 px limit); `life` (0.40, 0.50) -> (0.42, 0.46) so they go together. `land_haze`: `speed` 220 -> 400, `drag` 5 -> 8 (sits under the skirt ring, not inside it); MI SmokeHaze `OpacityMax` 0.3 -> 0.45, `FadeStart` 0.05 -> 0.30 (the veil that joins the lumps).
3. **emitter data** — brake too soft. side dark f157-f172. The same `drag` 22: 77 % of the travel at 0.067 s, 96 % at 0.15 s.
4. **look (material / flipbook)** — rock look on dark. zoom dark f162-f170. The ramp and occlusion values of gap 1, plus flipbook normal strength `k` 14 -> 10; pellets: cap the PuffClump colour at about 2x the floor luma on dark (r3 gap 2).
5. **game (tooling)** — `vfx_size_monotonic.py`: report the visible extent to 95 % of life with threshold alpha x fade x OpacityMax > 0.10, and fail on a drop over 15 %. Still missing: OFF pass from the same run, low preset, mp4, blind critic.

Game side: dust at f156 (42 px), one frame before the feet are down. Inherited, not this round.

Decisions for the owner:
- Colour decisions 1 and 2 of the r3 review are still open. New fact for them: on dark the dust is 15-30 % brighter against the floor than in r3 although the colours were not touched.
- Size: the skirt is at 92-98 % of the A1b early frame. Is that frame still the limit, or is the literal shoulder width wanted (gap 2 brings the width to about 225 px)?

## r5c recheck — FAIL (one blocking item) — 2026-11-19

Build r5c = the fixes above applied as written. Frames `Saved/VfxV3/a2r5c`, touchdown f156 (first dust frame; log: land on v155), `+n` from f156. Same scripts and windows as above. Looked at: the six r5c sheets and one sheet of raw crops (x2) of the left puff, dark and light, f162-f178.

**Correction to my r5b diagnosis.** The shrink is not the fade eating a lit rim. It is a geometric scale-down of the whole sprite in about the last quarter of its life, and r5b had it too. The ramp, occlusion and fade-curve changes did what they were for (softer matter, quick fade), but with the opacity now held high to the end they expose the scale-down on both floors.

| point | r5b | r5c | frame and view | result |
|---|---|---|---|---|
| owner 2: no visible shrink | no on dark | **no on both floors** | side dark f170-f178, side light f171-f178 (crops) | dark left puff: area 2858 px (f170) -> 2075 (f174) -> 1425 (f175) -> 889 (f176) -> 468 (f177) -> 181 (f178) while what is left stays at 2.20x -> 2.02x -> 2.01x -> 1.92x -> 1.76x -> 1.61x the floor; at f175 the peak is still 72 % of full and the area is half. Area above half of its own peak 1336 -> 1002 -> 766 -> 517 -> 304 -> 150; its share of the area above a quarter of the peak stays 0.66-0.73, the centre stays within 2 px, the two dark pits inside shrink with it: a scaled copy. From the weighted radius (20 px -> 14.5 at f175 -> 8.2 at f177 -> 5.2 at f178): scale about 1 to 72 % of life, then straight down to 0 at the end. Light: area above half peak 2415 px (f171) -> 2040 (f174) -> 1435 (f176) -> 493 (f178), crops show smaller copies at f176 and f178. r5b frames give the same share (0.68-0.72) |
| brake | no | **yes** | side dark f156-f170, consec dark | side width step per frame +30, +61, +29, +22, +16, +12, +8, +7, +6, +4, +3; 70 % of the travel at +4 (0.067 s), 95 % at +9 (0.15 s); game view 71 % and 94 % |
| owner 1: one radial burst | yes (narrow) | **yes** | side dark f156-f166 | side width 188 px (+0) -> 330 (+4) -> 379 (+9) -> 390 (+14); born in one frame, flat |
| owner 3: size | yes, at the limit | **yes, with margin** | game dark f170, side dark f170 | game width max 249 px = 81 % of the A1b early frame (307), side 390 px = 82 % (475); side top 115-118 px above the foot line; game top 68-69 px above the feet (knee line 90) |
| merging / ring cover | no | **better, not there** | consec dark f158-f172, zoom dark f166, f170 | ring width covered in game view 0.79-0.89 from +1 to +12 (r5b 0.92 -> 0.73), then 0.69 (+15) -> 0.50 (+20) as the puffs scale down; side components 5-6 at +9..+15 (r5b 7-10). Left and right clusters now read as a low band; front and back stay open and the lumps keep their own outline; the haze is seen on light (zoom light f166), barely on dark |
| matter on dark | no | **yes (narrow)** | zoom dark f162-f170, crops f162-f172 | black creases and flat facets are gone; soft rounded lumps. Left: a crisp outline (outline to half peak 8.6 px on a 28.7 px radius) and two dark pits per puff. Light edges are now harder too (8 px on 36.7 px; r5b 13 on 31) |
| duration | 0.43 s | 0.40 s | side dark f156-f180 | f156-f179, gone at f180-f181 |
| value, not judged | | | game dark f159-f170, side light f160-f172 | dark: game median 1.61-1.84x the floor at +3..+14 (r5b 1.74-2.05x, r3 1.55-1.81x): back at r3's level. Light: 0.81-0.82x the floor from +4 to +14 (r5b 0.84x to +10), darkest 1 % 0.69x (r5b 0.53x): no more near-black cores, but the grey-brown smudge is as dark and lasts longer |

Size tool on r5c: PASS (SmokeReal visible extent to 95 % of life, max drop 2.1 %). It models the material only, and the material does not shrink; the frames do. So the size change is outside the material.

Evidence for the cause (probable, not confirmed: the editor was not reachable): `NS_GSTN_LandRing.uasset` holds two instances of `/Niagara/Modules/Update/Size/ScaleSpriteSize`: `ScaleSpriteSize` with a `Scale Factor.FloatCurve` / `Scale Curve` (the burst template's own size-over-life module) and `ScaleSpriteSize001` with `Initial Sprite Size` (the one `sized()` adds at spawn). `_sprites()` turns off the template's `ScaleColor` but not its `ScaleSpriteSize`. This contradicts the note in `vfx_size_monotonic.py` ("no update-stage size module").

Blind critic on r5b (`CRITIC-A2r5.md`, NOT YET), weighed against r5c:
- "stop dead then erode": half agreed. The erode is real and is the blocking item. The stop is what the owner's reference study asks (nearly still by 0.15 s); once the puffs keep their size to the end, the slow growth (area x1.65 from +4 to +14) carries the late frames.
- "hollow donut, separate puffs": agreed in part, remaining item 2. The centre is the character; the open front and back are the gap.
- "light floor reads as grey soot": still true by value; this is owner decision 1, not judged.
- "life 0.5-0.6 s, grow 1.5-2x": not taken; the owner's study gives 0.40-0.45 s.

Remaining items:
1. **emitter data (blocking)** — sprite scaled down over the last quarter of life. In `_puffs()` (dust only, so the other sprite effects keep their reviewed behaviour): `e.off("ParticleUpdate", "ScaleSpriteSize")`, then check in the editor that the spawn-stage module (`ScaleSpriteSize001`) is the one that receives `Scale Sprite Size Mode`, `Initial Sprite Size` and `User.SizeScale`. Accept when the left puff keeps >= 85 % of its +14 area (d > 4) for as long as its median is >= 1.3x the floor, on both floors.
2. **emitter data** — ring cover 0.82. `land_shockwave` `count` 14 -> 18 (cover about 0.95 at the same radius). Check the cover again after item 1: the fall after +13 is the scale-down.
3. **game (tooling)** — setup log or `vfx_size_monotonic.py`: fail if a dust emitter has an enabled ParticleUpdate module that writes the sprite size; the material model alone passed two builds that shrink.

Decisions for the owner: unchanged (light-floor strength, dark hue). Not captured: OFF pass, low preset, mp4.

## r5d recheck — PASS (narrow, to the owner) — 2026-11-19

Build r5d = r5c with the template size module turned off on Skirt, Wisps and Clumps (build log: three "template size module off: ScaleSpriteSize" lines) and `land_shockwave` count 14 -> 18. Frames `Saved/VfxV3/a2r5d`, touchdown f156, `+n` from f156. Same scripts and windows. Looked at: five r5d sheets (side dark, consec dark and light, both zooms) and one sheet of raw crops (x2) of the left puffs, dark and light, f162-f182. Not looked at: side light sheet. No video, no blind critic on r5d.

| point | r5c | r5d | frame and view | result |
|---|---|---|---|---|
| owner 2: no visible shrink | no, both floors | **yes, both floors** | side dark f170-f182, side light f170-f180 (crops) | dark left puffs: area (d > 4) 2874 px (f170) -> 2830 (f175) -> 2793 (f178) -> 2726 (f179) -> 2568 (f180) while the median falls 2.61x -> 2.31x -> 1.92x -> 1.74x -> 1.50x the floor: 89 % of the +14 area is still there at 1.50x. Area above half of its own peak 1895 -> 1827 -> 1787 -> 1711 -> 1476; weighted radius 31.9 -> 32.0 -> 31.7 -> 31.2 -> 29.8 px. One frame misses my own bar: f181, 1760 px (61 %) at 1.35x, the last frame but one; in the crops f180 and f182 are the same size as f172, only dimmer. Light: area 4685 px (f170) -> 4536 (f175) -> 4060 (f177, 0.94x) -> 3486 (f179, 0.97x), weighted radius 27.1 -> 26.0 px; pales in place |
| merging / ring cover | 0.79-0.89, then down to 0.50 | **yes (narrow)** | consec dark f159-f177 | ring width covered in game view 0.89-0.97 at +3..+10, 0.81-0.86 at +11..+21 (r5b 0.73 -> 0.65). 3-5 components in game view (r5c 4-7). Reads as one low wreath round the feet on dark; in the side view and on the light floor the lumps still keep their own outline |
| brake | yes | yes | side dark f156-f170 | side width 183 px (+0) -> 316 (+4) -> 362 (+9) -> 374 (+14): 70 % at 0.067 s, 94 % at 0.15 s; game view 69 % and 91 %; 3-6 px per frame at +9 (peak 58) |
| owner 1: one radial burst | yes | yes | side dark f156-f166 | one component at +0 (9037 px), flat, both sides |
| owner 3: size | yes | yes | game dark f174-f178, side dark f173 | game width max 247 px = 80 % of the A1b early frame (307), side 381 px = 80 % (475). The puffs are larger than in r5c (the template curve also held them small) and no longer scale down, so the top is higher: game top 72-79 px above the feet at +10..+22 (knee line 90, r5c 68-69); side top 129-142 px above the foot line (r5c 115-118, r3 153-156) |
| matter on dark | yes (narrow) | yes (narrow) | zoom dark f162-f170 | soft rounded lumps, no creases; crisp outline and dark pits inside each puff remain |
| fade / duration | 0.40 s | 0.45 s | side dark f156-f182 | peak flat to +14, then 56 -> 47 (f175) -> 33.5 (f178) -> 20.6 (f180) -> 5.3 (f182); dark f156-f182 = 27 frames, light gone at f180-f181 (0.42 s). Reference 0.40-0.45 s |
| value, not judged | | | game dark f159-f172, side light f160-f172 | dark: game median 1.90-2.04x the floor at +3..+16 (r5c 1.61-1.84x, r3 1.55-1.81x): brighter again with larger, denser puffs. Light: 0.85-0.86x to +14, darkest 1 % 0.68x; still a grey-brown smudge |

Why PASS: the three owner points and the four items failed in r5b now hold by the same measures. Why narrow: one frame under my shrink bar at 1.35x, cover under 0.9 after +10, and nobody has seen it move.

Remaining items (none blocks):
1. **emitter data, other dust events** — the owner rule covers every dust event. `_puffs()` is fixed; `_smoke()` and the dust emitters made straight from `_sprites()` (jump puffs, run-start kick, skid row) use the same templates and were not touched; not checked here whether their template carries the module. Apply the same "template size module off" there when those effects get their round, and check them with the per-puff measure.
2. **look, owner decision** — dark-floor value 1.90-2.04x in game view. If the owner wants r3's level back: `DarkSnow` 0.26 -> 0.23 on SmokeReal (estimate, measure after). Light-floor strength and dark hue: still open, as in r3.
3. **game (tooling)** — make the build's "template size module off" line a failing check when a dust emitter keeps an enabled ParticleUpdate size module. Still not captured: OFF pass, low preset, mp4.
4. **look (flipbook), minor** — dark pits inside the puffs (zoom dark f166, right puff).
