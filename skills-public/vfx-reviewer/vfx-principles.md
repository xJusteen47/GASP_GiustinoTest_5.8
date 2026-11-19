# Game VFX Principles: General Theory and Checklists

General game-VFX theory, engine-agnostic except where marked as an engine example (Unreal/Niagara). All numbers are **indicative ranges**, never verdicts: the final judgement is always by eye, on video, in the game. A project may add a `project.md` layer with its own rules, numbers and assets; **where the two conflict, `project.md` wins**.

Legend: **[F n]** = stated by a source (§17). **[O]** = observed in reference material (Astro Bot, Donkey Kong Bananza clips). **[A]** = adaptation or reasoning, to be verified in game. **[G]** = added after an external AI peer review (Gemini); not independently verified unless stated.
Frames are counted from 1; ~60 fps, 1 frame ≈ 16 ms.

§0 / §15 / §16-bis: project-specific, kept in the project layer.

---

## 1. What a game VFX is: information first, beauty second
1. **A game effect is a message.** First it says what happened, where, how serious, and what to do; then it is beautiful. VFX "teach how to play" [F1, F2]. If it carries no information it is noise and should go [F3].
2. **Animating an effect is animating energy** [F4]. Every effect has a cause (a punch, a braking foot, a falling body) and an energy that discharges over time. Shape, speed and duration follow from that cause; so think in **cause-effect chains**: hit → flash → shards → dust settling [F5].
3. **Attention hierarchy**: the attention an effect takes is proportional to its gameplay importance. "If everything is important, nothing is important" [F6]. Practice: an importance rating per event, with two ratings (who acts / who receives).
4. **The character comes before the effect.** An over-showy effect "overshadows" characters [F7]: it should hit without covering. The pose carries the long phases, the VFX the short ones.
5. **VFX are designed with the move, not after** [F8]: readability problems come from effects arriving late.

## 2. Shape language
1. **Shape follows energy** [F5]. Sudden discharge = pointed, angular, radiating shapes. Slowing motion or gentle energy = round, soft shapes. So the hit is a star or a shard, dust is a sphere.
2. **Shape is the first channel of the message** [F2, F3]: good/bad, dangerous/harmless read from shape before color. Miyamoto: danger = pointed or burning [F9]. Corollary: **same gameplay function = same shape** across all kits; the kit color changes only the secondary tint [F6].
3. **Primary, secondary, tertiary** [F10]. The primary carries the gameplay message: high value, clean silhouette, full opacity, one focal point. The secondary adds flavor (sparks, fragments): lower value, simpler shapes. The tertiary is atmosphere (smoke, haze): low contrast, slow motion, never competing with the primary.
4. **Big, medium, small** [F11]. One dominant big shape, a few medium, some small. Never all the same size: without scale contrast the effect becomes "uniform noise".
5. **Avoid perfect symmetry, twins and "sawtooth"** [F5]: mirrored pieces, regular spacing, perfect ripple circles look mechanical. Vary size, spacing and path. Deliberate exception: the clean radial bursts of the Nintendo style (regular fan with small jitter) are a style choice, not an error [A].
6. **Hard and soft edges together** [F10]: all soft = unreadable fog; all hard = sticker. The primary has a crisp edge, the atmosphere soft edges.
7. **Effect silhouette** [F5]: detail that leaves the main silhouette gets lost. Big mass first, then details.
8. **Direction in the shape** [F10]: elongate along the motion, leading edge lighter and crisper, tail darker and softer. A still shape then "goes" somewhere. A hit is oriented along the direction of the blow or throw.
9. **Negative space and rhythm** [F12]: holes, pauses and gaps make the filled shapes readable; an effect with no voids is a blob. Alternate dense high-frequency zones (sparks, shards) with smooth resting surfaces (a clean smoke mass) [G].
10. **Few and large > many and small** [F13, F14]: detail goes in the shape or the texture, not in the particle count. What matters is the right **density**, which Sakurai names as the most important effect parameter [F15].
11. **Squash and stretch in effects too** [G, F5, F20]: stars, puffs and waves are not rigid; they squash against the contact point and stretch along the motion (stretch from velocity) before expanding. An effect that grows only in uniform scale looks like a sticker getting bigger.
12. **Right orientation for each shape** [G]: with a free third-person camera, a camera-facing sprite always shows its face but has no direction in the world; ground discs and shockwaves should be **aligned to the impact plane**, stars and elongated trails **to the hit axis**; whatever must read from every side has volume (mesh).

