# Surface-dependent audio (and haptics) research (2026-11-21)

Companion to `haptics.md`. Research only. Confidence: from Epic docs/forum titles (incl. the forum thread "How to Expand GASP Foley System for Different Surfaces in MetaSound?") plus general UE knowledge. Items marked (verify) must be checked in the project before coding: open `docs/knowledge/gasp/gasp-structure-5.9.md` and ask the Unreal Assistant "how does GASP stock pick foley sounds per surface" first (project rule 14).

## 1. What stock GASP already does (verify exact names)
- Clips carry **foley AnimNotifies** (footstep/foot plant, scuff, hand plant, body fall, etc.), identified by a foley event tag. These are the same notifies we already fire per foot plant.
- On notify, GASP resolves a **surface** by line trace from the foot/bone and reads the hit **Physical Material -> Surface Type**, then plays a sound from a **foley audio bank / data asset** (event x surface -> sound), usually a MetaSound with parameters (verify).
- Forum evidence says stock coverage per surface is thin by default (people ask how to expand it): the mechanism exists, the content (surface list, sounds) is what we extend.
- Implication: surface detection and the event hook already exist. We should not build a second footstep system.

## 2. Tools for context-dependent sound
- **Physical Material / Surface Type** (Project Settings > Physics > Physical Surfaces): the standard key. Needs Physical Materials assigned on materials/landscape layers, and "Return Physical Material" on the trace.
- **MetaSounds**: one MetaSound per event with a **surface input (int/enum or trigger-per-surface)** driving a switch/random-pick between sample banks, plus continuous inputs (speed, weight, fall speed) for volume, pitch and layer mix. Good fit: one asset, designer-tunable, parameters set from BP via Audio Parameter interface (`Set Integer/Float Parameter` on the audio component or a Sound Cue/Parameter Transmitter). Cheaper than many separate cues; per-voice cost is higher than a plain wave, so budget voices on the Switch 2 profile (cap simultaneous footstep voices, simple variant of the MetaSound for low profile).
- **Sound Cue** with Switch/Random nodes is the lighter fallback for low profiles.
- **Audio Modulation** (Control Buses/Mixes): not for picking sounds; use it for global behavior: ducking footsteps under music/hit sounds, per-category volume sliders (SFX/Footsteps/Haptics), mix states (e.g. hitstop). Optional, phase 2.
- **Attenuation + concurrency** assets: needed so a brawler with many characters does not spam steps (max count per category, volume scale by distance).

## 3. One table for sound and rumble
Key idea: a single data asset keyed by **(event tag, surface type)** that holds both the audio and the haptic response.
- Row = event (`Foley.Footstep`, `Landing`, `Bounce`, `Hit.Light/Heavy`...) x surface (`Default`, `Stone`, `Grass`, `Metal`, `Wood`, `Sand`...), with fallback to the event's `Default` surface and to the parent event tag (same pattern as our presentation table).
- Columns: Sound (MetaSound or wave), volume/pitch scale, Haptic effect (Force Feedback Effect), haptic scale, optional Niagara (dust/splash) so VFX can share it later.
- Magnitude (speed, fall speed, hitstun) is passed as a parameter to both the MetaSound and the haptic scale.
- Surface is resolved once per event (stock GASP trace) and passed to both consumers, so sound and rumble can never disagree.

## 4. Recommendation (extends stock GASP)
**Keep stock GASP's foley notify + surface trace; replace/extend only the lookup:**
1. Add our surfaces to Physical Surfaces and assign Physical Materials to the test-level materials (start with 4-5: Stone, Grass, Metal, Wood, Sand).
2. Create `GSTN_SurfaceResponseTable` (DataAsset under Content/GSTN) as in section 3, referenced by the existing foley bank as an override with stock fallback (stock content stays untouched; add a row in `docs/notes/gasp-divergences` per rule 14).
3. One MetaSound per event family with a surface input and speed/weight inputs; start with Footstep, Landing, Scuff.
4. Route the same lookup to the haptic backend described in `haptics.md`; sim events (hits, landings from the 62.5 Hz layer) use the same table through the presentation component.
5. Debug menu (project rule: every setting also in GSTN Debug Options): checkbox "Surface audio", checkbox "Surface haptics", dropdown "Surface debug" (Off / Print surface name / Draw trace).
6. Per-kit/per-skin override of the table, like our other presentation tables.

Cost estimate: ~6-8 $ for the table + footstep/landing wiring + 5 surfaces with placeholder sounds; sounds themselves are content work (free packs or recorded by the owner).

**Alternatives**
- **B. Sound Cue switch per surface** (no MetaSounds): simplest, works everywhere, no continuous parameters; good Switch 2 fallback profile.
- **C. Full audio-middleware path (Wwise/FMOD switches)**: most powerful (and what many studios use for surface switches), but a heavy dependency; not justified now.

## 5. Open points for the owner / next thread
- Which surfaces exist in the real levels? (needed to scope Physical Materials).
- Sounds: do we have a footstep sample source? Otherwise start with stock GASP sounds and add per-surface layers later.
- First verify in editor how GASP stock selects the sound (Unreal Assistant) before coding.
