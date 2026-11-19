# VFX v3 smoke redo, checkpoint A1 (landing dust): review and critic pass

Reviewer: Fable. Scope: `NS_GSTN_LandRing` only, material candidate (b) self-lit. Candidate (a) was not built, so there is no a/b comparison.
Sources: the two old/new sheets, the two f163 zooms, the flipbook preview, `dk_1.jpg`, `dk_sheet_b.jpg`, one extra crop strip (f157, f168, f174,
both floors), the three scripts, and pixel measurements on the raw frames (`Saved/VfxV3/a1/light3`, `dark4`). Screen values are sRGB 0-255.
Landing is at about f155, 60 fps.

## 1. Verdict

- **Light floor: yes.** A clear step. The old landing was one 3 m bluish haze with no parts; the new one is a low skirt of countable rounded
  forms with a light top and a cool underside, and it breaks into pieces and is gone 0.4 s after the landing.
- **Dark floor: partly.** The shape step is the same, but the value is wrong: every part of every puff is darker than the floor, so the dust
  reads as grey-blue blobs, not pale dust. The old cards, flat as they were, read as white dust there.
- Against DK: closer in kind (small lit forms, no card), not yet close in feel. See section 3.

## 2. Against the six look rules (REVIEW-SMOKE-LOOK.md section 4)

1. **Many small elements: pass.** 12 + 8 + 8 sprites, the largest 70 cm (0.39 of the character). Two notes: the skirt is 12, the plan said 16;
   and overlapping puffs still merge into a bank about 1.5 m wide (light f166-f168, right of the character).
2. **Lit form: pass on light, fail on dark.** Measured at f163 on puffs away from the character:
   - light floor: lit side 178/179/183, shaded side 117/131/160, floor 158. 50-60 % of the puff is brighter than the floor. Two tones, reads as a form.
   - dark floor: lit side 152/155/161, shaded side 97/112/143, floor 161/161/169. 0-10 % of the puff is brighter than the floor.
   The light direction is a fixed vector in sprite space (`LightDir` -0.45, 0.7, 0.55). It agrees with the sun in these captures only because of
   where the camera is (character shadow falls to the right, puffs are lit from the upper left). It will be wrong from the other side.
3. **Shape changes over life: partly.** The pop is good: the cloud reaches 75 % of its peak area in 3 frames (f156-f158). The break-up into
   separate blobs at f172-f179 is the best new thing in the effect. Between them it stalls: from f160 to f170 the cloud area stays within 10 %
   (8900-9900 px in my mask) and the forms do not change, because flipbook frames 5-17 are nearly the same image (preview rows 1-2) and the
   drag has stopped the puffs by then.
4. **Layers in time: partly.** Skirt, wisps and clumps all start on the same frame. The wisps (12-25 cm) and clumps (8-16 cm) are a few specks
   at f163 (light zoom, left of the character) and are not read as separate layers. Acceptable for a landing, weak for a slam.
5. **Wide variation: fail.** Sizes span only 2x (35-70 cm), there are 2 silhouettes, no mirror, no rotation. Dark f168 shows four puffs that are
   plainly the same clover of balls. Lifetimes (0.40-0.75 s) are spread well.
6. **The floor answers: fail.** I could not find the contact disc in any frame on either floor (0.22 opacity of a tint of the floor colour on the
   floor itself is under a few levels; whatever I measure there is mixed with camera drift). Nothing darkens under the individual puffs either.

## 3. Critic pass: what is still not as good as DK

In `dk_1.jpg` each puff is one dominant rounded mass with 2-3 bumps, clearly paler than the floor on top, about floor value underneath,
separate from its neighbours, sitting near the floor, gone in 0.2-0.3 s. Ours differ on all of these. Ranked by weight on the final look:

**Flaw 1 (`posa`): on the dark floor the puff has no light side; overall the puff sits below the floor value.**
DK tops are brighter than the floor; ours are 20 levels above it on light and 8 below it on dark, and the shade is 45-60 levels below on both.
Likely cause (not verified in the editor): the lit colour is an eye-adapted constant (floor colour leaning to `SnowColor`), while auto exposure
lifts the dark floor to the same screen value as the light one; the old material had a lit term that rose with exposure, this one does not.
Fix in `Tools/GAS/setup_presentation_vfx_puff.py`:
- `build_puff`: replace the single `Brightness` by `lerp(DarkBrightness, Brightness, bright)` using the `bright` term already in the graph;
  `Brightness` 1.0, new `DarkBrightness` 1.6. Target at dark f163: brightest 10 % of the puff >= 185 (now 152-167), shade about 130 (now 97-112).
  If it lands under 180, go to 1.8.
- `LOOKS["PuffDust"]`: `RampLo` 0.25 -> 0.18, `RampHi` 0.6 -> 0.5, `OccStrength` 0.7 -> 0.5, so a little more of the puff is on the light side.
  Keep `ShadeColor` (the light floor needs that shade to read).

**Flaw 2 (`posa`): each puff is a bunch of equal, perfect balls, and all puffs are the same bunch.**
6-8 lobes of similar radius with hard creases read as soap foam, grapes or popcorn (both zooms, f168 strip), not as a dust puff. DK's unit is
simpler and bigger-featured. With only 2 unmirrored variants the repetition is visible.
Fix in `Tools/GAS/vfx/make_vfx_puff_flipbook.py` (`_lobes`, `_frame`, `flipbook`) and the material:
- main lobes `rng.integers(6, 9)` -> `rng.integers(3, 6)`; core radius 0.36 -> 0.44; other radii 0.17-0.29 -> 0.14-0.32; offset 0.12-0.34 -> 0.20-0.40;
  knobs `rng.integers(4, 7)` -> `rng.integers(2, 4)`.
