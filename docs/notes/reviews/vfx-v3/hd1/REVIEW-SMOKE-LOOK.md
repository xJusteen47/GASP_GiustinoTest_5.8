# VFX v3 look review: why our effects read as flat cards, and a plan to reach the DK Bananza look

Date: 2026-11-19. Reviewer: Fable (senior VFX review pass, high effort). Scope: smoke and dust first, then the hit effects
(star, streaks, sparks, strong-hit debris, grains), folded into one look direction for the whole kit.

Owner's complaint: "our smoke is nowhere near DK Bananza; the clouds look like fairly big flat PNGs rather than something
real". This review agrees with him and explains why.

Images looked at, once each (10: the 8 asked for, plus the 2 hit sheets from the scope extension; `sheet_jump.jpg` had
already been read when the swap note arrived):
- Ours: `Saved/VfxV3/hd3/zoom_land_light_v162.jpg`, `zoom_bounce_dark_v112.jpg`, `sheet_jump.jpg`,
  `dsheet_ground_bounce.jpg`, `dsheet_hit_debris.jpg`, `dsheet_hit_medium.jpg`.
- DK Bananza: `Saved/VfxV3/hd3/zoom_dk_146.jpg` (it is in `hd3/`, not in `dkref/`), `Saved/VfxV3/dkref/dk_sheet_b.jpg`,
  `dk_sheet_c.jpg`, `dk_1.jpg`.

Code read: `Tools/GAS/setup_presentation_vfx_v3.py`, `Tools/GAS/vfx/make_vfx_v3_textures.py`,
`Tools/GAS/setup_presentation_vfx.py`. Context: `REVIEW-DK.md` in this folder.

Limits. The DK reference is a phone filming a TV: shape, size, count, timing and layering can be judged, the exact shading
model cannot. Where I say how DK builds something, it is what the footage shows plus normal practice for this kind of
game, and it is marked as an inference. Every number in the plan is a proposal to try: nothing here was tested in engine
(the editor MCP was not connected in this session). The figures in section 2 that say "computed" come from the texture
and material code, not from a measurement on screen.

## Short answer to the owner

You are right, and it cannot be fixed by tuning the current values. Three rounds of fixes made the dust readable from the
game camera by making the cards bigger and more opaque, and that is exactly what now makes them look like PNGs.

The cloud is built from 4 to 10 soft blurry cards, each 1.5 to 2.4 m wide (as big as the character or bigger), with a
single still image on each, almost no shading, and no movement after the first quarter second. DK Bananza does the
opposite: many small rounded puffs you can count, each with a light top and a shaded underside, which pop out, tumble and
shrink away in a few frames, on top of solid chunks of the floor. In the DK footage there is in fact no large smoke cloud
at all: the sense of volume comes from many small lit forms and from solids.

The hit effects have the same problem in another form: a few identical sprites from one geometric texture, scaled down
uniformly, frozen by the hitstop.

Recommended: option A below (clustered stylised puffs with an animated flipbook and real form shading, plus the same
"many small crafted elements, layered in time" rule for hits). About 30-50 $ in 5 checkpoints. It needs one owner
decision on the look direction, taken from a side-by-side test video at the first checkpoint, not from this text.

## 1. What DK Bananza does (from the footage)

Smoke and dust:
- `dk_1.jpg`: three separate round puffs behind the character, each about the size of his head or fist (my estimate:
  25-50 cm), pale on top and bluish underneath, with a defined edge. You can count them. They are forms, not a haze.
- `zoom_dk_146.jpg` (after a ground slam): no translucent cloud at all. The volume is carried by solid faceted ice
  chunks from 0.2 to 1 character in size, lit and shaded like the level, plus a few small white snow puffs and flecks at
  the crater's edge (each a few percent of the character's height).
- `dk_sheet_b.jpg` f146-161: the chunks stay about 0.5 s and tumble; the small white puffs are gone within 0.2-0.3 s.
- Nothing in the footage is a soft card wider than the character.

Hits (`dk_sheet_b.jpg` f131-143, f164-167; `dk_sheet_c.jpg` f208, f217-220, f232, f256):
- A white radial burst 1.5-2.5x the character made of dozens of thin rays of very different lengths, visible for 1-2
  sampled tiles (0.1-0.2 s).
