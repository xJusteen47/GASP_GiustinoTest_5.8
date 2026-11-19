# VFX review: landing dust A2 r6 (build b2) — FAIL — 2026-11-19

Reviewer: senior VFX (Fable). Not shown to the owner yet. Three items block: grit reads as floating beads, the cloud contracts while
still bright at the end on the dark floor, and the "lit" dust does not show the scene light side. The layer structure is right;
the read is not there yet. All fixes are vfx (presets/material), one round.

Looked at: dossier, DK study (text only, none of its images), r5d recheck; sheets r6b2 consec dark, zoom dark/light, side dark/light;
r5d consec dark, zoom light, side dark; 6 own crop sheets from `Saved/VfxV3/a2r6/b2/{dark,light}/jump/game` (f155-f159 flash,
f157-f174 debris, f172-f186 fade, r5d vs r6 at f166). Not looked at: side frames at full size, video, OFF pass, low preset.
Measures: game view, full frame 1920x1080, box x640-1160 y740-1000; "floor" = median floor value in the same frame; dark dust mask
= blue-ish pixels over 1.25x floor. Touchdown v156 dark / v155 light; `+n` from the first dust frame (f157 dark, f156 light).

## (a) Owner rules
| rule | result | frame | numbers |
|---|---|---|---|
| one radial burst | yes | dark f157-f164 | one event, width 115 px (+0) -> 190 (+3) -> 227 (+7) -> 244 (+15); no second emission |
| size cap (shoulders, below knee) | yes | dark f172-f176 | width max 245 px (r5d 243, A1b frame 307 = 80 %); top about 70-75 px over the foot line, knee line 85-90 |
| dust never shrinks | **no (dark), yes (light)** | dark f176-f186 | width 244 (f176) -> 228 (f180) -> 224 (f182) -> 165 (f184) -> 72 (f186); area 11920 -> 9319 -> 6992 -> 4521 -> 1646 px while the median is still 1.95x -> 1.80x -> 1.73x -> 1.70x floor and p95 3.45x -> 2.47x. At f184 only 35 % of the peak area is left at 1.73x (r5d: 89 % at 1.50x, one frame at 61 %). The puffs do not scale down; the thin edges and the outer skirt go first and bright cores stay, so the cloud closes in on two lumps for 4 frames. Light f172-f182: pales in place, fine |
| grit may shrink | n/a | light f169-f171 | grit does not shrink, it fades out in the air (see c) |
Duration: dark f157-f186 = 0.50 s (r5d 0.45), light f156-f181 = 0.42 s. Reference 0.40-0.45 s.

## (b) DK causes vs r5d
1. **Lit by the scene: half.** Material is lit and the puffs have more form on dark (p95/p5 inside one puff 3.0, r5d 2.2). But the
   bright side is the same as the painted r5d one: left half / right half 1.04-1.12 in r6 (dark f164, f166, f170, left and right
   puff), 1.12-1.17 in r5d. The character's shadow falls to the left in both captures, so the sun is on the right and the right
   half should be the bright one. Either the painted shading in the colour flipbook still drives the look or the normal X is
   flipped. On the light floor no lit side is visible at all (flat grey, light f160-f170).
2. **Layers: half.** Flash and grit exist, each with its own timing, but the flash is weak in game view and the grit reads wrong (c).
   Still missing by plan: streaks, shock ring, motes.
3. **Uniformity: better.** Mixed sizes and late lumps are visible (light f159-f163: small and large puffs, some inside the ring).
   Still a symmetric ring of same-looking cauliflowers on dark (f162-f170); no stronger side.

## (c) New layers
- **Flash: light, not a sprite (good), but too weak and one frame late.** Dark floor patch 40-60 cm from centre: 1.48x (f157) ->
  1.73x (f158) -> 1.27x (f159) -> 0.99x (f160); warm (+27 R, +21 G, +15 B at f158). Peak is at +1, not +0, and 1.7x on a floor of
  value 30 is +22 levels: easy to miss at game distance (consec dark f157-f158). Legs: tan patch on the back of the thighs only
  (dark f157-f158, light f156-f157), shins and feet unlit; with a hard edge it reads as a material patch, not as light.
  Light floor: 0.99-1.02x (f156-f158) = invisible; only the thigh patch shows.