- crease term 0.25 -> 0.15, and blend the normal across lobe joins over about 0.05 cell units instead of a razor line.
- 4 variants x 16 frames in the same 8x8 atlas (`FRAMES` 32 -> 16 in both scripts, `row_off = floor(rnd * 3.999) * 2`). The first 16 frames are
  nearly static today, so nothing is lost. Add a random mirror in `build_puff` (flip U and the sign of the normal's x): 8 silhouettes.
- give the first half some motion: rotate lobe centres about the puff centre by 25 degrees over life and slide each knob 1.2 rad over its parent.

**Flaw 3 (`dati`, with one small `posa` part): the puffs hang, float and linger.**
After the pop they stand still at full size for 10 frames, then deflate in place; the cloud is still half its peak at f174 (0.32 s) and 14 % at
f179 (0.40 s), where DK's small puffs are gone in 0.2-0.3 s. From f166 on they read as floating: light f174 has puffs at the crouched character's
shoulder height with nothing under them (some of that is distance behind him, but nothing ties them to the floor: the sprite shrinks around its
centre, so a 70 cm puff's bottom rises about 19 cm by the end, the flipbook lifts, and the curl noise is free to push upward).
Fix in `Tools/GAS/setup_presentation_vfx.py` `land_ring` and in `setup_presentation_vfx_puff.py`:
- Skirt: count 12 -> 16, size (35, 70) -> (25, 55), life (0.40, 0.75) -> (0.22, 0.50), curl (120, 0.02) -> (60, 0.02), ring z 14 -> 10.
- `LOOKS["PuffDust"]`: `ShrinkStart` 0.55 -> 0.30, `EndScale` 0.35 -> 0.15. In the flipbook, lobe death start 0.40-0.66 -> 0.25-0.55.
- Shrink around the bottom, not the centre: give `V3._scaled_uv` an optional pivot and use (0.5, 0.85) for the puff, so the base stays on the floor.
- `M_GSTN_VFX_ContactDisc`: `MaxOpacity` 0.22 -> 0.40, sprite size 170 -> 220, `FadeStart` 0.35 -> 0.5. Target: floor under the cloud 12-18 levels
  darker at f160.
- Targets for A2: cloud area at half of its peak by f166 and under 10 % by f172; no frame where a puff's base is visibly off the floor.

Not in the top 3 but must not be lost (`gioco`): `LightDir` has to come from the sun direction in view space (set by the presentation component
through a material parameter collection or a Niagara user parameter, cosmetic only, outside the simulation). With a free camera the fixed vector
will put the light side away from the sun half of the time. The plan already listed this; A1 skipped it.

## 4. Anything broken

- **Wrong value on the dark floor**: yes, flaw 1. This is the only thing I would call broken.
- **Floating puffs late in life** (f166 onward, both floors) and **contact disc invisible**: flaw 3.
- Black quads: none. Atlas bleed or cell borders: none seen. Hard clip line at the floor: none (f157 bases sit cleanly, depth fade 12 cm is fine).
- Sorting pops: none found. Frame-to-frame luminance steps in boxes left and right of the character, f155-f179, both floors, are smooth
  (max 4.4 levels, no spikes). This is a weak test on stills; confirm on the video at full speed.
- The puffs neither cast nor receive shadow; the character's shadow passes under them untouched (f157). Expected for candidate (b), and one
  reason they float.
- The flipbook preview shows a slightly hairy rim on frames 8-16 (edge noise 0.008); it is not visible at game size. Leave it.
- The old and new captures are taken at different spots and headings of the level, so the side-by-side is not like for like. Minor.

## 5. Gate: show now, or one fix first

**One fix round first, then show without further rounds.** The light-floor half is ready to show today. The dark-floor half would undersell the
direction: the owner would see grey blobs where he used to see white dust, and may judge the whole option on that. The round should contain only:
1. `DarkBrightness` 1.6 and the ramp change (flaw 1), a parameter-level change.
2. The flipbook rebuild with 3-5 lobes, 4 variants and the mirror (flaw 2). This is the one that answers the likely comment "now it looks like foam".
3. Shorter life, smaller sizes, bottom pivot, visible contact disc (flaw 3).
The `LightDir` from the sun can wait for A2, but the gate video should not orbit the camera, or it will show.
After that round, show it whatever the result: A1 is a direction gate, and on shape, count and timing the direction is already visibly right.

## Fix round applied after this review (Opus, same thread; not re-reviewed by Fable, checked by eye on stills and on the mp4)
All three flaws, values as proposed except: flipbook core 0.38 (0.44 made a plain ball, the dropped v2 look), main lobes 4-5 at 0.24-0.38
from the centre, knobs 2-3, crease 0.15, 4 variants x 16 frames, layout turns ~20 deg; no random mirror. Material: `DarkBrightness` 1.6
lerped to `Brightness` by floor brightness, RampLo 0.18, RampHi 0.5, OccStrength 0.5, shrink about pivot (0.5, 0.85), ShrinkStart 0.30,
EndScale 0.15. Niagara: skirt 16 x 25-55 cm, life 0.22-0.50, curl 60; disc 220 cm, MaxOpacity 0.40. Frames: `Saved/VfxV3/a1/light5`,
`dark5` (jump, land v154/v156). Seen: dark-floor puffs now white and readable; light floor quicker and floor-hugging; puffs read
smaller overall than the round before (open for the owner: size/count is a dial).