- Under and after it, saturated orange/yellow jagged arcs and a pink/red splash on the floor: a second and third layer,
  each with its own shape and its own timing. The shape changes from tile to tile (rays, then arcs, then fragments): it
  is animated, not one image shrinking.
- The burst is centred on the contact point and is roughly symmetric; the floor reacts (cracks, chunks).

Inference (not provable from a phone video): DK's puffs are small sprites or blobs with baked or normal-mapped form
shading and a short flipbook, drawn in clusters. That is the standard way to get this look, and it matches what is visible.

## 2. Smoke: why it reads as flat cards, ranked by impact

| # | Cause | Where | Impact |
|---|---|---|---|
| 1 | Few, very large cards: the unit of the cloud is a 1.5-2.4 m quad | `setup_presentation_vfx.py` `land_ring`, `impact`, `skid_dust`, `run_start_dust` | Highest |
| 2 | The puff texture is a blurred blob with no form: wide feather, no billows, no creases | `make_vfx_v3_textures.py` `puff()` | Highest |
| 3 | No real shading: baked gradient of one sphere, rotated at random, squeezed to ~10 %, then drowned by `EmissiveLift`; lighting mode has no direction | `puff()`, `build_smoke_full()`, `SMOKE_LOOKS` | High |
| 4 | Still image: no flipbook, no rotation over life, no turbulence; the cards stop and then fade in place | `_smoke()`, material | High |
| 5 | Dissipation is a uniform threshold at full size, all puffs die together | `ErodeStart`, `EndScale=1.0`, lifetimes | Medium |
| 6 | Not grounded: camera-facing 2 m quads half buried and depth-faded, nothing on the floor under the cloud | `ring=(45, 12)`, `FadeDistance 35` | Medium |
| 7 | Hard outline effect from `RimShade`: a grey halo around a white disc | `build_smoke_full()` rim term | Medium |
| 8 | Secondary layers missing or wrong: no wisps, grains are 8-ray stars | `_grains()` uses `T_GSTN_VFX_Star` | Medium |
| 9 | Hitstop freezes the cloud (time scale 0.15) | game side | Medium (bounce, hits) |

### 2.1 Count and size (cause 1)

What the code spawns at intensity 1:

| System | Emitters | Count | Sprite size |
|---|---|---|---|
| `NS_GSTN_LandRing` | Puffs | 10 | 150-210 cm |
| `NS_GSTN_Impact` | Core / Lobes / Tops | 4 + 2 + 2 | 200-240 / 140-180 / 100-130 cm |
| `NS_GSTN_SkidDust` | Kick / Trail | 6 + 30 per s | 140-190 / 120-160 cm |
| `NS_GSTN_RunStartDust` | Puffs | 9 | 130-180 cm |
| `NS_GSTN_JumpPuff` | Puffs | 5 | 70-100 cm |

The character is about 180 cm. A cloud made of 8 cards that are each as big as a person can only look like 8 cards:
there is no parallax between elements, no small detail, and each card's round outline is visible on its own
(`zoom_bounce_dark_v112.jpg`: three discs, countable, one per card). DK's element is 5-8 times smaller and there are
several times more of them.

History matters here: `REVIEW-DK.md` (hd1) asked for bigger and more opaque puffs so the dust would read on a phone. That
fixed readability and caused this. Readability has to come from count, contrast and solids, not from card size.

Cost note: this is not more expensive. 10 cards of 200 cm cover about 400,000 cm2 of overdraw; 30 puffs of 50 cm cover
about 75,000 cm2.

### 2.2 The texture (cause 2)

`puff()` sums 5-7 wide gaussians (sigma 0.22-0.36 of the half cell) and maps density to opacity with
`_smooth(dens + 0.35 * (n - 0.5), 0.18, 0.75)`. Computed from that: after the material's `x 1.4` sharpen, roughly 40 % of
each puff's visible radius is a soft feather, and the noise only wobbles the outline at low frequency. So:
- The silhouette is a fuzzy circle. Clouds, stylised or real, are recognised by a lumpy, fairly crisp silhouette
  (cauliflower billows). Ours has none.
- There is no interior: no creases between lobes, no occlusion, no smaller lumps on the big ones. The lobes are summed
  into one smooth hill.
- 256 px per cell stretched over a 2 m card is about 1 texel per screen pixel at the game camera, all of it blur.
- 4 variants, static.

