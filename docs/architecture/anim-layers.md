# Animation per feature, decoupled from the main graph (anim layers and anim sets)

Status: **proposal** (2026-10-25). Part A (the override-layer seam and the wall layer) is implemented on `claude/project-thread-2vuw3f`;
part B (the `GSTNAnimation` plugin with anim sets and skin rules) waits for the owner's answers in section 7.

## 1. Goal

Owner, 2026-10-25: wall slide and similar features (spin, drill, later others) bring **their own animations**, but **decoupled from the ABP**;
and **depending on the skin, or some of its traits, some abilities' animations may differ, while the hitbox stays the same**.

So:

1. The main anim graph (GASP `SandboxCharacter_Mover_ABP`) never names a feature. A feature's animation lives in its own **linked anim layer**
   (a separate anim Blueprint), chosen by **data**.
2. Which layer class and which montages a feature uses is resolved from **sources with priority**: character default < kit < weapon < skin rules.
3. A skin (or a trait of it) can replace a feature's animation **only in presentation**. Simulation (timelines, hitboxes, hurtboxes,
   `BakedSocket` frames, Mover queued moves) only ever reads the feature's **reference** montage, so gameplay is identical for every skin.

This is the Lyra pattern (weapon anim layers linked by `LinkAnimClassLayers`, variants as child anim Blueprints) plus a tag-driven choice.

## 2. Pieces

```
Main ABP (parent UGSTN_CharacterAnimInstance)            knows no feature
  GASP locomotion ──┐
                    ├─ Blend Poses by bool (bOverrideLayerActive, OverrideBlendTime, inertialization)
  Linked Anim Layer ┘     one node per layer interface (ALI_GSTN_Wall::WallLayer, later ALI_GSTN_Spin...)

Layer ABP (e.g. ABP_GSTN_WallLayer, parent UGSTN_WallAnimInstance, implements ALI_GSTN_Wall)
  reads its own data (WallAnim) and says when it wants the pose (IGSTN_OverrideLayerSource)

Anim set (data, part B)          Anim.Set.Wall -> layer class + montage overrides
Anim set component (part B)      resolves sets per tag from the sources, links the layer classes, maps reference -> visual montages
```

| Piece | Kind | Where | Part |
|---|---|---|---|
| `IGSTN_OverrideLayerSource` | C++ interface: `IsOverrideLayerActive`, `GetOverrideBlendTime`, `GetOverridePriority` | project `Animation/` (moves to the plugin in B) | A |
| `UGSTN_CharacterAnimInstance` | C++ parent of the main ABP: polls linked layers, exposes `bOverrideLayerActive`, `OverrideBlendTime`, `ActiveOverrideLayerClass` | project `Animation/` (moves in B) | A |
| `UGSTN_WallAnimInstance` | C++ parent of the wall layer ABP: `WallAnim` (wall-anim.md) | project `WallMovement/Animation/` (stays: it reads Mover wall state) | A |
| `ALI_GSTN_Wall`, `ABP_GSTN_WallLayer` | assets (interface + layer graph, placeholder clips) | `Content/Blueprints/Animation/` | A, editor step |
| `UGSTN_AnimSetDefinition` | `UPrimaryDataAsset`, type `GSTN_AnimSet` | plugin `GSTNAnimation`, module `GSTNAnimationCore` | B |
| `FGSTN_AnimSetRule`, `UGSTN_AnimSetCollection` | conditional choice of a set (tag query on cosmetic tags), list of rules | same | B |
| `UGSTN_AnimSetComponent` | presentation component on the pawn: resolve, link, `ResolveVisualMontage` | same | B |
| `Anim.Set.*`, `Anim.Trait.*` | native roots in the plugin; leaves: native where code owns the feature (`Anim.Set.Wall` in the project), ini for designers | same | B |

## 3. Part A: the override-layer seam (implemented)

- The main graph asks, it never decides: `UGSTN_CharacterAnimInstance::NativeUpdateAnimation` walks `GetLinkedAnimInstances()`; every one
  that implements `IGSTN_OverrideLayerSource` and is active competes by priority. Linked instances update before the main one, so the answer is this frame's.
- Each layer computes its own data on the game thread from presentation state (the wall layer reads the Mover component). Nothing is simulated,
  so a layer may keep timers; it must never write to the simulation.
- One Linked Anim Layer node per layer interface in the main graph. Today there is one (wall). When a second feature arrives (spin), the main graph gets
  a second node and the bool becomes "which interface" (an int from `ActiveOverrideLayerClass` or a slot tag). That is the only main-graph edit per new interface,
  and none per new variant.

## 4. Part B: anim sets (proposal)

