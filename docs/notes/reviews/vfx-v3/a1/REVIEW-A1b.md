# VFX v3 smoke redo, round A1b (soft "realistic" landing dust): review and critic pass

Reviewer: Fable. Scope: `NS_GSTN_LandRing`, MI `SmokeReal` on `T_GSTN_VFX_SmokeFB`, against A1 (MI `PuffDust`) and the owner's two sentences.
Sources: 6 images looked at once each (both sheets, both f166 zooms, the flipbook preview, `dk_1.jpg`), the three scripts, and pixel
measurements on the raw frames (`Saved/VfxV3/a1/light9`, `dark9` = A1b; `light5`, `dark5` = A1) and on the two flipbook PNGs.
Screen values are sRGB 0-255, 1920x1080, 60 fps. The owner's YouTube reference was not available. No video was watched.

## 1. Verdict

- **Light floor: partly.** The edges are about 3 times softer than A1 (edge gradient p95 4.8 against 12-18), the dust hangs longer (gone at
  +38 f against +22 f) and there is no crisp toon outline left. But each puff is still a solid ball with a blurred rim, and it does not
  "fade while it gets bigger": it erodes and shrinks at almost full brightness (see flaw 1). It reads as cotton wool, not as dust.
- **Dark floor: partly, and weaker than the light floor.** The puffs merge into two soft masses left and right of the character, about +19
  levels over the floor, with no inner form. It is close to the old "haze with no parts" that A1 was built to replace.
- **Closer to DK than A1: partly.** Softer edges and a longer hang go the owner's way. The light top and darker base that A1 had, and that the
  DK puffs in `dk_1.jpg` also have, are lost. A1b traded form for softness instead of adding softness to form.
- Does it answer "fade toward alpha 0 while growing": **no on the stills, partly in the numbers.** The fade is in the material, but the flipbook
  erosion removes the puff before the fade is seen.

## 2. Critic pass: the three flaws, by weight

**Flaw 1 (`posa`, heaviest): the puff has an opaque core and dies by erosion, not by thinning out.**
- Flipbook alpha: maximum 1.00 through frame 6 and 0.96 at frame 9; mean alpha inside the shape 0.72-0.89. Only the rim is translucent.
- Erosion: coverage of the cell falls from 0.19-0.26 (frame 3-6) to 0.05-0.09 at frame 9 in three of four variants. Variants 0 and 3 are
  empty from frame 12, so the last quarter of the life draws nothing. The shape half-width of variant 0 goes 0.52 to 0.27 while the
  material scale goes 0.55 to 0.93: on screen the puff gets smaller in the second half of its life.
- On screen (light floor): the lit area halves between +18 f and +25 f (27k to 15k px) while the brightest part stays at +21 levels. The
  opacity drop only shows from +30 f (+17) to +35 f (+12), on the last scraps. This is the opposite order to what the owner described.
- Fix, `Tools/GAS/vfx/make_vfx_smoke_flipbook.py`, `_frame`: threshold `th = 0.22 + 0.72 * smooth(t, 0.25, 1)` to
  `0.18 + 0.22 * smooth(t, 0.4, 1)`; edge `_smooth(dens, th, th + 0.22)` to `th + 0.55`; multiply alpha by 0.7 (peak 0.7, mean near 0.4);
  drop the `1 - 0.35 * ...` term. Then `setup_presentation_vfx_puff.py`, `SmokeReal`: `FadeStart` 0.3 to 0.15, `StartScale` 0.55 to 0.45.
  The material fade then does the whole exit, on a shape that is still growing.

**Flaw 2 (`posa`): flat lighting and weak, uneven billows, so the soft puff has no volume.**
- Light floor: lit pixels are +15 over the floor (171/173/179 against 153/154/156) and there is almost no shade side: 0.3k px darker than
  the floor against 2.9k px at -16 in A1. Inner detail (high-pass std inside the puff) is 2.0 against 4.5 in A1; dark floor 1.9.
- The shade tint gives a baby-blue cast on a neutral floor (blue minus red +8 on the puff, +3 on the floor). DK dust takes the ground colour.
- The four variants do not match: variant 2 (preview rows 5-6) is a smooth bean with no billows that never erodes (alpha 0.65 on the last
  frame); variant 3 (rows 7-8) has a dark square hole and speckle; only variants 0 and 1 look like smoke.