### 2.3 Shading and lighting (cause 3)

Four things stack up, each of which alone would flatten the puff:
1. **The baked shading is one sphere over the whole cell** (`z = sqrt(1 - x2 - y2)`, light from the top left), unrelated
   to the lobes. It is a smooth gradient across the quad, and its dark side falls mostly in the transparent corner.
2. **Random rotation turns the baked light.** `_smoke()` calls `particle(..., rotation=True)` (Sprite Rotation Mode
   Random), so each card's "top-left light" points in a random direction, including from below. Overlapping cards cancel
   each other.
3. **The range is squeezed.** Computed: inside the opaque core, G goes from about 0.62 to 0.86; with `ShadeMin` 0.60-0.70
   the shade term goes from about 0.86 to 0.95. That is a 10 % value change across a puff.
4. **`EmissiveLift` 0.5-0.8** adds 50-80 % of the base colour as unlit emissive (it was added in r6 because lit
   translucency came out far darker than the floor). On screen the puff is mostly self-coloured, so whatever shading is
   left is diluted again.

And the lighting mode is `TLM_VOLUMETRIC_PER_VERTEX_NON_DIRECTIONAL`: light is sampled at the 4 corners of a 2 m quad and
has no direction. The cloud cannot have a lit side and a shadow side that agree with the sun, it does not shadow itself,
and it casts nothing on the floor. A flat tone with a soft edge is the definition of a sprite card.

### 2.4 Motion (causes 4, 5, 9)

- Velocity 300-620 cm/s with drag 6: travel is v / drag = 50-100 cm, mostly done in the first 0.25 s. After that the
  cards stand still. There is no curl noise, no rotation rate, no per-layer speed difference; `rise` is 15-70 cm/s2,
  which is nothing over 0.6 s.
- The only change inside a card is the erosion threshold on the same static noise (B) and a scale-up inside the quad
  (`StartScale` 0.45-0.6 to 1 by 70-80 % of life). The puff keeps growing while it fades at full size (`EndScale` 1.0).
  "Grow and fade" is right for realistic smoke with a simulated flipbook; with a blob texture it reads as a picture being
  enlarged and faded out.
- Lifetimes are 0.50-0.62 s or 0.65-0.80 s, a narrow band: all the cards go at once.
- Seen in the sheets: the landing sheet is almost unchanged from v162 to v172 (`sheet_jump.jpg`), the bounce cloud from
  v118 to v130 (`dsheet_ground_bounce.jpg`). Ten frames of a still translucent shape.
- The hitstop slows the system to 0.15 after 2 free frames, which holds everything still even longer.

DK's puffs pop out, keep tumbling, and shrink to nothing instead of fading as a sheet.

### 2.5 Grounding and edges (causes 6, 7, 8)

- The landing cards are centred 12 cm above the floor and are up to 210 cm wide, so half of each quad is under the floor
  and is removed by a 35 cm Depth Fade. The bottom of the cloud dissolves into the floor instead of sitting on it, and
  there is no darkening under it. Soft particles are present and working; they are not the problem, the card size is.
- `RimShade` darkens the feather (0.60-0.72 at zero density). On the dark floor this draws a grey halo around each white
  disc (`zoom_bounce_dark_v112.jpg`), like an outer-glow layer style. It was added in r5 to give an edge; it now
  underlines the card's outline.
- `_grains()` uses `MI_GSTN_VFX_Grain`, which uses `T_GSTN_VFX_Star`: the snow grains are 8-ray stars 12-28 cm wide.
  They read as sparkles (already noted in `REVIEW-DK.md`, still open).
- Sprite renderers keep the default `SortMode = None`. It does not show today because the tone is uniform; it will show as
  soon as puffs have shading, so the fix must set view-depth sorting.

### 2.6 What is not the cause

- Depth Fade / soft particles: present.
- Colour and value against the floor: solved in hd1-hd3 (`ShadeTint`, `DarkSnow`, the `*Light` twins). Keep it.
- Particle budget: we are well under any limit; the fix lowers overdraw.
- Six-way lightmaps are not required for this look. They are for realistic smoke (option B).

## 3. Hit effects, same lens

