# VFX v3 hd1: review against DK Bananza (owner: "I cannot see the effects on my phone")

Date: 2026-11-19. Reviewer: Fable (VFX review pass). Images looked at once each:
`Saved/VfxV3/dkref/dk_sheet_{a,b,c}.jpg` (reference, phone filming a TV, 30 fps) and
`Saved/VfxV3/hd1/sheet_{jump,run_skid,hit_medium,hit_debris,ground_bounce}.jpg` (ours, game camera, light floor, 60 fps).
Parameters read from `Tools/GAS/setup_presentation_vfx.py` and `Tools/GAS/setup_presentation_vfx_v3.py`.

Limits of this review: the reference is blurry and small, so it is judged on shape, volume, size against the character,
duration and contrast, not on colour or texture detail. Our sheets are contact sheets of 400 px tiles, every 2nd or 4th
frame: effects shorter than 3 frames can fall between tiles. Nothing below was tested in engine: every number is a
proposal to try, not a measured fix.

## Short answer to the owner

No. None of the five effects is at DK Bananza's level yet, and the owner is right that they do not read from the game
camera. The ground bounce cloud is the closest (right volume, visible lobes). The main reason is not particle count: it
is **contrast**. DK Bananza always has a value or hue step between the effect and the floor (pale or pinkish dust on a
mid blue floor, saturated orange/red and white on hits, big solid chunks). Ours is a floor-coloured puff on a
near-white floor (0.88, 0.91, 0.97), so it is white on white, and the only places where it shows are where it overlaps
the character's dark legs, the cast shadow or the grey back wall.

## What DK Bananza does (from the three sheets)

- Landing (a: f51-57, f81-87): a low ring of puffs around the feet, about 2x the character's width, flat on the floor,
  clearly a different tone from the floor, readable for 3 sampled frames (0.2-0.3 s) with the character at only ~1/6 of
  the frame height.
- Hits (b: f110, f131-134, f140-143, f164-167; c: f208, f217-220, f232, f256): a white radial burst 1.5-2.5x the
  character, a saturated orange/red layer under it (cracks or sparks on the floor), and solid chunks of the floor from
  0.3 to 1 character in size that fly and stay for 10+ frames (b: f146-161). The terrain is also destroyed (crater), which
  is a game feature, not a particle effect.
- Every effect has three layers: flash (white), colour (saturated warm), solids (chunks). Each layer alone would still
  be visible on the phone footage.

## 1. Verdicts

| Effect | Verdict | Why |
|---|---|---|
| Landing dust (`NS_GSTN_LandRing`) | NOT | A faint white smudge at knee height, about 60-80 cm wide on screen, visible only against the legs (v156-176). DK: ring 2x the character, readable against the floor. |
| Run start / skid dust | NOT | Run start (v100-132): nothing visible. Skid (v136-152): one puff of ~60 cm behind the foot, seen only where it crosses the shadow. |
| Medium hit | NOT | 3-5 thin orange streaks of ~30 cm at the hip (v26-34, v46-52), no star in any tile, hidden behind the attacker's body. DK: burst larger than the character. |
| Strong hit | NOT (streaks close) | The streak fan (v38-44) is the one element that reads well. No star in the tiles, debris and sparks are specks (v46-52), no dust. DK: big chunks that stay. |
| Ground bounce (`NS_GSTN_Impact`) | NOT (closest) | Right volume (~150-200 cm) and visible lobes (v118-130), but it reads only against the grey wall, it sits at torso height and has no solids. |

## 2. Gaps and what to try

Labels: **posa/look** = shape, size, colour, shading; **dati** = values in the setup scripts; **gioco** = what the game
does (cue position, time scale, camera).

### 2.0 Shared by every dust effect (do this first, it is the largest gain)