## 3. Timing: the three phases
1. **Anticipation → climax → dissipation** [F10, F16, F11]. Every effect has these three phases; design them as intensity curves over time, not property by property [F11].
   - *Anticipation* (charge, wind-up): warns that something is about to happen. In VFX attached to a character, the **pose** usually provides the anticipation, not the effect; a charge effect is only for slow or charged moves [A].
   - *Climax* (impact, pop): the peak of value, saturation, size and element count. It falls **in the first quarter of the effect's life** [F10, F17].
   - *Dissipation* (erosion, decay): the rest of the life. Longer, softer, low contrast, and must not distract [F16].
2. **Impulse or rhythm** [F11]. *Instant impulse*: no anticipation, opens already at maximum; the choice for a hit (frame 1 = the biggest and clearest). *Crescendo impulse*: builds toward the peak (charge). *Regular rhythm*: calm cycles (ambient fire, indicators). *Broken rhythm*: intensity that varies over time. A complex effect chains them (charge at rhythm → launch at impulse → travel at rhythm → impact at impulse).
3. **Never linear curves** [F10]. Fast start and braking (ease-out) for anything that explodes or is pushed; ease-in only for what vanishes "sucked in" (pop-out). Constant velocity = dead effect.
4. **"If it feels long, it is too long"** [F10]. Short effects keep the screen readable and leave room for the next effect; long ones cover and cost (overdraw).
5. **Scale is communicated by timing** [F5]: big and heavy = slower, more frames; small and light = fast. A strong hit is not only bigger, it is also a bit longer (in hitstop and dissipation).
6. **Hold the key shape, then dissipate fast** [F5]: an effect that falls apart immediately loses its shape; better to hold it and then close it quickly. A "hold" is never fully still: small variation per frame.
7. **Overlap and offset** [F5]: elements do not all start and end together; secondaries start before the primary ends. Same idea as overlap in the body.
8. **Effect timing hooks to key poses** [F16]: poses and move frames exist first, then the effect is built on top; in fighting games it must coincide with the damage frame [F11].
9. **Emotional rhythm** [F11]: pulsing faster than a heartbeat = aggressive; slower = friendly (heal, pickup).
10. **Lock timing before polish** [F11, F18]: simple gray shapes + sound, tweak timing until it "sounds right", only then materials and details. Retiming a polished effect costs far more.
11. **Follow-through of secondary layers** [G, F20]: when the body stops (end of hit, hitstop), the air and light matter pushed by the hit keep going by inertia in the tangent direction, then brake. It is the body's overlap applied to the effect.
12. **Thermal decay** of energy [G, F5]: white core (peak) → saturated yellow/orange edge → dark red → cold smoke. Holds for fire, explosions and "hot" hits; skipping a step costs credibility. For non-thermal elements (electric, ice) the same idea: white at the core, saturated tint, then muted tint.
13. **Death according to matter** [G, F10]. *Energy and solids*: close by edge erosion (threshold on the mask) or contraction, not by veil; slowly falling opacity leaves a gray veil and overdraw. *Gases* (dust, smoke, snow, steam): they **disperse** like real ones, growing while opacity falls to 0; they do not shrink while staying opaque (it looks like a deflating object, not air). Avoid the veil by making alpha truly reach 0 within the life and lowering contrast, not by shortening the growth.

**Indicative ranges (frames at ~60 fps)** [A]: hit flash 1-2 frames; whole hit 12-20 frames (200-330 ms); dust 15-27 frames (250-450 ms); landing ring 15-21 frames; debris < 1 s.

## 4. Volume and layers
1. **A good effect is a composite of layers**, each with its own role, shape and timing [F19, F20]. Typical layers of an impact, in order of birth [A]:
   | Layer | Role | Typical life |
   |---|---|---|
   | Flash / core | where and when (the contact frame) | 1-3 frames |
   | Primary shape (star, wave, shard) | direction and force | 6-15 frames |
   | Solid debris | matter, weight, physics | 20-60 frames |
   | Smoke / dust | volume, material, air | 15-40 frames |
   | Ground mark / shadow / light | depth and position | as long as the primary |
   | Hitstop, shake, sound | feel and rhythm | 4-12 frames |