What is on screen (`dsheet_hit_debris.jpg`, `dsheet_hit_medium.jpg`, dark floor):
- **Strong hit, v36-v44**: a fan of about 10 identical blades, same length, same width, same texture, all pointing to one
  side, held unchanged for 8+ frames by the hitstop. It reads as a feathered wing stuck on the victim. No centre, no
  flash in any tile.
- **Strong hit, v46-v52**: red glossy balls (the debris "gems" in the kit accent colour) with saturated blue blob shadows
  on the floor, a few small white 4-point stars, and 2-3 blurry dust discs.
- **Medium hit, v26-v36 and v46-v52**: a small white blot at the hip for one tile, then 4-6 thin needles above the
  shoulder. Smaller than the character's torso, partly behind the body.

Causes, ranked:

| # | Cause | Where | Impact |
|---|---|---|---|
| 1 | One procedural geometric texture for everything: a perfectly regular 8-ray star (also used for sparks and grains) and one tapered line | `star()`, `streak()`, `FLASH_LOOKS` | Highest |
| 2 | No shape animation: every element is one sprite scaled to zero (`EndScale 0`) | `build_flash()` `_life_size` | Highest |
| 3 | No variation: streaks are all 16 x 130 cm, same speed, same life (0.18-0.22 s) | `hit_spark()` | High |
| 4 | Frozen by the hitstop: a static arrangement held at time scale 0.15 | game side | High |
| 5 | One-sided fan (cone 150 degrees along the push, tilted up): no contact point, no burst | `streaks.cone((1, 0, 0.5), 150, 2200)` | High |
| 6 | The effect does not touch the world: no light on the characters or floor, no ground ring, no mark | missing | High |
| 7 | Flat unlit translucent with a hard fringe: white sticker with an orange outline, no glow falloff | `M_GSTN_VFX_Flash` | Medium |
| 8 | Debris are accent-coloured glossy balls with blue shadows: candy, not pieces of anything | `debris.mesh("Gem", "Debris", shadow="Debris")` | Medium |
| 9 | The star lives 0.042 s and is 1 sprite: in practice the flash layer is absent | `star.burst(1).particle((0.042, 0.042), ...)` | Medium |

The pattern is the same as the smoke: few large identical cards, one still image each, uniform scale as the only
animation, no layering in time, no reaction from the scene. DK's hit is three layers with different shapes and timings,
dozens of elements with a wide size spread, centred on the contact point.

## 4. One look direction for the whole kit

Six rules. Each one is the direct opposite of a cause above.

1. **Many small crafted elements, never a few big cards.** No sprite wider than about 0.5 character for dust, except one
   short soft glow under a hit.
2. **Everything is either a lit form or light.** Puffs and chunks have a light side and a shadow side that agree with the
   sun. Flashes are light, and light lands on the characters and the floor.
3. **Shape changes over life, not just scale and alpha.** Flipbooks for puffs and flashes; stretch by speed for streaks
   and sparks.
4. **Layers in time.** Frame 0-2 flash, 1-6 burst, 3-15 secondary (puffs, sparks), 10-40 residue (chunks, thin haze).
5. **Wide variation.** Sizes and speeds on a wide spread: a few heroes, many small. At least 2 texture variants per element.
6. **The floor answers.** Contact darkening under dust, a thin fast ring on strong ground events, chunks made of the
   floor's colour.

What stays from v3: the cue table and its counts/intensity, `User.SurfaceColor` from the physical material, `ShadeTint`
/ `DarkSnow` / `SnowColor` and the bright-floor blend, the exposure-proof emissive (Eye Adaptation Inverse), Depth Fade
(with a shorter distance for small puffs), the delayed `Lobes` / `Tops` idea in the impact, the ground plane parameters,
the mesh renderer + bounce pipeline for solids, the core + fringe flash material, the idempotent setup scripts and the
capture harness.

What goes: 150-240 cm cards, random rotation of baked light, `EmissiveLift` as the main source of brightness, `RimShade`
as an outline, "grow to full size and erode in place", star-shaped grains and sparks, accent-coloured glossy gems with
blue shadows, the one-sided fan of identical blades.

## 5. Options

