# Surface-dependent audio (and haptics) research (2026-11-21)

Companion to `haptics.md`. Research only. Confidence: from Epic docs/forum titles (incl. the forum thread "How to Expand GASP Foley System for Different Surfaces in MetaSound?") plus general UE knowledge. Items marked (verify) must be checked in the project before coding: open `docs/knowledge/gasp/gasp-structure-5.9.md` and ask the Unreal Assistant "how does GASP stock pick foley sounds" first (project rule 14).

## 1. What stock GASP already does (verify exact names)
- Clips carry **foley AnimNotifies** (footstep/foot plant, scuff, hand plant, body fall, etc.) identified by a foley event tag. These are the same notifies we already fire per foot plant.
- On notify, GASP resolves a surface by line trace from the foot/bone, reads the hit **Physical Material -> Surface Type**, and plays a sound from a **foley audio bank / data asset**, usually via a MetaSound with parameters (verify).
- Forum evidence says stock content per surface is thin: the mechanism exists, the content is what we extend. We should not build a second footstep system.

## 2. Tools for context-dependent sound
- **Physical Material / Surface Type** (Project Settings > Physics > Physical Surfaces): the standard floor key. Needs Physical Materials on materials/landscape layers and "Return Physical Material" on the trace.
- **MetaSounds**: one MetaSound per event family with integer/enum inputs (footwear, floor) and float inputs (speed, weight, fall speed). Set from BP via Audio Parameter interface (verify). Per-voice cost higher than a plain wave: cap footstep voices and give a simple variant for the Switch 2 profile.
- **Sound Cue** with Switch/Random nodes: lighter fallback.
- **Audio Modulation**: not for picking sounds; for global behavior (ducking, SFX/Footsteps/Haptics volume sliders, mix states). Optional phase 2.
- **Attenuation + concurrency** assets so a brawler with many characters does not spam steps.

## 3. One table for sound and rumble
Single data asset keyed by **(event tag, footwear, floor)**. Sound columns are not full-matrix samples (see 4): they hold the footwear layer and the floor layer. Other columns: haptic effect (Force Feedback Effect), haptic scale, optional Niagara. Fallback chain: exact row -> footwear x Default floor -> event default -> parent event tag. Magnitude (speed, fall speed, hitstun) goes to both the MetaSound and the haptic scale. The floor is resolved once per event by the stock GASP trace and shared by sound and rumble.

### 3b. Owner decision: two axes (2026-11-21)
Start from stock GASP sounds and basic materials. The four types named are **footwear, set per skin**:
- **Metal** (knight boots), **Clay-like** (what GASP has now = stock, the default), **Barefoot**, **Shoe sole**.
- Axis 1 = **footwear** (enum/tag on the skin data; per-skin override pattern like our choosers, stock fallback). Axis 2 = **floor** from GASP Physical Materials (start with what stock distinguishes; add floors only if needed).
- `Clay` = stock behavior: a skin without footwear sounds exactly as today.

## 4. Recommended design: layered MetaSound (owner confirmed 2026-11-21)
Instead of a footwear x floor matrix of samples, **one MetaSound per event family (Footstep, Landing, Scuff) mixes two layers**:
- **Footwear layer** (the contact: metal clank, soft clay thud, bare slap, rubber sole squeak) chosen by the `Footwear` input.
- **Floor layer** (the texture: stone, grass, sand, wood...) chosen by the `Floor` input (surface type).
- Inputs: `Footwear` (int), `Floor` (int), `Speed`, `Weight`, plus optional `Wetness`. Speed/weight drive both layers' gain, pitch and layer balance; each layer has random variation. Mix ratio per footwear/floor is a small tunable curve in the DataAsset.
- Benefit: N footwear + M floors = N+M sample sets, not N x M; new skins and floors are cheap; the haptic response uses the same two params. First pass uses stock GASP samples for both layers (pitch/EQ variants), proper samples later.
- Cost: two voices per step; cap concurrency and offer a one-layer Switch 2 profile (footwear layer only, or a pre-mixed cue).

Steps for the implementation thread:
1. `Footwear` enum on skin data (Clay default, Metal, Barefoot, ShoeSole), BP-readable.
2. `GSTN_SurfaceResponseTable` (DataAsset, Content/GSTN) referenced from the stock foley bank as override with stock fallback; gasp-divergenze.md row (rule 14).
3. The layered MetaSounds, first Footstep then Landing and Scuff.
4. Same lookup feeds the haptic backend (`haptics.md`); sim events use it through the presentation component.
5. Debug menu (one control per option): checkbox "Surface audio", checkbox "Surface haptics", dropdown "Footwear override" (Skin default / Clay / Metal / Barefoot / Shoe sole), dropdown "Layer mode" (Both / Footwear only / Floor only), dropdown "Surface debug" (Off / Print / Draw trace).

Cost ~6-8 $; extra samples are content work.

**Alternatives**: B. Sound Cue switch per footwear x floor (simplest, full matrix, Switch 2 fallback). C. Wwise/FMOD switches (powerful, heavy dependency, not now).

## 5. Open points
- Does stock GASP already read a floor type we can reuse? Verify first in editor via the Unreal Assistant.
- Which floors beyond stock appear in real levels (add only when needed).
