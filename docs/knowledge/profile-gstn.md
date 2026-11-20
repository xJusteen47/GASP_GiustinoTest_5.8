# GSTN style profile (level 2)

The project layer on top of the general knowledge base (`xJusteen47/game-anim-vfx-review-skills`, folder `knowledge/`).
It **tunes** level-1 ranges and records the owner's taste, each rule with its **scope** and origin. It never restates a
principle. Built from the template `knowledge/PROFILE-TEMPLATE.md`. When this file and level 1 disagree, this file wins
only where it says so explicitly.

## 1. North star
- **"Ganondorf inside Astro Bot"** (owner 2026-11-19; scope: all animations): fluid and cartoony, but with the
  character's personality and weight. Take from Astro Bot: snappy toy timing, clean poses, readable shapes, tactile
  VFX. Take from the Ganondorf archetype: mass, power, deliberate commitment.
- **Quality bar** (owner 2026-11-15; scope: every animation review): "is this on par with Smash Bros or similar AAA?"
  Doubt = no. (Rubric axis BAR.)
- **VFX** (owner; scope: all VFX): Astro Bot / DK Bananza level of detail; realistic **behaviour**, stylised **palette**.

## 2. Animation
| Item | Value | Scope | Origin | Tunes |
|---|---|---|---|---|
| Timing scale | toy-stylised: about 20-40% shorter than realistic, wider contrasts, crisper holds | all | north star | P-TIME-01/02 |
| Mass class | medium-heavy (Ganon-style locomotion); playtest around the Overlord | locomotion, Overlord | owner 11-19 | P-TIME-02, P-WEIGHT-03/04 |
| Run start | 0.3 s stomp + frantic run; L3 = instant dash | run start | owner | P-ANTIC-03 |
| Landing | aerial → landing lag near-hard cut; impact and weight read | landings from aerials | owner 11-18 | P-WEIGHT-04, P-GAME-06 |
| Return to idle | feet re-plant one at a time, never both airborne | all hand-offs | owner 11-19 | P-CONTACT-03 (already level 1) |
| Intended foot slide | marked by the designer on the clip/montage (marker or foot-lock curve); not a FAIL | skids and similar | owner 11-19 | P-CONTACT-01 exception |
| Interpolation space | FK → natural arcs; IK → straight paths unless curves shape them | authoring | owner 11-18 | P-ARC-03 |
| Review views | from behind (game camera) AND from the side; a move must read in both | all moves | owner 11-12 | P-POSE-03, P-GAME-08 |
| Frame counting | designer-facing frame data counts from 1 | all | owner | — |
| Simulation rate | 62.5 Hz sim, 60 fps clips | all | design | P-GAME-05 |
| Isolation check | every clip passes it before in-game iteration; slopes up and down too | all clips | owner 11-19 | P-GAME-01 |
| Setup principle | designer-understandable, reusable with any animation set; no per-clip hacks | whole setup | owner 11-19 | P-GAME-10 |
| Starting material | never build a move from scratch from text: owner poses, free clips, measured reference poses | authoring | owner 11-15 | P-LIFE (how life is added) |
| Identity traits | each move brief lists 2-3 identity traits; changing them needs the owner | moves | project rule 13 | — |

## 3. VFX
| Item | Value | Scope | Origin | Tunes |
|---|---|---|---|---|
| Behaviour vs palette | realistic behaviour, stylised palette; avoid "stylised" for behaviour | all VFX | owner | theory §7, V-MATTER |
| Dust | grows only (never shrinks) while fading to alpha 0; landing dust = one central shockwave, shoulder-wide skirt below knee | dust, landings | owner | V-TIME-07, V-MATTER-02 |
| Shared look | one shared particle look + per-move generator presets | all VFX | owner 11-18 | — |
| All kits same | movement/VFX/feel polish in one shared profile; kits differ only by moves | all kits | owner 11-18 | — |
| Test backgrounds | light and dark floor | VFX review | project | V-READ-07 |

### Reference observations (moved from the general VFX theory, where they were project taste)
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


## 4. Owner rules: how to apply them
Follow the broad principle behind an owner remark, record it with its scope, never generalise a remark made for one
move, phase or frame to everything (project rule 12). Examples are examples.

## 5. Approved and locked versions
`docs/notes/anim-approvate.md` and tags `approved/<move>-<version>` (project rule 13b).