| | A. Clustered stylised puffs (recommended) | B. Simulated smoke flipbook, lit | C. Mesh toon puffs |
|---|---|---|---|
| Look | DK / Nintendo: countable rounded puffs, soft two-tone shading, snappy | Astro Bot / UE demo: realistic wispy smoke | Hard cartoon: solid blobs, crisp |
| Puff source | Our own generated flipbook (lumpy metaball puff: normal, occlusion, crisp alpha). A pack flipbook can enter the test if the owner adds one | Pack flipbook (Niagara Footstep VFX, Big Niagara Bundle, PotaVFX Smoke Free) or our own bake with Niagara Fluids + Baker | Low-poly blob meshes with noise wobble and dithered soft edge |
| Lighting | Form shading from the texture normal against the sun direction, cool shadow tint | Six-way lightmaps or normal map, lit translucency | Real scene light and shadows |
| Needs the owner | Direction choice from a test video | Epic login to add packs; direction choice | Direction choice |
| Cost | 30-50 $ | 30-60 $, wider spread | 30-50 $ |
| Main risk | Our generated flipbook is not good enough on the first try | Goes realistic, not DK; pack style unknown; six-way must be hand-built | Close to the v2 glossy balls the owner already dropped |

### Option A (recommended): clustered stylised puffs

**Build**
- **Puff flipbook v2** (`make_vfx_v3_textures.py`, new function). Per frame 128 px, 8x8 atlas (1024 px), two variants in
  the two halves. Shape: 5-9 hard-union metaballs (creases between lobes, not a summed hill), edge feather about 10 % of
  the radius instead of 40 %. Channels: RG = normal from the lump heightfield, B = crease occlusion, A = density. Over the
  64 frames: lobes drift out and up, shrink one by one, the puff splits into 2-4 smaller blobs with holes. Erosion is
  part of the animation, not a uniform threshold.
- **Material** (`build_smoke_full`). Flipbook frame from particle relative time: the current 2x2 atlas pick generalises to
  NxN with two samples for frame blending, so no authoring-library change is strictly needed. Shading from the texture
  normal against the light direction, in the sprite's tangent space so particle rotation no longer breaks it; soft
  two-tone ramp; shadow side = surface colour x `ShadeTint`; occlusion from B. Two candidates to compare at checkpoint 1:
  (a) Default Lit with `TLM_SURFACE` and the normal plugged in (directional light from the translucency volume, cheap),
  (b) self-lit: unlit, own wrap lighting from a light direction parameter set by the presentation component (cosmetic
  only, outside the simulation). I expect (b) to win: `EmissiveLift` already makes the current look mostly unlit, and (b)
  gives the same result on every floor and exposure. Its price: puffs ignore local shadow and coloured lights.
  Remove `RimShade`. Depth Fade distance 35 -> 10-15 cm.
- **Niagara** (`_smoke` and the systems). Sizes 25-110 cm on a wide spread; counts x3 (table below); Curl Noise Force
  (stock module, present in the install) for swirl; a rotation rate of +-30-90 deg/s; lifetimes
  spread 0.25-0.7 s so puffs die one after another; scale over life: pop to full in the first 15 %, hold, then shrink to
  0.2-0.4 (not hold at 1.0); `SortMode` view depth on the sprite renderer.
- **Grounding.** Spawn on the floor with the puff's lower third at floor level; one soft contact-darkening disc per dust
  event (floor colour x a cool grey, alpha <= 0.2, 0.35 s), as already proposed in `REVIEW-DK.md` 2.0.2 and never built.
- **Solids.** Snow clumps: small round lit bits instead of stars, thrown low. Floor chunks: 3 faceted low-poly meshes in
  `User.SurfaceColor`, matte, 1-2 big (30-50 cm), 5-8 medium, tumbling, bounce, neutral cool shadow at alpha 0.25. Only
  where a surface is hit (landing from high, ground bounce, wall impact, strong hit near the floor).

Starting numbers (to try):

| System | Now | Proposal |
|---|---|---|
| Landing | 10 x 150-210 cm | 16 skirt puffs 35-70 cm (speed 350-650, drag 7) + 8 wisps 12-25 cm (speed 700-900, life 0.2-0.3) + 8 clumps; ring 2-2.5x the character's width, top below the knee |
| Impact / bounce | 8 x 100-240 cm | Core 8 x 60-110, Lobes 12 x 35-60 (delay 0.04 s), Tops 8 x 20-35 (delay 0.085 s) + 3-10 floor chunks; spawned at the floor contact point |
| Skid | 6 + 30/s x 120-190 cm | Kick 10 x 30-60 + trail 45/s x 25-45 cm, low wedge |
| Run start | 9 x 130-180 cm | 12 x 25-55 cm, thrown to the sides and back |
| Jump | 5 x 70-100 cm | 7 x 20-40 cm |