2. **Layers are separated by contrast**: only one layer light and crisp, the others subordinate [F10]. Two layers of equal strength compete and the eye does not know where to go.
3. **Additive blending alone is not enough** [F19]: it blends with any background but vanishes on light ones; it needs its own contrast (a dark edge, an opaque body, a value different from the background).
4. **Volume with light, not outline** [F5]: smoke volume comes from shading and scene light, not an outline. Big masses "swallow" small ones as they rise.
5. **Composition**: the effect guides the eye to the important point (the contact, the target), like a cover [F1].
6. **Separate phases into event-driven layers** [F16]: so a phase can be lengthened or shortened (e.g. by hitstop) without breaking the others. Engine example (Niagara): presentation events → separate Niagara systems/layers.

## 5. Readability at the gameplay camera
1. **The farther the camera, the more hit feedback matters** [F21]. With a wide camera (e.g. ~86-95° horizontal FOV) a small hit risks vanishing: a **minimum on-screen size** is needed [A].
2. **Read from where the player looks**: judge from behind (gameplay camera) and from the side; a flat effect (sprite, disc) seen edge-on vanishes, so hit shapes need volume or must face the camera [A].
3. **Depth is read from the ground** [F9-bis, A]: contact shadow, ring or mark under an airborne impact, light projected on the floor. In Astro Bot the crisp shadow under the character is the first depth cue [O].
4. **The character stays readable**: the effect covers neither the body nor the striking limb; with VFX on, the silhouette of the active frames still reads [F7].
5. **Honest reach** [F6]: an active effect does not make the hit look shorter or smaller than the hitbox.
6. **The first frame counts**: a visible change on frame 1 after input (pose or effect) [F22].
6-bis. **Readable and believable together**: for gases, readability comes from timing, position, light and material color, not from full opacity or inflated size. If to "read" the cloud becomes an opaque disc or a flat PNG, realism is lost and the effect is wrong even if it reads.
7. **Density with several players**: in PvP effects add up; the opponent's effects that threaten me matter more than my cosmetics [F6]. Short effects and few layers remain the first defense.

## 6. Color and value against the background
1. **Value (light/dark) is the tool of attention** [F1, F10]: the eye goes to the point of highest value contrast. Practical test: look at the freeze-frame **in grayscale**: if the hit is not the lightest (or most contrasted) point of the frame, color will not save it [A].
2. **Body at mid values, the maximum only in the core** [F10]: no pure white or black as a base; the highest value is reserved for the contact flash. So the flash "punches through" and the rest does not dazzle.
3. **Saturation steps** [F10, F17]: muted low environment → characters → VFX with a higher ceiling → UI. Effects cut through the background because their saturation sits above the world's, not because they are bigger.
4. **Complementary colors: one dominates** [F10]: the accent is small and less saturated. Few colors per effect (analogous or triad), a "narrow" gradient in light areas to avoid muddy grays [F17].
5. **Contrast and saturation mark the phases** [F16]: climax = maximum contrast and high saturation; dissipation = low contrast, low opacity. Smoke that stays saturated and contrasted at end of life "doesn't go away".
6. **Color carries state** [F16, F6]: team, element, stunned, broken shield. One state = one tint, everywhere.
7. **Two mandatory backgrounds**: verify each hit on a light background and on a dark or saturated one. On snow and bright sky light VFX vanish: there a saturated edge or warm core works, not white [O DK, A].
8. **Effects take the scene's light** when they are matter (smoke, dust, debris) and **emit** it when they are energy (flashes, trails) [O, F23]. Mixing the two roles produces "flat gray" or "dirty neon".

## 7. The Astro Bot / DK Bananza look: real behavior, stylized palette
1. **Three families of matter, three treatments** [O, F23]:
   - *Energy* (flashes, stars, trails, lasers): emissive, crisp, short, pure colors; reads on any background.
   - *Gas* (smoke, dust, snow, steam): realistic, soft, translucent, **lit** by the scene, **colored by the material** that produced it (pink on the pink enemy, white on snow) [O].
   - *Solids* (debris, shards, confetti, coins): glossy meshes, rounded edges, shadows, physical bounce [O].