1. **posa/look, dati: no value step on a light floor.** `SmokeDust` and `SmokeLight` use `ShadeMinLight 0.8`,
   `RimShadeLight 0.72 / 0.8`, `EmissiveLiftLight 0.5`: the whole puff lands within a few percent of the floor's value.
   r5 found neutral `ShadeMin 0.4` reads as soot, so do not go back to a neutral grey: make the shade **cool and
   tinted**, like snow in shadow.
   - Add a `ShadeTint` colour to the smoke material, multiplied only on the shaded side (the `lerp(ShadeMin, 1, G)`
     term): try (0.78, 0.86, 1.0).
   - `SmokeDust`: `ShadeMinLight` 0.8 -> 0.62, `RimShadeLight` 0.72 -> 0.60, `RimWidth` 0.35 -> 0.45,
     `ErodeStart` 0.2 -> 0.3 (stays dense longer).
   - `SmokeLight`: `ShadeMinLight` 0.8 -> 0.66, `RimShadeLight` 0.8 -> 0.64, `OpacityMax` 0.9 -> 1.0,
     `ErodeStart` 0.1 -> 0.2.
   - `SmokeCloud`: `ShadeMinLight` 0.78 -> 0.60, `RimShadeLight` 0.76 -> 0.62, `OpacityMax` 0.8 -> 0.9.
   - Check: the lit top must stay at the floor's value or slightly above; only the underside and rim go pale blue-grey.
     If it reads grey instead of blue, raise the tint's saturation before lowering `ShadeMin` further.
2. **posa/look: no contact with the floor.** Real snow dust darkens the floor under it. Add one ground-aligned soft
   disc per dust system (sprite facing `User.GroundNormal`, or a small decal): colour = floor x (0.70, 0.78, 0.92),
   alpha 0.18 -> 0 over 0.35 s, radius growing 0.6x -> 1.2x the puff ring. It is a shadow, not soot: keep alpha <= 0.2.
   This is the cheapest way to draw the effect's outline on white.
3. **dati: grains are below what a phone can show.** 5-8 cm grains (9-12 cm on impact) are a few pixels at 1080p and
   disappear in a phone-sized video. DK's solids are 0.3-1 character. Snow chunks: 12-22 cm for feet effects, 16-28 cm
   for impacts, double the counts (below), with the same cool shade so they are not white dots on white.
4. **dati: the landing puffs look smaller than the numbers say.** 6 puffs of 110-150 cm pushed out at 260 cm/s should
   cover ~2.5 m; the sheet shows well under 1 m. Either `User.SizeScale` from the fall speed is low for a normal jump,
   or only the dense cores survive against white. Capture the same jump on the dark floor once to tell which. If it is
   the scale: clamp the landing scale to a minimum of 0.8.

### 2.1 Landing dust: NOT

- **dati** count and spread: puffs 6 -> 10; size 110-150 -> 150-210 cm; ring radius 30 -> 45 cm; speed 260 -> 380;
  drag 6 -> 5; life 0.50-0.62 -> 0.55-0.70 s. Target on screen: a ring about 2x the character's width, below the knee.
- **posa/look** it rises around the legs instead of spreading on the floor: `rise` 40 -> 15, origin (0, 0, -12) ->
  (0, 0, -4) so the push is more horizontal.
- **dati** grains 6 -> 12, size 5-8 -> 12-20 cm, speed 520 -> 650.
- **posa/look** contact disc as in 2.0.2, radius 90 -> 170 cm.
- Jump puff (`NS_GSTN_JumpPuff`, 3 puffs of 30-40 cm): not visible at all in the sheet. 3 -> 5 puffs, 30-40 -> 70-100 cm.

### 2.2 Run start and skid dust: NOT

- **dati** run start: puffs 5 -> 7, size 110-160 -> 130-180 cm, speed 150 -> 220, grains 4 -> 8 at 12-18 cm. Still
  nothing visible at v100-132, so also check in the log that the cue fired in this capture.
- **dati** skid: `Kick` 3 -> 6 puffs, 100-140 -> 140-190 cm, cone speed 120 -> 200 with the axis tilted against the
  slide instead of straight up; `Trail` rate 18 -> 30 /s, size 95-135 -> 120-160 cm; grains 4 -> 8 at 12-18 cm.
- **posa/look** the skid needs a shape that says "sliding": a low wedge of dust ahead of the feet, wider than the
  stance (1.5-2 m), not a ball behind one foot. Spawn box 10 -> 35 cm wide across the slide direction.
- **posa/look** optional: two short cool-grey skid marks on the floor (alpha 0.15, 0.4 s). Same role as the contact disc.

### 2.3 Medium hit: NOT

- **gioco** the star lives 0.042 s (2.5 frames) and is in no tile. For the eye on a phone that is a flicker. Life
  0.042 -> 0.083 s, and keep it out of the hitstop slowdown (r5's problem was the slowdown stretching it to 12 frames,
  so give the star its own unscaled age instead of a shorter life). Size 210 -> 240 cm.