- **Grit: fails.** 9 same-size round dark beads, born in one clump about 40 px (22 cm) wide between the feet (light f157). They rise
  to about 35 px (f159) -> 65 (f161) -> 75-85 px (f163-f167, knee height), hang there, and fade out in the air at f169-f171.
  None comes back to the floor, none goes outward. On dark they are black dots over the left calf for 10 frames (f160-f170): reads
  as dirt on the leg. Life (about 0.25 s) is shorter than the flight (about 0.6 s at real gravity for a 45 cm apex).
- **Dust shading on both floors: no** (see b1). New on light: a hard straight dark edge at the foot line under the dust
  (zoom light f156, f162, f166; crops f172, f180); r5d had a soft ellipse there (f166). Looks like the lit sprites cutting the floor
  without depth fade, or the contact disc drawn darker.

## (d) Checklist (items that apply; D2, D5, D6, D12 are overridden by the owner's realistic-dust rule)
D1 no, minor (grit dots on the calf, dark f160-f170). D3 yes (18 puffs + 9 grit). D4 yes. D7 yes (78 % of width at +3, 93 % at +7).
D8/D14 colour: not failed on hue; value below. D13 no on dark: dust p95 4.2-4.4x floor (r5d 3.1x), brightest thing after the body.
D14 reads on both floors; "takes the scene light": no. D16 yes (contact shadow). D21 no: grit never lands.

## (e) Worse than r5d
- Dark-floor value: median 2.35-2.75x floor at +3..+17 (r5d 2.21-2.32x by the same mask), p95 4.2-4.4x (3.1x), B-R 45 (35).
  Brighter and more saturated blue; with the crisp lobes it reads as blue popcorn. Not failed alone (colour decisions open).
- Light-floor value: side boxes median 0.79-0.85x floor at +4..+16 (r5d 0.84-0.86x), hue now neutral grey-blue (172,174,181),
  r5d brown patches gone. About the same; not worse. The hard edge under the feet is worse.
- End of life on dark (rule a). Light: dust and flash start at f156 with both feet still about 45 px over the floor (pose lags the
  sim by one frame there; dark f157 has the feet down). **game/pose**, minor, same cue timing as r5: only note it.

## Fixes (all block except 6)
1. **vfx, grit.** Launch pitch 15-40 deg over the floor (now near vertical), speed 250-450 cm/s outward on all 360 deg, spawn on a
   ring of radius 10-25 cm; gravity -2000 cm/s2 (2x), drag 1.5; life 0.35-0.45 s and kill or stop at the floor plane (shrink to 0
   in the last 3 frames on the floor is allowed); count 9 -> 12; sizes 1.5-5 cm, 3 pieces at 4-5 cm; not spheres: flat shard mesh
   or sprite with random rotation and spin 360-900 deg/s; albedo = floor colour x 0.6 (dark floor: not black, about 1.3x floor).
   Target: at +6 the pieces span at least 150 px and none is above 45 px; all on the floor by +16.
2. **vfx, end of life.** All dust layers end inside the same 2 frames (Lumps life minus 1.5 frames, lifetime random off or +-5 %);
   opacity = texture alpha x fade (no threshold/erosion); sprite size +15 % over the last 30 % of life; alpha fade from 55 % of life.
   Target on dark: area still over 80 % of peak when the median is 1.5x floor; total 0.42-0.45 s.
3. **vfx, lit material.** Flat albedo (no painted light in the colour flipbook: use it as density/alpha only), check the normal
   flipbook X/Y sign. Proof to capture: sun side / shadow side at least 1.15 on both floors, and the bright side flips when the
   test light is turned 180 deg. Restore depth fade 10 cm against the floor.
4. **vfx, flash.** Peak on the spawn frame (no ramp in), intensity x2: floor at 50 cm at least 2.5x on dark at +0, then 60 %, 30 %,
   10 % (4 frames in all); radius 220 -> 300 cm; height 20-30 cm over the floor so shins and shoes get it too. Light floor: aim for
   at least +8 % and a warm tint (R-B +12) at 50 cm, or leave it and accept legs only.
5. **vfx, dark value.** Bring median back to 2.0-2.2x and p95 to 3.2x floor or less (albedo x0.8 or directional share down),
   B-R 35 or less, until the owner settles decisions 1-2.
6. **vfx, variation (no block).** One stronger side: 60 % of the puffs in the 180 deg toward the travel direction, speed +-40 %.