2. **Real behavior, style in color** [F4, F5]: smoke rises and slows, dust disperses and falls, debris bounces and stops, scene light models them. What is "of style" is the palette: clean saturated colors, material tinting the gas, few tints per effect. Do not simplify gas into flat cartoon silhouettes: that is the cause of "clouds like big flat PNGs". For solids, clean rounded shapes remain fine (glossy meshes, §7.1), always with true physics. Avoid the word "stylized" for behavior.
3. **The effect is born from material data** [F24]: in DK Bananza where it breaks, how much, and of what material drive the effect, and voice and sound are synced to the final blow. Destruction effects are generated automatically from the materials' textures (800+ types). The tactile feel comes from effects and sounds that **respond to every break**, not from fixed animations [F24]. For a project: surface hit → color and type of dust/debris [A].
4. **"It is more fun to destroy what is beautiful"** [F25]: the effect is worth as much as its contrast with what it breaks; a poor environment makes even the best VFX poor. (Scope: levels/exploration, not PvP arenas.)
5. **What is seen in the DK Bananza clip** [O, TV capture, approximate colors]: at each hit a short orange/yellow star with bright sparks, a fast ground shockwave, ice and snow chunks flying off the ground, dust puffs on landings; hits read well on blue snow because the core is warm and saturated, complementary to the cold background.
6. **What is seen in Astro Bot** [O]: crisp white radial flash + short colored streaks + sparks, over physical dust/leaves; crisp contact shadow; soft translucent smoke colored by the material; saturated emissive rings + glossy solid confetti; warm directional light with rim light and purple haze in the background.
6-bis. **What Team Asobi says (CEDEC 2025, physics and graphics only, nothing on smoke or effect materials)** [F32]: up to ~7,500 physics particles per scene at 60 fps (PS5), only for small objects and simple convex shapes; destruction fragments in limited number and **removed after a time** to hold 60 fps; hybrid fountains (column = mesh, only the falling drops simulated); "below 60 fps is a bug". The principle applies (cap on pieces, short life, mesh for the predictable part), not the numbers, which are for a PS5 [A].
7. **The "generic Unreal look"** is recognized by: many small dots, diffuse glow everywhere, unlit gray translucency, procedural noise, dull colors, identical sparks everywhere [A].

## 8. Smoke and dust: how they behave
1. **Different causes, different behaviors** [F5]. *Dust* is pushed by an external force and **falls back** by gravity. *Smoke* rises because it is hot, **slows** as it cools, **rolls up** where it meets cold air. *Steam* vanishes early (absorbed into the air). Lifted *snow* behaves like dust but lighter and brighter.
2. **Burst then braking** [F5, F13]: explosive start (maximum velocity at frame 1), strong drag, nearly still before vanishing. Smoke at constant velocity looks like a conveyor belt.
3. **Landing and braking dust spreads along the ground** [A, F5]: it is air pushed out from under the body, so radial and low; it rises only a little and falls.
4. **Rolling**: masses turning on themselves in a direction coherent with the push; alternating forces give the S-shaped oscillation [F18].
5. **It detaches, breaks, disperses** [F18, F5]: smoke breaks into pieces and opens up; big pieces absorb small ones as they rise. Each mass **grows while fading to alpha 0**, like real dust in air; it does not shrink opaque and leaves no lingering veil (alpha reaches 0 within the life, §3.13).
6. **Volume with light** [F5]: no outlines; scene light from above, shadow below, rim toward the camera. Smoke colored by the material is lit, not gray: gray smoke on a light background "dirties" the screen [A].
7. **Do not cut on planes**: smoke touching the floor or a wall must fade (depth fade), otherwise it shows the hard line of the quad [F26].
8. **Shape variety**: not equal circles rising; masses of different size, overlapping, with holes [F5].
9. **Cheap versions of semi-realistic smoke** [G, A]: (a) groups of low-poly Masked mesh spheres that take light and shadow from every angle and have no overdraw (suited to a low-quality preset); (b) translucent sprites with fake two-tone lighting (light toward the light, shadow tint downward, half-Lambert style) instead of precomputed light maps; (c) closing by mask erosion. The lit flipbook with motion vectors stays for the high preset.
10. **Anchored to the ground** [G]: contact dust is born on the ground plane and follows its normal, it does not float above.