- **posa/look** a white core on a white floor is invisible: the read must come from the saturated fringe. Widen and
  saturate the orange/red fringe of `FlashStar` and `FlashStreak` (DK's hits read through orange/red, not white).
- **dati** streaks for M: count 6 -> 10, length 85 -> 130 cm, width 12 -> 16 cm, life 0.13-0.17 -> 0.18-0.22 s.
  Sparks 6 -> 10, size 7-11 -> 12-18 cm.
- **gioco** the fan flies along the push (X), which from the game camera is away from the viewer and behind the
  attacker's torso. `toward_camera` 60 -> 90 cm for star and streaks, and tilt the cone 25-30 degrees up so half the
  fan clears the two bodies.

### 2.4 Strong hit: NOT (streaks are close)

- Keep the streak fan (v38-44): right size, right colour, reads on white.
- **gioco / dati** star: same fix as 2.3.
- **dati** debris: 3 gems of 10-13 cm are specks. 6-8 pieces of 22-35 cm, life 0.75-0.90 -> 1.0-1.2 s, speed
  900 -> 1100. This is the layer DK relies on most.
- **posa/look** no floor reaction: add 4 `SmokeDust` puffs of 140-180 cm low at the victim's feet on a strong hit
  (the landing ring at 0.7 scale), with the contact disc.
- Not a gap to close with particles: DK's craters and cracks are terrain destruction.

### 2.5 Ground bounce: NOT (closest)

- **gioco** the cloud is centred at about 1-1.8 m, around the attacker's torso, not on the floor (v106-130), and the
  victim is above the attacker's head at v105. Check where the cue takes its position: it looks like the victim's
  location or a contact with the attacker's capsule, not the floor hit point.
- **posa/look** it reads against the wall and vanishes on the floor: material changes of 2.0.1 plus a contact disc of
  radius 120 -> 220 cm.
- **dati** grains 10 -> 16, size 9-12 -> 16-28 cm, speed 650 -> 800; `Core` 3 -> 4 puffs. Keep the delayed
  `Lobes` / `Tops`: the lobes are what works.
- **dati** it lingers almost unchanged from v118 to v130 (the hitstop or a long hold): a snow cloud should thin fast
  after the peak. `ErodeStart` stays at 0.25-0.3 but check the system is not slowed by a hitstop here.

## 3. Capture and game problems (not the effect)

1. **Test floor**: flat, untextured, near white, next to a flat grey wall. It is the worst case for floor-coloured
   dust and no real level will look like it. Keep it as the hard test, but also show the owner a mid-value floor.
2. **Camera line**: in both hit sheets the victim is exactly behind the attacker, so the hit point is covered by the
   attacker's back. Offset the dummy 30-40 degrees to the side for captures; in game, see 2.3 (`toward_camera`, tilt).
3. **Sampling**: every 2nd frame at 60 fps cannot show a 2.5-frame star. Capture every frame for the 8 frames after a hit.
4. **Debug clutter**: "OPZIONI DEBUG" text, kit labels, the yellow dashed vertical line under the jumper (v146-150),
   the bright ring at the attacker's feet in the bounce sheet. The column of white dots above the victim is the tumble
   trail on a vertical launch: it reads as a debug line, not as an effect (worth its own look later).
5. **Dark ellipse under the feet** on landing (v158-164): blob shadow or decal, with a hard edge. It is the most
   readable thing in the landing sheet, which shows that a soft contact darkening would work.
6. **Phone delivery**: the character is small in a wide frame. For the owner's video crop to about 2.5x the character's
   height, add a half-speed replay of each effect, and avoid heavy compression (thin streaks and pale dust are the
   first things a low bitrate removes).

## Order of work

1. Material shading on light floors (cool `ShadeTint`, lower `ShadeMinLight` / `RimShadeLight`) + contact disc.
2. Chunk sizes and counts (grains, debris).
3. Star life outside the hitstop, fringe saturation, fan toward the camera.
4. Bounce cue position.
5. Puff counts and sizes per effect, then a new capture with the side-offset dummy, every frame around the hits.

## Attempt 1

Date: 2026-11-19. Looked once at `Saved/VfxV3/hd2/sheet_{jump,run_skid,hit_medium,hit_debris,ground_bounce}.jpg` (light floor)
and `dsheet_{jump,hit_debris,ground_bounce}.jpg` (dark floor 0.22, 0.215, 0.21). Same limits as above: 400 px tiles, every 2nd
or 4th frame, DK reference blurry. Not done in this attempt (by the coordinator's note): contact disc, star unscaled age,
bounce cue position, dummy offset.

Overall: the light floor went from invisible to readable. The cool shade works: the dust reads as pale blue snow, not soot,
and no contact disc was needed for it. The dark floor is now the weak side: the dust is tan / khaki and the bounce cloud is
faint.

| Effect | vs DK Bananza | Phone readability | Notes |
|---|---|---|---|
| Landing dust | COMPARABLE on light, NOT on dark | PASS light, PASS dark (size) | Light v156-172: low ring ~2.5x the character's width, pale blue, light like snow. Dark v156-172: same size but warm tan lumps on a neutral grey floor (khaki), fairly opaque. |
| Run start | NOT | FAIL | Still nothing visible at v100-128. |
| Skid | COMPARABLE | PASS | v136-152: ~1.5 m pale cloud at the feet, clear against the floor. Still a ball more than a wedge; acceptable. |
| Medium hit | NOT (close) | PASS | v26-32: fan of ~1 m orange-white streaks, above and beside the bodies, reads well. Still no star or flash in any tile: it is spikes only, DK has a white burst under them. |
| Strong hit | COMPARABLE | PASS | v36-46: fan ~2x the character, light and dark. Red debris visible v44-52. Dust at v50-52 is faint but present. |
| Ground bounce | NOT | PASS light, FAIL dark | Light v108-130: big white lobed cloud, reads. Dark v106-130: thin grey-brown haze, hard to see. Both: still centred at torso height, not on the floor. |

Checks:
- Soot / grey on light: none. Landing and skid are pale blue-white. The landing haze is rather even at v166-172 (lobes merge
  into one flat sheet), acceptable.
- Cream / khaki on dark: **present** on the landing (dsheet_jump v158-172) and the hit dust (dsheet_hit_debris v50-52).
- Opaque sticker on dark: the landing lumps are close to it (flat, even tone, hard-ish outline). The bounce cloud is the
  opposite, too thin.
- Light like snow: yes on light, no on dark (reads as dry earth).

Other things seen (not counted in the 5):
- The "grains" are 4-point star shapes that drift at hip to head height (light v164-172, dark v164-172 where they are dark
  stars on a dark floor). They read as sparkles, not as snow chunks thrown along the floor.
- Debris shadows on the light floor are saturated blue blobs (sheet_hit_debris v38-52): they read as paint spots.
- White ring at the attacker's feet in the dark hit and bounce sheets, debug text, tumble dotted column: as before.

Remaining changes, highest gain first (max 5):

1. **posa/look, dark floors: khaki dust.** The puff takes the dark floor's colour and the warm light makes it tan. On dark
   floors only, set the dust colour to lerp(SurfaceColor, (0.85, 0.88, 0.95), 0.4) and raise the dark-side `EmissiveLift`:
   `SmokeDust` 0.5 -> 0.8, `SmokeCloud` 0.4 -> 0.7. Apply `ShadeTint` on dark floors too at half strength (0.9, 0.93, 1.0).
   Target: a pale cool grey clearly lighter than the floor. Dark `ShadeMin` for `SmokeDust` 0.8 -> 0.65 so the lumps get a
   lit top and a shaded side instead of one flat tone.
2. **gioco: bounce cloud position.** Spawn `NS_GSTN_Impact` at the floor contact point (`User.GroundPoint`), not at the
   victim / attacker height. On dark also check `SmokeCloud` opacity: with change 1 it should read; if it is still thin,
   dark `OpacityMax` 0.9 -> 1.0.
3. **gioco, posa/look: hit flash.** Give the star its own unscaled age with life 0.083 s (not done yet) and add one soft
   round glow under the streaks: 180 cm, 4 frames, white core with a wide orange fringe, `toward_camera` 90. This is what
   separates the medium hit from DK.
4. **dati: run start.** Check in the log that the cue fires in this capture (v100-128). If it does: puffs 7 -> 9, speed
   220 -> 320, cone axis (-1, 0, 0.35) -> (-1, 0, 0.15) with angle 55 -> 70 so the dust spreads sideways out of the
   runner's silhouette (from behind, the body hides anything kicked straight back), life 0.45-0.58 -> 0.55-0.70 s.
5. **posa/look, dati: grains.** Replace the star sprite with a round soft chunk, colour as in change 1 (lighter than the
   floor on dark, cool-shaded on light), and keep them low: landing speed 650 -> 420, cone 70 -> 50, so they travel along
   the floor instead of rising to head height. Debris shadow: neutral cool grey at alpha 0.25 instead of saturated blue.

## Attempt 2 (final)

Date: 2026-11-19. Looked once at `Saved/VfxV3/hd3/sheet_{jump,run_skid,ground_bounce}.jpg` (light) and
`dsheet_{jump,run_skid,hit_debris,ground_bounce,hit_medium}.jpg` (dark). Light hits were not recaptured: the attempt 1
verdict is reused. Same limits: 400 px tiles, every 2nd or 4th frame, nothing tested in engine by the reviewer.

The khaki on the dark floor is gone: dust there is now white to pale grey, clearly lighter than the floor, and reads as
snow. The dark bounce cloud went from a thin haze to a large lobed cloud. The price is that dark-floor dust is now rather
flat and opaque (see checks).

| Effect | Light: vs DK / phone | Dark: vs DK / phone | Notes |
|---|---|---|---|
| Landing dust | COMPARABLE / PASS | COMPARABLE in size and read, flat in look / PASS | Light v156-172: pale blue low ring ~2.5x the character. Dark v156-172: white sheet ~3 m wide, even tone, sharp outline, almost unchanged from v164 to v172. |
| Run start | NOT / FAIL | NOT / FAIL (marginal) | Nothing at v100-128 on either floor. Only the turn's run start shows, as a halo around the legs (dark v156-168; faint on light). The body covers it from the game camera. |
| Skid | COMPARABLE / PASS | COMPARABLE / PASS | v136-152 on both floors: ~1.2-1.5 m cloud beside the feet. A ball rather than a wedge. |
| Medium hit | NOT (close) / PASS (attempt 1) | NOT (close) / PASS | Dark v26 and v46: a small white flash is now visible at the hip (~50 cm), then ~70 cm streaks v28-32. Reads, but it is smaller than the character; DK's burst is larger than the character. |
| Strong hit | COMPARABLE / PASS (attempt 1) | COMPARABLE / PASS | Dark v36-46: fan ~2x the character; v46-52 red debris and white dust puffs both visible. |
| Ground bounce | NOT / PASS | NOT / PASS | Volume and lobes are now at DK's scale on both floors, but the cloud is still centred at torso height around the attacker, not where the victim meets the floor. |

Checks:
- Soot / grey on light: none.
- Cream / khaki on dark: none any more.
- Opaque sticker on dark: **partly**. The landing sheet and the skid ball have one even tone and a hard silhouette, with
  little lit-top / shaded-side separation; the bounce cloud keeps its lobes and is fine. A first try: dark `EmissiveLift`
  `SmokeDust` 0.8 -> 0.65 and `SmokeLight` 0.75 -> 0.6, dark `OpacityMax` 1.0 -> 0.85 for those two, keep `DarkSnow` 0.4.
- Light like snow: yes on both floors.

### Can the owner be shown this as "comparable to DK Bananza"?

Not as a blanket claim. What can be said honestly: the effects are now readable from the game camera on both floors
(the original complaint), and three of them (landing dust, skid, strong hit) are at DK's scale and read. Medium hit,
run start and ground bounce are not there yet, and DK's level of detail (flash layer, solid chunks, floor reaction) is
still ahead of ours on every effect. Suggested wording: "readable now, landing / skid / strong hit in DK's range, three
items still open".

### Still missing for DK level

1. **gioco**: bounce cloud spawned at the floor contact point (`User.GroundPoint`), not at torso height.
2. **gioco, posa/look**: hit flash layer: star on its own unscaled age (0.083 s) plus a soft round glow ~180 cm with a
   wide orange fringe; the medium hit is still smaller than the character.
3. **dati, gioco**: run start hidden by the runner's body: spread it sideways out of the silhouette (spawn width 10 ->
   40 cm across the run, two side puffs at +-45 degrees) and confirm in the log that the first run start fires.
4. **posa/look**: solids: round soft snow chunks instead of 4-point stars, 16-28 cm, thrown low along the floor; debris
   shadows in neutral cool grey (alpha 0.25) instead of saturated blue blobs.
5. **posa/look**: dark-floor dust is flat: restore self-shading (values in the checks above) and let it erode after the
   peak instead of holding one shape for 8+ frames.
6. **posa/look**: floor reaction: soft contact darkening or a short mark under landing, skid and bounce (DK always leaves
   something on the floor); skid shaped as a low wedge ahead of the feet.