**Hits, same direction**
- **Flash**: replace the regular star with a 4-frame flipbook, 2 variants: frame 1 an irregular burst of 12-20 rays of
  random length and width, frame 2 hollowed out (the centre empties), frame 3 broken shards, frame 4 specks. Life about
  0.083 s on unscaled time, so the hitstop cannot freeze or stretch it.
- **Glow and light**: one soft round glow under the burst (about 180 cm, 3-4 frames, the only large sprite allowed) and a
  Niagara light for 3-4 frames in the fringe colour, so the hit lights the two characters and the floor. This is the
  cheapest single cue that the effect is in the world. No shadows on that light.
- **Streaks**: 16-24, lengths 40-220 cm and widths 6-24 cm on a wide spread, speeds 900-3000, full circle around the
  contact point with a bias toward the push (not a one-sided fan), length driven by speed (stock Scale Sprite Size By
  Speed) so they shorten as they slow instead of shrinking on a timer.
- **Sparks**: 20-30 hot dots of 3-8 cm stretched by speed, gravity, bounce on the ground plane on strong hits. No star texture.
- **Strong hit**: a thin fast ring flat on the floor (5 frames) and 3-5 small puffs from the new kit. The kit accent
  colour moves to the fringe and the light, not to candy debris.
- **Hitstop**: effects should keep animating while the characters freeze (flash on real time; the rest at 0.5 or more
  instead of 0.15).

**Checkpoints (one thread each)**

| # | Work | Opus | Fable review |
|---|---|---|---|
| A1 | Look test on the landing only: flipbook v2, material candidates (a) and (b), one test system, close captures on light and dark floors, side-by-side video with the current landing and the DK clip. A pack flipbook joins if the owner has added a pack | 5-8 $ | 2-4 $ |
| A2 | Convert the dust kit (landing, jump, run start, skid, impact, strong-hit dust): counts, sizes, motion, sorting | 4-6 $ | 2-4 $ |
| A3 | Solids and grounding: snow clumps, floor chunks, contact disc, neutral shadows, bounce cloud at the floor contact point | 3-5 $ | 2-3 $ |
| A4 | Hits: flash flipbook, glow and light, streak and spark rework, ground ring, effect time during hitstop (small C++ change in the presentation component) | 5-8 $ | 2-4 $ |
| A5 | Full capture (light, dark, one mid-value textured floor), owner video, DK side by side, one fix round | 3-5 $ | 2-4 $ |
| | **Total** | **20-32 $** | **10-19 $** |

A1 is a gate: if the owner does not see a clear step toward DK in that video, stop and reconsider B or C before
spending the rest.

**Risks**
- The generated flipbook may still look procedural. Mitigation: A1 compares it with a pack or engine flipbook; the engine
  ships `T_SmokeSubUV_8X8` (`Engine/Content/Tutorial/SubEditors/TutorialAssets`), realistic in style, usable as a
  stand-in to test the motion.
- Self-lit puffs ignore local lighting. Acceptable for this style; if a dark level needs it, blend in part of the lit term.
- Many small translucent sprites need sorting; with shading, popping would show. Set at A2, check at A5.
- The dynamic light on hits costs something on a low-end target. One unshadowed light for 4 frames; measure at A4.
- Changing the effect time during hitstop touches an earlier owner decision (2A).

### Option B: simulated smoke flipbook, lit (semi-realistic)

**Take or build**: a fluid-sim smoke/dust flipbook, either from a pack the owner owns (Niagara Footstep VFX, Big Niagara
Bundle) or the free PotaVFX Smoke Free, or baked by us from the stock Niagara Fluids templates
(`Grid3D_Gas_DustExplosion` exists in the install) with the Niagara Baker. Lit with six-way lightmaps or a normal map.

**Facts checked on this machine**: only the Game Animation Sample is in the launcher's vault cache, so none of those
packs is downloaded here. I found no six-way lighting asset or shader by name in the UE 5.9 install (searched
`Engine/Content`, `Engine/Plugins/FX`, `Engine/Shaders`), so a six-way material would have to be hand-built (two RGB
lightmap textures blended by the light direction in view space), and it only pays off if the flipbook comes with six-way
maps. I have not opened the packs, so their style and whether they include six-way or normal maps is unknown.