Why FAIL: the two new layers are the news of this round and neither reads as intended in game view (flash +22 levels for 2
frames, grit as hovering beads), the lit dust cannot show which side the sun is on, and the owner's never-shrink rule is weaker
than in r5d on the dark floor. Showing this would likely get "still fake" plus a question about the black balls.

## r6c recheck — PASS (narrow, to the owner) — 2026-11-19

Build r6c, frames `Saved/VfxV3/a2r6/c`, touchdown v156 both floors, first dust frame f157 (`+n` from f157). Same scripts, boxes and
masks as above. Looked at: `zoomA_r6c_{dark,light}`, `zoom_r6c_{dark,light}`, one own crop sheet (dark and light f160-f178).
Not looked at: consec sheets, side view, video. No light-flip test (not in this round).

| item | b2 | r6c | frame | result |
|---|---|---|---|---|
| 1 grit | beads rise to the knee, fade in the air | **PASS** | zoomA f159-f166, crops f164, f168, f174 | small dark pieces leave on all sides and low: about 165 px span at f164 (+7), about 265 px at f168 (+11), highest piece 50-60 px over the floor at f164 (target 45: close), on the floor at f168-f171, none left at f174. Still round dots, read as pebbles at game distance |
| 2 never shrinks (dark) | 35 % of area at 1.73x | **PASS (narrow)** | dark f172-f182 | width 242 (f172) -> 243 -> 241 -> 244 (f178) -> 242 (f180): the outline holds to the last frame. Area 6320 (f176) -> 4786 (f178, 76 %, median 1.51x) -> 3052 (f180, 48 %, 1.46x) -> gone at f182. 76 % against my 80 % bar, but nothing closes in. Dark f157-f181 = 0.42 s; light pales in place f174-f178, gone at f180-f181 |
| 3 lit side | left side bright (1.04-1.12), as painted | **not proven, no block** | dark f164, f166, f170 | left/right 0.90-0.97 (left puff), 1.00 (right puff); top/bottom 1.09-1.18. The painted left light is gone and the bias is now even or slightly to the sun side, but under the 1.15 target and with no flip test. Light floor: no lit side visible; dust now reads as thin grey smoke with darker cores |
| 4 flash | 1.73x at +1, light floor 1.02x | **PASS** | f157-f159 both | dark floor 2.91x (f157) -> 3.28x (f158) -> 0.96x (f159), warm (118,103,94 at f158); light floor 1.10x -> 1.09x -> 1.01x. Full on the spawn frame, clear in game view. It cuts off in one frame (no tail), and the legs still take it as a cream patch on the back of the thighs only (zoomA f157-f158) |
| 5 dark value | median 2.35-2.75x, p95 4.3x, B-R 45 | **PASS (narrow)** | dark f159-f176 | median 1.73-1.87x (target 2.0-2.2: a little under), p95 3.1-3.4x, B-R 41-44 (target 35: not reached, hue stays with the owner). Masked area at peak 5500-6300 px against 9900 in r5d: the cloud is thinner on dark, tufts keep their own outline |
Light-floor value: median 0.81-0.87x at +7..+17 (b2 0.79-0.85x), neutral grey-blue (168,171,180). Hard dark line at the foot line
still shows in two frames (zoomA light f159, f164), weaker than in b2.

Overall PASS: both new layers now read as intended (floor lit 3x for two frames, pieces thrown out and down), the dust holds its
outline while it fades, and the dark floor no longer glows. Narrow because the lit side is not proven, 76 % area against 80 %, and
nobody has seen it move. Tell the owner what is not in: streaks, shock ring, motes, richer flipbook.

Remaining (none blocks):
1. **vfx, lit side.** Flip test with the sun turned 180 deg and normal X sign check; target sun/shadow side 1.15 on both floors.
2. **vfx, flash.** Add a tail: 100 %, 100 %, 40 %, 15 % (4 frames). Thigh patch: source at 25 cm lights only what faces down;
   try 60 cm in front of the feet toward the camera, or radius falloff softer, so shins and shoes take it.
3. **vfx, dark density.** DarkBrightness 1.15 -> 1.25 or opacity +15 % to get the median to 2.0x; keep p95 at 3.3x or less.
4. **vfx, grit shape.** Shards with spin in place of round dots; 3 pieces at 4-5 cm are not visible as larger (crops f164, f168).
5. **vfx, light floor.** Depth fade against the floor for the remaining hard line (f159, f164).
