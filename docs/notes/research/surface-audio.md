# Surface-dependent audio (and haptics) research (2026-11-21)

Companion to `haptics.md`. Research only. Confidence: from Epic docs/forum titles (incl. the forum thread "How to Expand GASP Foley System for Different Surfaces in MetaSound?") plus general UE knowledge. Items marked (verify) must be checked in the project before coding: open `docs/knowledge/gasp/gasp-structure-5.9.md` and ask the Unreal Assistant "how does GASP stock pick foley sounds" first (project rule 14).

## 1. What stock GASP already does (verify exact names)
- Clips carry **foley AnimNotifies** (footstep/foot plant, scuff, hand plant, body fall, etc.) identified by a foley event tag. These are the same notifies we already fire per foot plant.
- On notify, GASP resolves a surface by line trace from the foot/bone, reads the hit **Physical Material -> Surface Type**, and plays a sound from a **foley audio bank / data asset**, usually via a MetaSound with parameters (verify).
- Forum evidence says stock content per surface is thin: the mechanism exists, the content is what we extend. We should not build a second footstep system.

## 2. Tools for context-dependent sound
- **Physical Material / Surface Type** (Project Settings > Physics > Physical Surfaces): the standard floor key. Needs Physical Materials on materials/landscape layers and "Return Physical Material" on the trace.
- **MetaSounds**: one MetaSound per event family with integer/enum inputs (footwear, floor) choosing between sample banks, plus float inputs (speed, weight, fall speed) for volume/pitch/layers. Set from BP via Audio Parameter interface (verify). Per-voice cost higher than a plain wave: cap footstep voices and give a simple variant for the Switch 2 profile.
- **Sound Cue** with Switch/Random nodes: lighter fallback.
- **Audio Modulation**: not for picking sounds; for global behavior (ducking, SFX/Footsteps/Haptics volume sliders, mix states). Optional phase 2.
- **Attenuation + concurrency** assets so a brawler with many characters does not spam steps.

## 3. One table for sound and rumble
Single data asset keyed by **(event tag, footwear, floor)** with columns: Sound (MetaSound or wave), volume/pitch scale, Haptic effect (Force Feedback Effect), haptic scale, optional Niagara (dust/splash). Fallback chain: exact row -> footwear x Default floor -> event default -> parent event tag. Magnitude (speed, fall speed, hitstun) goes to both the MetaSound and the haptic scale. The floor is resolved once per event by the stock GASP trace and passed to both consumers, so sound and rumble never disagree.

### 3b. Owner decision: two axes (2026-11-21)
Owner: start from stock GASP sounds and basic materials. The four types he named are **footwear, set per skin**, not floors:
- **Metal** (knight boots), **Clay-like** (what GASP has now = stock, the default), **Barefoot**, **Shoe sole**.
- Axis 1 = **footwear**, a per-skin property (enum/tag on the skin data; same per-skin override pattern as our choosers, stock fallback). Axis 2 = **floor** from GASP Physical Materials (start with what stock GASP already distinguishes; add floors later, only if needed).
- Footwear `Clay` = stock GASP behavior, so a skin without a footwear set sounds exactly as today.
- Metal and Barefoot rows can reuse stock sounds first (pitch/volume/EQ variants) and get proper samples later.

## 4. Recommendation (extends stock GASP)
Keep the stock foley notify + floor trace; extend only the lookup:
1. `GSTN_SurfaceResponseTable` (DataAsset, Content/GSTN) as in 3/3b, referenced by the stock foley bank as an override with stock fallback (stock content untouched; add a row in `docs/notes/gasp-divergenze.md`, rule 14).
2. `Footwear` enum on the skin/character data (Clay default, Metal, Barefoot, ShoeSole), passed into the lookup; BP-readable.
3. One MetaSound per family (Footstep, Landing, Scuff) with footwear and floor inputs; start with stock samples.
4. Same lookup feeds the haptic backend (`haptics.md`); sim events use it through the presentation component.
5. Debug menu (every setting also in GSTN Debug Options): checkbox "Surface audio", checkbox "Surface haptics", dropdown "Footwear override" (Skin default / Clay / Metal / Barefoot / Shoe sole), dropdown "Surface debug" (Off / Print / Draw trace).

Cost ~6-8 $ (table + enum + footstep/landing wiring, stock sounds); extra samples are content work.

**Alternatives**: B. Sound Cue switch per footwear x floor (simplest, Switch 2 fallback profile). C. Wwise/FMOD switches (powerful, heavy dependency, not now).

## 5. Open points
- Does stock GASP already read a floor type we can reuse? Verify first in editor via the Unreal Assistant.
- Which floors beyond stock appear in real levels (only add when needed).