## 9. Impact, hitstop and the impact frame (Sakurai)
Hitstop rules themselves are covered by other sources. Here is the relation **between hitstop and VFX** [F21, F27, A]:
1. **The contact frame is a single event**: on the same frame freeze, flash, shake, sound and start of the reaction begin. If the flash arrives one frame after the freeze, the hit feels "soft".
2. **During hitstop**: the contrast between **frozen bodies** and an **exploding effect** sells the weight of the hit. The external peer review claims that in Smash, Street Fighter 6, Guilty Gear Strive and DMC5 the **hit mark** (flash, star) advances at full speed while characters are frozen, and that an effect slowed to 0.15 over 8 frames of freeze advances ~1 frame and looks like a render error [G]. Not verified on video (Sakurai read only from summaries), but the reasoning is consistent with §3.2 (instant impulse) [A]. Proposal: **energy** (flash, star, wave) at full speed during the freeze; **matter** (shards, dust, smoke) braked or still, then restarting with the character. Whether to apply a global effect slow-down during hitstop (and at what value) is an art-direction decision. A global slowdown remains suitable for cinematic slow-motion (final zoom), not for ordinary hitstop [G].
3. **Proportion**: more damage = longer hitstop, bigger and slightly longer effect, with a **cap**; projectiles less hitstop; more players = shorter hitstop (per-mode setting) [F27].
4. **Direction**: the primary shape follows the direction of the hit or throw; the shake is horizontal on the ground, vertical in the air, relative to the screen [F27, A].
5. **Impact frame** [F28]: one or a few stylized frames, often monochrome or color-inverted, on the contact of the strongest hits. They work by contrast and directionality. Use: only maximum-importance hits (smashes, finishers), never on frequent hits, always as a disableable setting (flashing-light sensitivity) [A].
6. **Blade and limb trails** [F29]: make the hit feel powerful and show the arc in the active frames, which the body alone does not show from every view. The trail lasts the active frames plus a few frames, does not exceed the recovery, and does not hide the limb. (Sakurai's video on trails and pitfalls was read only as a summary.)
7. **A whiff is different from a hit**: whiff = trail only; hit = trail + flash + hitstop + debris. If whiff and hit look alike, the hit loses value [A].

## 10. Technical choice: flipbook, sprite, mesh, ribbon, light, decal
| Technique | When | Look | Main cost |
|---|---|---|---|
| **Sprite** (camera-facing quad, single texture) | flashes, stars, sparks, cores | crisp if Masked/emissive | overdraw if large and translucent; reduced by cutting the quad to the shape (cutout) [F14, F26] |
| **Flipbook** (sprite with frame sequence, sub-UV) | deforming gas: smoke, fire, explosions | rich, "realistic" | texture memory, overdraw; fewer frames with **motion vectors** (e.g. 121 down to 32 frames, double resolution) [F30]; fake light with precomputed light maps from 4 or 6 directions [F30]. But motion vectors + light maps = 2-4 samplers and math on translucent quads: **high preset only**; on low, mesh spheres or sprites with 2-tone fake light (§8.9) [G] |
| **Mesh particle** | debris, shards, shockwaves, rings, confetti | solid, lit, shadows | draw calls per emitter, triangles; almost no overdraw if opaque |
| **Mesh with scrolling texture** | tornadoes, volumetric trails, waves | clean, continuous | one shader; replaces hundreds of predictable particles [F14, F16] |
| **Ribbon / trail** | limb and blade trails, trajectories | continuous line | vertices, translucency along the trail |
| **Short dynamic light** | flash lighting floor and character | very "tangible" | per-pixel light cost, heavy on integrated GPUs [G]; medium/high preset only, few, short, no shadows; on low it is replaced by an **emissive ground disc** (fake light bounce) + core bloom [G, A] |
| **Decal / ground disc** | impact mark, effect shadow | anchors the effect to the ground | projection; a projected blob decal may prove invisible, a ground disc is the safer choice |
General rules: **detail goes in the texture, not in particle count** [F14]; "use particle systems only when you must", prefer a scrolling mesh for what is predictable [F16]; grayscale channel-packed textures with color from a parameter, so one texture serves many effects [F16].

## 11. Performance on low-end hardware
1. **Overdraw is the first enemy** [F14, F26, F10]: every stacked translucent layer recomputes the same pixels. Smoke easily reaches 5-20 layers. Doubling a quad's side = quadrupling its pixels [F14]. Remedies: fewer layers, smaller on-screen particles, shorter life, quads cut to shape, opaque/Masked where possible, depth fade only where needed. Indicative target in combat: **3-5 stacked translucent layers** at most at the worst point [G].
2. **Few particles per effect** [F14]: 3-10 for effects that can fire en masse; a 30-particle effect dropped to 2 (flash + wave) by baking detail into texture.
3. **CPU for few, GPU for many** [F26, F31]: Epic only says CPU suits emitters with few particles and that it must be measured per platform [F31]; one source puts the crossover around ~1000 particles [F26], Gemini around ~100-200 [G]. **No number is verified**: if bursts are tens of particles they stay on CPU; measure only if an effect exceeds ~100 [A]. Every emitter has a fixed cost: fewer emitters, more renderers or mesh arrays in the same emitter [F31].
4. **Niagara (engine example)** [F31]: an Effect Type per system with scalability levels (graphics presets), distance culling and instance cap, **pooling**, **fixed bounds**, no warmup, mesh shadows and collisions off if not needed, big bursts spread over several frames.
5. **Shaders** [F14, F26]: sums, products, lerp are cheap; sin/cos/pow, SceneColor and refraction are expensive (+20-80 instructions). Order of magnitude for large effects: 100-150 instructions; mobile 80-120. The **number of texture reads** also matters: at most ~2 for a large translucent shader; SceneColor and refraction forbidden on the low preset [G].
6. **Indicative time budgets**: one source gives < 1-2 ms of Niagara tick on weak hardware [F26]; for low-end hardware a reasonable target is **< 0.5 ms** for combat VFX, peaks up to 1 ms [G]. To be measured, not assumed.
7. **Expensive modules** to avoid in bulk [F14]: collision (replace with braking at a fixed height / analytic plane), noise (replace with velocity curves), trails.
8. **How to check** (engine example, Unreal): Shader Complexity and Quad Overdraw views, Niagara Debugger and `stat Niagara`, low graphics preset on during capture [A].

## 12. Process
1. The VFX is born **in the move's sheet** (importance rating, intended shape, layers) [F8].
2. **Block-in**: gray shapes and timing with sound, judged in game; then materials, then details [F11, F18].
3. A **style guide** for the game with: allowed shapes, value and saturation range, timing rules, what is solid, gaseous, energetic [F17].
4. **Review in two passes** (VFX off, then on), two backgrounds, two views, at real speed and then frame by frame.
5. **An effect is compared with a reference of the same class** (hit with hit, dust with dust), **on video**: first write the reference dossier, the builder uses it as a target and the reviewer as a yardstick. The checklist does not replace the comparison.
6. **Technical doubts on the engine** (engine example, Unreal: Niagara, materials, rendering): ask the AI assistant built into the editor, which gives engine-specific advice (needs the editor open). Useful UE 5.9 tools: Niagara Python API, animation notifies that launch Niagara.

## 13. Checklist for whoever builds an effect
1. What does it communicate, to whom (who acts / who receives), with what importance rating?
2. What is the cause and what energy does it discharge? Does the shape follow (pointed/round)?
3. Which layers, each with a role? Only one is the primary?
4. Timing: climax in the first quarter, non-linear curves, duration that "doesn't feel long"?
5. Hooked to the right frame of the move (contact, start, landing)?
6. Emissive energy, lit and material-colored gas, glossy solids with shadow: every piece in its role?
7. Does it read in grayscale, on light and dark backgrounds, from behind and the side, at maximum camera distance?
8. Does it not cover the character, not lie about reach?
9. During hitstop does it slow and restart with the character?
10. Cost: particle and emitter count, overdraw (≤3-5 layers), texture reads, low preset?
11. Does it squash and stretch along the motion? Does each shape have the right orientation (camera, plane, axis)?
12. Does it die according to matter: energy and solids by erosion or contraction; gases growing while fading to alpha 0; no lingering gray veil?
13. Does it read **and** look like the real material (dust, smoke, snow), without inflated opacity or size?
14. Placed beside the reference video (dossier), does it match? Sizes relative to the character, life, dispersion, light.

## 14. Checklist for whoever reviews
Provided by the reviewer skill (12 yes/no questions judged from the video + 2 instrument checks, FAIL labels, verdict format).

## 16. Limits
- No video watched: GDC 2017 (Keyser/Chamberlin), Sakurai (effects, hit mark, trails), Lyndon GDC 2018 and Quitevis GDC 2026 read only as summaries or abstracts. *Elemental Magic* read through a thesis that summarizes it [F5], not the book.
- No Team Asobi source on Astro Bot VFX; for DK Bananza only the GDC/CEDEC 2026 sessions on destruction and voxels. The [O] observations come from reference clips (the DK clip is a TV capture).
- Performance numbers [F26] are from a single author; they hold as orders of magnitude, to be measured on your hardware.

## 17. Sources
F1 vfxapprentice.com/blog/five-artistic-principles-gaming-vfx · F2 GDC 2017 "Visual Effects Bootcamp: Artistic Principles of VFX", J. Keyser (Riot) and H. Chamberlin (Blizzard, Hearthstone), gdcvault.com/play/1024439 + thread realtimevfx.com/t/artistic-principles-of-vfx/4081 ·
F3 80.lv/articles/vfx-staples-shape-color-and-motion (Jevremović) · F4 J. Gilland, *Elemental Magic* vol. I-II, Focal Press 2009/2012 (review parkablogs.com) · F5 O. Salla, Theseus thesis 2017 theseus.fi/bitstream/handle/10024/137717 (summarizes Gilland) ·
F6 leagueoflegends.com/en-us/news/dev/clarity-in-league + realtimevfx.com/t/anatomy-of-a-league-of-legends-missile-part-2/13062 · F7 gonintendo.com/contents/12076 (Sakurai, effects and character prominence) · F8 gdcvault.com/play/1034360 (Rocksteady, GDC 2024) ·
F9 iwataasks.nintendo.com/interviews/wiiu/super-mario-3d-world/0/2 · F9-bis iwataasks.nintendo.com/interviews/3ds/super-mario-3d-land/0/3 · F10 vfxapprentice.com/blog/10-league-of-legends-vfx-design-tips (from the style guide of Jin ho Yang, Riot) ·
F11 vfxapprentice.com/blog/the-soul-of-effects-what-is-timing-in-vfx · F12 GDC 2026 "Controlled Chaos: 12 Principles of Design for VFX", R. Quitevis (Beyond-FX), abstract · F13 vfxapprentice / Riot, "few large" (F10 point 1) ·
F14 pocketgamer.biz "VFX optimisation in midcore games" · F15 gonintendo.com/contents/33516 (Sakurai, effect density) · F16 = F3 (phases, trigger-driven layers, scrolling on meshes) · F17 vfxapprentice.com/blog/creating-vfx-style-guides-for-games ·
F18 design.tutsplus.com/tutorials/how-to-animate-explosion-effects--cms-30960 · F19 gonintendo.com/contents/9838 (Sakurai, layers and additive) · F20 GDC 2018 "Zip! Thwack! Ping! Animation Principles of VFX", M. Lyndon (SideFX), abstract ·
F21 gonintendo.com/contents/23556 (Sakurai, hit mark) · F22 tvtropes Masahiro Sakurai On Creating Games (summaries) · F23 project reference notes on the Astro Bot look (emissive flashes, lit smoke, solid debris principle) ·
F24 dev.classmethod.jp/en/articles/cedec-2026-voxel (DK Bananza, CEDEC 2026) · F25 kotaku.com/donkey-kong-bananza-nintendo-gdc-2026-2000678021 · F26 adamkomarowski.github.io/unrealtechart/book/vfx ·
F27 sourcegaming.info/2015/11/11/thoughts-on-hitstop-sakurais-famitsu-column-vol-490-1 + nintendowire.com (Eight Hit Stop Techniques, 2022-12-12) · F28 80.lv/articles/learn-how-to-add-accents-to-your-animations-with-impact-frames · F29 gonintendo.com/contents/30552 (Sakurai, blade trails) ·
F30 loronsodev.github.io/posts/explosion-fx (motion vector, TLRB light) + forums.unrealengine.com flipbook motion vectors · F31 dev.epicgames.com/documentation/en-us/unreal-engine/scalability-and-best-practices-for-niagara ·
F32 cgworld.jp/article/202510-cedec-astrobot.html (Team Asobi, CEDEC 2025: H. Yamaguchi, T. Yoshida) · G = external AI peer review (Gemini) of the first version of this theory