- Fix, `SmokeReal` in `setup_presentation_vfx_puff.py`: `Wrap` 0.4 to 0.25, `RampLo` 0.1 to 0.2, `RampHi` 0.8 to 0.65, `OccStrength` 0.6 to
  0.8, `ShadeColor` (0.55, 0.62, 0.8) to (0.60, 0.63, 0.72). In the flipbook: normal strength `k` 9 to 14, `h = _blur(dens, 3)` to 2, noise
  scale 0.32 to 0.42 (0.55 gave fur), and `dens = mass * (0.35 + 0.95 * billow)` to `0.55 + 0.75 * billow` so the core cannot open holes.

**Flaw 3 (`dati`): sixteen equal balls on an even ring, three times the screen area the owner approved.**
- Peak lit area is 30k px at +13 f against 10k px at +7 f for A1, at nearly the same brightness. The owner called the A1 size "probably fine".
  The note "the soft shape fills about half the frame" is low: measured half-widths are 0.50-0.62 of the half cell.
- The puffs sit evenly spaced and equal in size around the character, each one separate. The 8 wisps (25-50 cm) on the same flipbook read as
  small round "ears" stuck on the big puffs (f166 light, left and right puff). DK shows 3-4 overlapping puffs of mixed size, bunched on one side.
- Fix, `Tools/GAS/setup_presentation_vfx.py`, `land_ring`: Skirt count 16 to 10, size (60, 120) to (45, 105), life (0.50, 0.95) to
  (0.45, 0.85), ring (36, 12) to (30, 10). Wisps: count 8 to 5, size (25, 50) to (90, 140), speed 820 to 420, on a new look `SmokeHaze`
  (copy of `SmokeReal` with `OpacityMax` 0.35): one thin low layer under the puffs instead of small balls.

## 3. Anything broken

- **Dark ring with a pale centre under the feet** (f166 light zoom; a dark ellipse is also on the sheet at +13 f and +24 f). It reads as a
  hole in the floor. Either the contact disc (220 cm, 0.40) or the character's shadow, with self-lit puffs on top that ignore the shadow.
  Cause not confirmed from stills: check on the video, and try the disc at `MaxOpacity` 0.25 if it is the disc.
- Black quads: none. Wrong colours: only the blue cast of flaw 2.
- Hard cut on the floor: none seen (`FadeDistance` 20). The square-looking right mass on the dark zoom is the character in front of the puff,
  not a quad edge: the soft flipbook has alpha 0.00 in the outer band of every frame measured.
- Sorting pops: not visible on stills, and the light-floor area curve is smooth (13, 19, 24, 25, 30, 30, 28, 23, 16, 7, 2k px). This is a weak
  test. The dark-floor frames after +18 f could not be measured cleanly (camera and character move). Confirm on the video at real speed.
- Snow clumps: no clash. At 8-16 cm they are a few white specks at the feet on the light floor and are not seen on the dark zoom. DK also
  mixes crisp chunks with soft dust. They could go up to 12-22 cm, but that is a dial, not a fault.
- The "dark floor" renders at 142-150 on screen after auto exposure, so both tests are mid-grey floors. A truly dark test is still missing.
- Side note on A1: `T_GSTN_VFX_PuffFB` variants 0 and 3 reach alpha 1.0 in the outer band (frames 3-9) and are trimmed by the cell edge
  fade. Fix the lobe spread if A1 is carried forward.

## 4. Recommendation

- **One short fix, then show.** Do flaw 1 only (flipbook threshold and alpha, `FadeStart`, `StartScale`; regenerate the texture, rerun both
  captures). It is the owner's own sentence, and on the current frames it is not met: he would likely repeat the same comment.
  Flaws 2 and 3 can wait for his answer on the direction.
- Then show A1 and A1b in one video: same spot, same camera, both floors, real speed and one pass at 0.25x. Ask one question: stylised or soft.
- **For A2 carry A1b forward** as the base: the soft flipbook and the grow-and-fade exit are the owner's stated direction. Bring back from A1
  the form shading (flaw 2) and the size (flaw 3). Keep `PuffDust` as the fallback look and for the snow clumps; do not delete it.
- Still open from A1: `LightDir` from the sun, shadow on and under the puffs. Both matter more for soft smoke than for the toon puffs.

## Fix applied after this review (Opus; flaw 1 only, as recommended; not re-reviewed by Fable, checked by eye)
Flipbook `_frame`: threshold `0.18 + 0.22 * smooth(t, 0.4, 1)`, soft edge `th + 0.55`, alpha x0.7, late `1 - 0.35 ...` term removed.
`SmokeReal`: FadeStart 0.15, StartScale 0.45. Frames `Saved/VfxV3/a1/{light10,dark10}` (land v154/v155). Seen: the puffs now thin
out and dilute while they grow instead of shrinking; noticeably fainter overall (owner to judge). Flaws 2 and 3 open for A2.