**Checkpoints**: B1 get the flipbook (pack imported by the owner: 3-5 $; our own bake: 8-15 $, the Baker has not been
driven from a script here) + review 2-4 $; B2 lit flipbook material 5-8 $ + 2-4 $; B3 convert the dust kit 4-6 $ + 2-4 $;
B4 hits as A4 5-8 $ + 2-4 $; B5 final as A5 3-5 $ + 2-4 $. Total about 30-50 $ with a pack, 35-60 $ with our own bake.

**Risks**: the result looks like realistic smoke, which is the Astro Bot reference of v3, not DK's rounded cartoon puffs;
realistic wisps on a toy-like character can look pasted in. Pack assets live outside the versioned content folders and
cannot go into a public repository, so every machine needs the pack (the laptop too), or the few textures used are
copied into the plugin under the Fab licence terms. Flipbooks at 2048 px x 2 textures per effect weigh more in memory.

**What stays**: everything listed in section 4. The motion and count changes of option A apply here too (smaller
elements, sorting, staggered death): a good flipbook on ten 2 m cards would still read as cards.

### Option C: mesh toon puffs

**Build**: puffs as low-poly lumpy blob meshes (3 variants) with a matte snow material, a noise wobble on the vertices, a
dithered soft edge, lit and shadowed by the real scene lights. They pop, tumble and shrink to zero. Solids as in A.

**Checkpoints**: C1 look test on the landing 5-8 $ + 2-4 $; C2 convert the dust kit 4-6 $ + 2-4 $; C3 solids and
grounding 3-4 $ + 2-3 $; C4 hits as A4 5-8 $ + 2-4 $; C5 final 3-5 $ + 2-4 $. Total about 30-50 $.

**Risks**: v2 already used mesh balls for dust and the owner dropped them ("no glossy balls"); this version is matte,
soft-edged and clustered, but it is the same family. Meshes cut into characters and floors with a hard line (the dither
softens it, and can shimmer with temporal anti-aliasing). Reads as cartoon, less as light snow dust.

**Why it is not the recommendation**: it gives the strongest volume for the least shading work, but it moves away from
"light like snow". Worth keeping for the solids (clumps, chunks), which option A already takes from it.

## 6. Decisions

**Owner-level**
1. Look direction: A, B or C. This changes one line of the v3 design sheet ("air = real smoke" becomes "air = stylised
   volumetric puffs" under A). Best decided on the A1 test video.
2. Whether to bring in Fab packs (B, or as an extra candidate in A1).
3. A thin ground ring on strong hits and ground bounces. v3 said "no rings"; DK has one.
4. Debris in the floor's colour instead of the kit accent colour (the accent moves to the flash fringe and the hit light).
5. Effects keep animating during hitstop (changes decision 2A).
6. Budget: about 30-50 $ for A.

**We just apply** (inside whichever direction is chosen): counts, sizes and spreads; texture generation; lighting mode
and shading maths; curl noise, rotation, scale curves, staggered lifetimes; sprite sorting; removal of `RimShade`;
snow clumps instead of star grains; neutral debris shadows; flash on unscaled time; bounce cloud at the floor contact
point (open since `REVIEW-DK.md`).

**What the owner must do himself**
- Watch the A1 side-by-side video and choose.
- Only if he wants pack flipbooks in the test or option B: add the pack to the project from the Epic launcher or the Fab
  window in the editor (his Epic login), and say which folder it landed in.
- Nothing else. No paid tool is needed for A or C.

## 7. Not verified

- Nothing was run in the editor. The texture and shading figures are computed from the code.
- DK's shading model is inferred from a phone video.
- Whether the stock Niagara light renderer and the Curl Noise / Scale Sprite Size By Speed modules can be set through
  `UGSTN_VfxAuthoringLibrary` as it is. The modules exist in the install; setting curve inputs may need a small addition
  to the library (an editor-module rebuild).
- The `.uproject` on this machine is associated with engine 5.9 while `CLAUDE.md` says 5.8; the engine checks above were
  made on the 5.9 install.
- The test level is a flat untextured grey room. DK's world is saturated and textured, which makes any side-by-side
  harsher on us. A5 adds one mid-value textured floor for that reason.