```cpp
// GSTNAnimationCore (pure: Engine, GameplayTags; no GAS, no fork, no project)
UCLASS() class UGSTN_AnimSetDefinition : public UPrimaryDataAsset
{
    FGameplayTag SetTag;                                   // Anim.Set.Wall, Anim.Set.Spin, Anim.Set.Drill ...
    TSoftClassPtr<UAnimInstance> LayerClass;               // linked layer (implements the feature's ALI); a skin variant is a CHILD anim BP
    TArray<FGSTN_MontageOverride> MontageOverrides;        // { TSoftObjectPtr<UAnimMontage> Reference; TSoftObjectPtr<UAnimMontage> Visual; }
};

USTRUCT() struct FGSTN_AnimSetRule
{
    TSoftObjectPtr<UGSTN_AnimSetDefinition> Set;
    FGameplayTagQuery When;                                // against the cosmetic tags: skin SkinTags + Anim.Trait.* ; empty = always
    int32 Priority = 0;
};

UCLASS() class UGSTN_AnimSetCollection : public UPrimaryDataAsset { TArray<FGSTN_AnimSetRule> Rules; };

UCLASS() class UGSTN_AnimSetComponent : public UActorComponent      // presentation only, never read by simulation
{
    void SetSource(FGameplayTag SourceTag, UGSTN_AnimSetCollection* Collection, int32 SourcePriority); // character, kit, weapon, skin
    void SetCosmeticTags(const FGameplayTagContainer& Tags);       // skin tags + traits, set when the applied skin changes
    const UGSTN_AnimSetDefinition* FindSet(FGameplayTag SetTag) const;
    UAnimMontage* ResolveVisualMontage(UAnimMontage* Reference) const; // Reference when no override
    FOnAnimSetsChanged OnAnimSetsChanged;
};
```

Resolve (on any source or tag change, never per frame):

```
for each SetTag present in any source:
    candidates = rules of every source whose Set.SetTag == SetTag and When matches CosmeticTags
    winner = max by (SourcePriority, Rule.Priority)          // skin > weapon > kit > character by default
async-load the winners' soft refs, then:
    for each winner: Mesh->LinkAnimClassLayers(LayerClass)   // unlink the previous class of the same interface first
    rebuild Reference -> Visual montage map
    broadcast OnAnimSetsChanged
```

Who calls what:

- A feature names only its `Anim.Set.*` tag (in its ability/mode doc), never an asset. Its layer interface is fixed per feature.
- Kit, weapon, skin data each hold an optional `UGSTN_AnimSetCollection` (soft). The skin keeps its existing `SkinTags`; traits are more tags (`Anim.Trait.Heavy`,
  `Anim.Trait.Small` ...), so a rule like "drill layer for heavy skins" is data, and the skin asset itself does not need to know which features exist.
- The presentation path that plays a timeline's montage on the mesh asks `ResolveVisualMontage(Reference)`. The simulation path (timeline frames,
  hit/hurtbox windows, `BakedSocket` bake and lookup, root motion if any) reads `Reference` only.

## 5. Hitbox invariance (the rule)

1. Simulation never calls `UGSTN_AnimSetComponent`; it has no dependency on it (the component is in a presentation module; the combat NP module
   does not link it).
2. Baked sockets (M5 E8) are baked from the **reference** montage only. A visual override is never baked.
3. Editor validator for each `FGSTN_MontageOverride`: same section names, same section lengths within one frame, same play rate, and the visual
   montage's root motion must be off or equal (movement comes from Mover queued moves, not from the clip). A mismatch is an error, so a skin cannot
   desync the visible strike from the gameplay frames.
4. Hurtboxes follow the kit's `HurtboxSet`, not the skin. A skin with a very different silhouette is a design question, not an animation one.

## 6. Plan

| Step | What | Needs |
|---|---|---|
| A1 | `IGSTN_OverrideLayerSource`, `UGSTN_CharacterAnimInstance`, wall anim state + tests | done (cloud, build not yet verified) |
| A2 | Editor: `ALI_GSTN_Wall`, `ABP_GSTN_WallLayer` (placeholders), reparent main ABP, one blend + one linked layer node | editor free, owner told (shared Blueprint) |
| B1 | Plugin `GSTNAnimation` (Core): set definition, rule, collection, component, native roots, README; move A's two generic classes there | owner OK (section 7) |
| B2 | Validator of montage overrides (editor module), tests of the resolve order | B1 |
| B3 | Hook kit / weapon / skin collections in the loadout apply path (where `AnimClass` / `LocomotionAnimLayer` are applied today) | B1, M3 loadout code |
| B4 | Spin/drill (thread "Aggiungere spin e trapano") use `Anim.Set.Spin` / `Anim.Set.Drill` | B1 |

## 7. Questions for the owner

1. **New plugin `GSTNAnimation`** (recommended) or keep the anim set types in the project module?
2. **Skin variants by tag rules** in collections (recommended: skins stay untouched, traits are tags) or a direct list of anim sets on each skin?
3. **Montage overrides must keep the same sections and lengths** (recommended, it is what keeps hitboxes honest) or allow different lengths with
   retiming to the reference frames?
