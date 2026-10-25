# Combat Parts — destructible body parts, capped damage percent, crystal presentation (specification)

Part of the combat design ([combat.md](combat.md)): read §5.3, §12 and §15 there, M4 ([combat-m4-hits.md](combat-m4-hits.md))
§3, §7 and §9, [modularity-and-tags.md](modularity-and-tags.md) §3, §4 and §5b, and M8 ([combat-m8-minions.md](combat-m8-minions.md))
§3 and §9. Owner direction: [game-design-direction.md](game-design-direction.md) §1, §2, §2b and §5; it wins over this file.
Section numbers below are local to this file. Items marked *Proposal* are choices for the owner or the milestone owner to
confirm; *Reading* items interpret the owner's message (see the open questions list kept with the project notes).


> **Revision 2026-10-22 (owner direction, game-design-direction.md §5b):** (1) damage **inside** a part is shown only by a
> **material/shader** parameter (diegetic health: cracks, exposed crystal), never by spawned pieces; (2) a part that
> **detaches** is a real separation (mesh, chunk, physics), §6.2; (3) a **player is a single part**: when it breaks, the player
> dies (§4.5). Chips as spawned meshes/bursts are removed from §6.1.

## 1. Scope and place in the milestones

**Goal:** an entity (player fighter, enemy fighter, server-only minion) can have **parts**. A hit lands on one part; the
hit's **penetration** against the part's **hardness** decides how much of the hit damages the part and how much feeds the
entity's main pool; a part that passes its cap **breaks** (state change, events, possible launch of a chunk); the main pool
is a **capped damage percent** that also drives knockback, weight and presentation; passing the main cap destroys the
entity. Presentation shows the crystal under the skin: a damage material inside each part, limbs detaching on break, shatter on death.

In: part set data, penetration table, part and main damage rules, per-part stagger thresholds and reaction caps, weight and juggle reaction modifiers, part state
sync, break effects (derived tags, GE requests, disabled hurtboxes/hitboxes), events, minion path, presentation (client only),
tests and harness scenarios.

Out: new movement modes, the wall-splat/bounce rules (M4 launch and tumble), grab logic (M6 owns the grab, this file only
supplies a tag it can gate on, §7), thrown enemies as projectiles (M10), editor tooling beyond validators (M5 hooks, §8).

**Milestone (*Proposal*): a separate milestone "M4b Parts" between M4 and M5**, not folded into M4.

| Option | For | Against |
|---|---|---|
| Fold into M4 | One pass over the pipeline | M4 already carries spikes, lag compensation, dodges and presentation; parts are optional by owner direction and must be removable, so they cannot share M4's exit gate |
| **M4b (recommended)** | M4 stays shippable and testable without parts; M4b is additive and has its own gate; M5 editor can then include part gizmos once; M6 grab gating and M8 compact state can be planned against a fixed contract | Needs a handful of **generic seams in M4** (below) or M4b would edit the combat core |

**Seams that must land inside M4** (generic, inert without a provider, reviewed as combat changes in separate commits):

1. `FGSTN_HitResolutionContext` gains `FGSTN_HitReactionModifiers { float WeightScale = 1, KnockbackScale = 1,
   HitstunScale = 1, HitstopScale = 1, StaggerScale = 1; float StaggerThreshold = −1 (unset); EGSTN_HitReaction MaxReaction =
   Launch; bool bNoFlinch = false; }` (M4 §3) and `OutcomeExtensions` (`TArray<FInstancedStruct>`);
   `PercentScaling`, `HealthFraction` and `Fixed` models read the modifiers.
2. The victim inbox entry and the context carry `uint8 HurtboxIndex` (which hurtbox the overlap picked, §3.4).
3. **Outcome appliers**: a registered `IGSTN_OutcomeExtensionApplier` is called when the combat tick applies the outcome
   (the point where M4 §7 writes `ActionState`, requests GEs) for each `OutcomeExtensions` entry whose struct type it owns.
4. `DamageMode::PercentCapped` and the capped-percent knockback model (§4.1): `MaxHealth` is the cap in percent points.
5. Hit-data extension slot (`ExtensionData`, §5b table) and the per-field override variant for it.
6. Tick-extension point "before candidates" can mask hitbox groups of the entity's own active hitboxes (§3.5).

M4's `FGSTN_HitVictimView`/snapshot-extension seams of M8 §3 are reused; if M8 lands first they already exist.

## 2. Plugin and module boundaries (*Proposal*)

A separate optional plugin, `Plugins/GSTNParts/`, **L3b combat extension** (modularity §3.1): depends on `GSTNCombat`
and `GSTNInput` only through public headers, never on `GSTNMinion`, `GSTNDefence`, `GSTNLink` (siblings). `GSTNCombat` must
build, run and pass its tests with `GSTNParts` absent: without it an entity has plain hurtboxes and one health/percent.

| Plugin / module | Type | Depends on | Contents |
|---|---|---|---|
| `GSTNParts` / **`GSTNPartsCore`** | Runtime | `GSTNCombatCore`, `GameplayTags`, `Core`, `CoreUObject` | Pure, Automation-tested: `UGSTN_PartSetDefinition`, `FGSTN_PartDefinition`, `UGSTN_PenetrationTable`, `FGSTN_PartsState`, `UGSTN_PartMath`, `UGSTN_HitStep_PartResolve`, baked curves, quantisation, break-effect data structs, native tags. No world, GAS, NP. |
| `GSTNParts` / **`GSTNPartsGAS`** | Runtime | `GSTNPartsCore`, `GSTNCombatGAS`, `GameplayAbilities` | Break-effect appliers that need a GE (`FGSTN_PartBreakEffect_Stats`), event emission, tag-projection helpers |
| `GSTNParts` / **`GSTNPartsNP`** | Runtime | `GSTNPartsGAS`, `GSTNCombatNP`, NP fork | `FGSTN_PartsFragment` (sync-state fragment), `UGSTN_PartsTickExtension`, snapshot contributor, outcome applier, registration |
| `GSTNParts` / **`GSTNPartsPresentation`** | Runtime, **not loaded on a dedicated server** (module type `Runtime`, subsystems and components return `ShouldCreate = false` when `NM_DedicatedServer`; **a listen-server host loads it**, §5.5b) | `GSTNPartsGAS`, `Engine`, `Niagara`, `GeometryCollectionEngine`, `ChaosSolverEngine` | `UGSTN_PartsPresentationComponent`, pooled debris subsystem, `UGSTN_PartPresentationSet`, chip/shatter drivers |
| `GSTNParts` / **`GSTNPartsEditor`** | Editor | `GSTNPartsPresentation`, `UnrealEd` | Validators, Physics Asset → part set helper (M5 hook) |
| **`GSTNPartsMinion`** (separate tiny plugin) | Runtime | `GSTNParts`, `GSTNMinion` | The only place that knows both; see §5.5 |

Rules:
- `GSTNParts.uplugin` lists `GSTNCombat` as a dependency; `"EnabledByDefault": false`. Disabling it removes fragment, step,
  presentation and content; the M4 seams of §1 stay and are inert.
- Parts are attached to an entity by **data**, never by code in `GSTNCombat`: fighters through a kit extension
  `FGSTN_PartsKitExtension { TSoftObjectPtr<UGSTN_PartSetDefinition> PartSet; }` (the `FGSTN_KitExtension` array of §5b),
  minions through the bridge plugin's definition extension, both ignored with a warning when the plugin is off.
- A rules extension `FGSTN_PartsRulesExtension` (on `UGSTN_CombatRules`, `FGSTN_RulesExtension` array) holds game-wide
  values: default `UGSTN_PenetrationTable`, global weight curve default, presentation budgets, and
  the pipeline lists `UGSTN_HitStep_PartResolve` (§5.3). Missing from the pipeline = no parts (validator reports a kit with a
  part set under rules without the step).
- Dedicated-server builds do not load `GSTNPartsPresentation` and never reference its assets (soft paths only, §3.6). The reference topology is a **listen server** (owner direction §6b): the host runs authority and presentation in one process, so the module is a normal runtime module gated at runtime, not a `ClientOnly` one.
- Isolated tests: `GSTNParts.Core.*` links `GSTNCombatCore` only. Build check per commit: with `GSTNParts` disabled, then
  enabled; combat tests run in both. Docs: combat.md §3 gets a row per module, CLAUDE.md one line.

## 3. Data

### 3.1 Part set (`UGSTN_PartSetDefinition`, DataAsset, `GSTNPartsCore`)

One per entity type, referenced by the kit or minion definition. Fixed upper bound: **16 parts** per entity (`uint16` mask;
minions: 8, §5.5).

`FGSTN_PartDefinition`:

| Field | Meaning |
|---|---|
| `FName Name`, `FGameplayTagContainer Types` | Identity for designers/debug; `Part.Type.*` classification (§7) |
| `TArray<FName> Hurtboxes` | Names in the entity's `UGSTN_HurtboxSetDefinition` (M4 §3). The part set maps parts to hurtboxes; the hurtbox set is unchanged, so combat without parts is untouched. A hurtbox belongs to at most one part (validated); unmapped hurtboxes belong to the implicit "body" (no part: full hit goes to main with scale 1) |
| `TArray<FName> Bones` | Bones hidden/detached on break (presentation only; may be empty) |
| `int32 ParentIndex` | `INDEX_NONE` for roots. Breaking a part breaks its children in the same frame (hand under arm), deterministic order by index |
| `int32 Hardness` (0–15) | Armour tier. Integer tiers on purpose (Helldivers' armour levels): no float comparison in the rule that picks the band |
| `float PartCap` | Part damage percent at which it breaks. `<= 0` = unbreakable (armour plate that only reduces) |
| `float PartDamageScale` | Multiplier of damage that goes to the part (before the band) |
| `float MainTransfer` | Share of the (band-scaled) damage that goes to the main pool. Weak point: low hardness, small `PartCap`, `MainTransfer` > 1 (e.g. 2). Armoured limb: 0.2–0.5. Torso/body: 1 |
| `float BrokenMainTransfer` | `MainTransfer` once the part is broken (stump), default = `MainTransfer × 0.5`; hurtbox policy below |
| `EGSTN_BrokenHurtboxPolicy` | `Keep` (stump still hittable, uses `BrokenMainTransfer`), `Disable` (hits pass to the body), `Replace(FGSTN_HitShape)` (smaller stump shape) |
| `bool bWeakPoint` | Cue/UI/event flag (`Combat.Event.Part.WeakPointHit`, hit marker). No damage effect of its own: the numbers above already encode it, so it cannot double count |
| `FGSTN_PartReaction Reaction` | `StaggerThreshold` (the part's **hitstun condition**: negative = inherit the body's, then the rules' value), `MaxReaction` (the most severe reaction tier it can undergo), `JuggleFactor`, `HitstopScale`, `KnockbackScale` per part (§4.4) |
| `float LostMassFraction` | Fraction of the entity's base weight lost when broken (§4.3) |
| `TArray<FInstancedStruct> OnBreak` | Break effects, base struct `FGSTN_PartBreakEffect` (§3.3) |

Set-level fields: `EntityCap` is **not** here (it is the entity's `MaxHealth` under `PercentCapped`, §4.1, so the plain
combat model has it too); `UGSTN_PenetrationTable* Table` (null = rules default), `FGSTN_BakedCurve WeightCurve`,
`FGSTN_PartReaction BodyReaction` (entity-level `StaggerThreshold`/`MaxReaction`/`JuggleFactor`: used for hits on the body and as the parts' default), `FGSTN_GrabGate GrabGate` (§7),
`TSoftObjectPtr<UGSTN_PartPresentationSet> Presentation` (§6). `IsDataValid`: unique names, hurtbox in at most one part,
no parent cycle, `PartCap` and transfers finite, mask size ≤ 16, bones exist in the preview mesh (warning).

### 3.2 Hit-side data (`FGSTN_PartHitData`, an entry of `FGSTN_HitData::ExtensionData`)

```
FGSTN_PartHitData : FGSTN_HitExtension { uint8 Penetration; float PartDamageScale = 1;
                                         uint8 Flags (IgnoreHardness, MainOnly) }
```

There is **no stagger field** here: the stagger force is the hit's own (`FGSTN_HitData::StaggerForceMode`, `StaggerForce`,
`StaggerForceScale`, M4 §3), one source of truth that also works without parts. This extension only adds the penetration, whose
band scales the force (§3.4).

Authored per hitbox (the extension mechanism of §5b; a combat build without parts ignores it with a warning). Relationship
variants can override each field (`FGSTN_HitDataOverride` extension entry), so one hitbox may have a weaker `Penetration`
against allies. Heavy attacks, projectiles and thrown enemies (M10) just set a higher `Penetration`. No `FGSTN_PartHitData`
on a hit = `Penetration 0`, scales 1 (so unannotated attacks still work, mostly ricocheting off hard parts).

### 3.3 Break effects (base `FGSTN_PartBreakEffect`, polymorphic)

Two kinds, by rollback behaviour. **Derived** effects are functions of the broken-mask, recomputed every frame, so nothing
is replayed. **One-shot** effects are requests applied once at the break frame through the outcome applier, with the same
rollback guarantees as the damage GE.

| Struct | Kind | Effect |
|---|---|---|
| `FGSTN_PartBreakEffect_Tags { Granted; Removed }` | derived | Tags projected on the entity while the part is broken (`Combat.Part.*`, designer lock tags such as the designer-made `Input.Lock.Command.*` of a lost arm). `Removed` suppresses a tag of the *definition's* base tags (e.g. `Combat.Immunity.Grab`) while broken. Projection, not GE: no counted-tag bookkeeping |
| `FGSTN_PartBreakEffect_DisableHurtbox` | derived | Mask of hurtbox names the snapshot hides while broken (overrides the part's own policy) |
| `FGSTN_PartBreakEffect_DisableHitGroups { uint8 GroupMask }` | derived | The entity's own hitboxes in those `HitGroup`s stop being generated (§3.5): a lost arm cannot punch |
| `FGSTN_PartBreakEffect_Stats` | one-shot | Applies an infinite GE with `Combat.SetByCaller.Stat.*` magnitudes (walk speed, `Weight`, resistances) like M3 §5 weapon stats |
| `FGSTN_PartBreakEffect_MainDamage { float AddPercent; bool bKill }` | one-shot | Bonus main damage at break (head break = +30 %, or lethal) |
| `FGSTN_PartBreakEffect_Launch` | one-shot | Optional impulse on the entity (recoil of the break), through the knockback layered move like M4 §7 |
| `FGSTN_PartBreakEffect_Stagger { int32 Frames }` | one-shot | Forces at least this much hitstun (a lost leg buckles) |

Detached mesh and chunk are **not** effects: they are presentation data keyed by part index (§6.2). Gameplay never waits
for them.

### 3.4 Which part a hit lands on, and the penetration rule

Hit resolution (M4 §6 step 4) already finds overlapping hurtboxes. Parts need exactly one. **Rule (*Proposal*):** among
hurtboxes overlapped by the hitbox this frame and not intangible, pick the **highest `Priority`** (new `uint8` on
`FGSTN_Hurtbox`, default 0, weak points authored higher), ties by **lowest index**. Deterministic, order independent. The
attacker stores `HurtboxIndex` in the inbox entry; the victim never re-derives it. Lag compensation already rewinds the
transforms, so the chosen index is consistent with what the attacker saw.

**Band lookup** (`UGSTN_PenetrationTable`, data-driven): `Delta = Penetration − Hardness` (integers), bands sorted by
`MinDelta` descending, the first band with `Delta >= MinDelta` wins. `IgnoreHardness` forces the top band.

```
FGSTN_PenetrationBand { int32 MinDelta; float PartScale; float MainScale; float StaggerScale; float HitstopScale;
                        bool bRicochet; FGameplayTag CueTag; }
```

Default table (*Proposal*, tuned in data, not code). Revised 2026-10-22 (owner, 11:20–11:21: "la durezza regola la possibilità di
applicare il danno o no"; "la durezza sarebbe il valore di armatura di helldivers"): hardness acts on **damage only**, as Helldivers 2's
armour value against armour penetration, so the stagger and hitstop scales of the damaging bands are 1 (M4 §3 three dials), and an
attack below the armour value does not damage at all: it bounces.

| Delta (pen − hardness) | Meaning | PartScale | MainScale | StaggerScale | HitstopScale | bRicochet |
|---|---|---|---|---|---|---|
| ≥ +1 | above: full | 1.0 | 1.0 | 1.0 | 1.0 | no |
| 0 | equal: partial (Helldivers 2 deals about 65 %) | 0.65 | 0.65 | 1.0 | 1.0 | no |
| ≤ −1 | below: **ricochet** | 0.0 | 0.0 | 0.0 | 0.0 | yes (cue `GameplayCue.Combat.Part.Ricochet`, event, no hitstun, **no hitstop**) |

A game that wants a softer curve adds bands (e.g. a reduced −1 band) or lowers a band's `StaggerScale`/`HitstopScale`: these are
designer knobs, not the standard.

**Ricochet** (owner, 2026-10-22: "se si prova ad attaccare una parte troppo dura rispetto all'attacco, non si avrà un hitstop, ma
un effetto di ricochet del colpo"): a `bRicochet` band gives **no hitstop on either side**, no damage, no stagger, no hit-confirm; the
**attacker's weapon bounces back**: the attacker side returns `EGSTN_AttackerHitOutcome::Ricochet` through M4's attacker-outcome
seam (M4 §3b), and the attacker's combat ability ends its current timeline and plays a **recoil montage chosen per attack**
(`FGSTN_RicochetResponse` on the timeline, M4 §3b, with a per-weapon/kit default). The attacker's machine predicts it exactly:
penetration and hardness are static data and the broken-part mask is replicated (M4 §3, prediction from the victim's reaction state).
The victim gets only the spark cue and the `Combat.Event.Part.Ricochet` event. The band's `StaggerScale` multiplies the hit's stagger force before the part's threshold test (§4.4); it **defaults to 1**, because
hardness decides damage, not hitstun (owner, 2026-10-22, 11:20; M4 §3 three dials), and a designer may lower it so a hit that barely
penetrates staggers less. A table with fewer bands is valid; an empty band set is a validation error.

**Drill and demolition (owner, 2026-10-25).** What a drill can bore is a balance question, not a flag: the drill's tip hit carries
a penetration like any hit, so a part is drillable when penetration reaches its hardness (heavy armour refuses a weak drill, which
bounces off). Players are drillable; the latch is a hit-grab and becomes an M6 link (`Combat.Link.Type.Drill`). Details:
[spin-drill.md](spin-drill.md) §13b. Demolition force (combat-m4-hits.md §7c) is for props and level meshes; parts use it only when
they opt in.

**Hollow parts behind armour (owner, 2026-10-25 19:26).** A medium-hardness cavity inside a heavy-armour leg must be reached only
through its hole: a bulky swing that overlaps the leg from outside would otherwise also overlap the cavity inside it, pick it by
`Priority` and hit soft material through the armour. The cavity hurtbox gets an **opening** (a mouth shape plus the largest pass
radius that fits), and only a hitbox that overlaps the mouth and fits counts on it; everything else falls to the armour. Rule and
types: [combat-m4-hits.md](combat-m4-hits.md) §9c B (M4b checkpoint P18).

### 3.5 Entity's own hitboxes while parts are broken

The attacker side (M4 §6 step 1) asks the tick-extension point "before candidates" for a `HitGroupMask`; `GSTNPartsNP`
returns the union of `DisableHitGroups` of broken parts (from the fragment at end of F-1). Hitboxes of masked groups are not
generated: no hit, no hitstop, no confirm. Minions apply the same mask from the store. Moves meant to degrade gracefully
author their hitboxes in groups per limb.

### 3.6 Presentation data is separate

`UGSTN_PartPresentationSet` (Presentation module, soft reference from the set) is indexed by part index: bones to hide, stump
cap mesh and socket, chunk class and impulse, damage-material parameter names and part mask channel, shatter profile. Dedicated servers never load it; a listen-server host does.

## 4. Health model

### 4.1 Main pool: capped percent

The main pool is the existing **`DamagePercent`** of `UGSTN_CombatAttributeSet` (M4 §4), not a new attribute, so combat
without parts has the same thing. Under `DamageMode::PercentCapped` (M4 seam 4): `MaxHealth` is the **cap in percent points**
(e.g. 300 for a fighter, 120 for a grunt); `DamagePercent` is clamped at the cap by `UGSTN_DamageExecution`; reaching the cap
raises the existing `Combat.Event.KO` (destroyed). Derived, never stored: `Progress = DamagePercent / MaxHealth` (0–1) and
`HealthFraction = 1 − Progress`. `Health` is unused in this mode. M8's `KnockbackModel_HealthFraction` reads the same fraction,
so minions and fighters agree on "how wounded".

**Which knockback model** (owner, 2026-10-22, 11:20: "il peso determina la forza del knockback"; the three dials of M4 §3):
the standard rules use M4's `UGSTN_KnockbackModel_WeightDriven`, where the capped percent makes knockback stronger **only
through the effective weight** of §4.3. `UGSTN_KnockbackModel_CappedPercent` below is the platform-fighter option, with a flat
`WeightCurve` (lost mass still applies), so damage is never counted twice.

`UGSTN_KnockbackModel_CappedPercent` (in `GSTNCombat`, a variant of `PercentScaling`): the percent term is
`DamagePercent` as is (platform-fighter feel: the cap only bounds it), every constant stays a property, and the effective
weight, juggle factor and the modifiers below are applied exactly once, in this order: `Weight × WeightScale`, knockback
magnitude, `× KnockbackScale`, the stagger test and the reaction tier (M4 §3), hitstun from the knockback `× HitstunScale`, hitstop `× HitstopScale`.

### 4.2 Part damage and main damage per hit

Given hit damage `D` (already multiplied by the attacker's outgoing and the victim's incoming multipliers), the part `p`,
the band `b` and `FGSTN_PartHitData h`:

```
PartDelta = D × b.PartScale × h.PartDamageScale × p.PartDamageScale
MainDelta = D × b.MainScale × p.MainTransfer              (BrokenMainTransfer if p is already broken)
Part damage state += PartDelta     (quantised, §5.1);  p breaks when it reaches p.PartCap
DamagePercent    += MainDelta  (+ OnBreak MainDamage of parts broken by this hit, once each)
```

- A hit on the body (no part) uses `b = top band`, scales 1: `MainDelta = D`. With no part set the step does nothing: M4's
  `ApplyDamage` adds `D`, as without the plugin.
- Overflow: damage beyond `PartCap` on the breaking hit is **not** carried to the main pool (bonus is the explicit
  `MainDamage` effect). Hits on an already broken part follow its `EGSTN_BrokenHurtboxPolicy`.
- The main damage is passed to M4's `ApplyDamage` as the amount: the step writes `Ctx.Outcome.Damage = MainDelta`, so the
  existing damage GE and `SetByCaller` path is unchanged.
- Cascade: parts whose parent broke break in the same hit, without their own `MainDamage` unless they declare it.
- Order inside one inbox drain: entries are resolved in M4's (AttackerId, hitbox order); each entry sees the part state
  produced by the previous one, so two hits in the same frame behave identically on every machine.

### 4.3 Weight from remaining health

`EffectiveWeight = BaseWeight × WeightCurve(Progress) × Π(1 − p.LostMassFraction for broken p)`, clamped to
`MinWeightScale × BaseWeight` (default 0.35). `WeightCurve` is authored as a `UCurveFloat` and **baked at load into a
64-entry `uint16` table** (`FGSTN_BakedCurve`, integer interpolation), so every machine reads the identical number and the
float curve evaluator is not on the simulation path. Default curve: 1.0 at 0 % falling to 0.6 at the cap (a battered enemy
flies further). `BaseWeight` is the existing movement-set `Weight` attribute (M3 §5); effective weight is **derived each
resolution from the snapshot extension**, never written back, so rollback has nothing to undo. M7 carry load and M10 thrown
weight may call `UGSTN_PartMath::EffectiveWeight` with the same inputs. `PartResolve` writes the result as
`Reaction.WeightScale = EffectiveWeight / BaseWeight`.

### 4.4 Stagger force, hitstun condition and reactions: what and where decide hitstun and hitstop

Owner direction ([game-design-direction.md](game-design-direction.md) §2b): whether an enemy or boss enters hitstun **depends on the
part that was hit**; each part has its own **hitstun condition**, generally that the attack's **stagger force** reaches a threshold set
on the part. **M4 §3 owns the force** (`FGSTN_HitData`: `Derived` from the hit's damage and nominal knockback, or the designer's
`Override`, times `StaggerForceScale`) **and the test**; parts supply the "where", per part and for the body (`BodyReaction`), all
data-driven:

- **`StaggerThreshold`** (*Proposal* for the placement): the hit staggers when `StaggerForce × b.StaggerScale ≥ threshold`. `PartResolve`
  writes `Reaction.StaggerThreshold` (the part's, else the body's, else left unset so the rules'
  `UGSTN_CombatRules::StaggerThreshold` applies as the **global default**) and `Reaction.StaggerScale = b.StaggerScale` (the penetration
  band, §3.4); a ricochet band skips the whole reaction (no damage, no hitstop, no flinch). Below the threshold M4's Armor semantics apply: damage and hitstop, no hitstun, no launch
  or push, flinch presentation only. This **replaces** the old per-part stagger factor and the single global threshold: with parts on,
  the part's own value is the condition. Typical data: a **weak point a low threshold** (a light hit staggers it, the reward for
  aiming), the torso a middle value, **armoured limbs and boss plates a high one** (light hits scratch the part's material and are shrugged off, only
  heavy attacks stagger it).
- **`MaxReaction`** (the `min` of the part's and the body's): the most severe reaction tier the part can undergo, `StaggerInPlace`,
  `PushBack` or `Launch` (default). A heavy limb or a boss never flies: it is capped at `PushBack` or `StaggerInPlace`.
- **Weight feeds the tier.** The reaction tier is chosen in M4 §3 by comparing the knockback `K`, computed with the effective weight of
  §4.3 (remaining health and lost mass of broken parts), with the tier thresholds: a battered or mutilated enemy has a lower weight, so the
  same hit moves it from stagger in place to push back to launch, with no extra rule here.
- **`JuggleFactor`**: the weight-for-knockback multiplier of the owner's message: `WeightScale ×= p.Reaction.JuggleFactor ×
  Body.JuggleFactor`; a head hit may launch more than a leg hit, a heavy limb less. When the victim is already in `Tumble`
  the rules' `AirborneJuggleScale` (default 1) is applied too, the knob that makes long juggles possible or not (*Reading*).
- **`HitstopScale`**: `b.HitstopScale × p.Reaction.HitstopScale`, applied to the **automatic** hitstop only (a designer's `Override` is
  final, M4 §3); a hit on a weak point freezes longer (more impact), a ricochet does not freeze at all (the attacker recoils instead).
- **`KnockbackScale`**: `p.Reaction.KnockbackScale`, e.g. 0.3 for a pauldron, 1 for the torso.

"What hits" enters through `FGSTN_HitData` (stagger force, `HitstunMultiplier`, hitstop mode and multipliers) and `Penetration`, "where"
through the part and the band. Steps stay const and pure: they write modifiers into the context, and the knockback model and
`Hitstop` step read them (M4 seam 1).

**Attacker hitstop and prediction.** The attacker predicts its own hitstop against a possibly stale puppet (M4 §6, M8 §3).
It must not depend on the victim's *dynamic* part state. Rule: attacker hitstop uses only static data: the hit's authored damage and
nominal knockback (M4 §3 `Auto`) or its `Override`, then `HitstopScale` of the
band (from `Penetration` and the chosen part's `Hardness`) and the part's `Reaction.HitstopScale`; never broken mask,
percent or weight. A test asserts it (as M8 does for the base model). The only disagreement left is a part that broke
between prediction and resolution.

### 4.5 Players versus enemies

| Aspect | Player fighter | Enemy fighter | Minion |
|---|---|---|---|
| Part set source | Kit extension | Kit extension | Definition extension (bridge) |
| Cap (`MaxHealth`) | High (long fights, comeback) | Per enemy, tuned for PvE | Small, 5-bit health on the wire |
| Part set | **One part = the whole body** (owner, 2026-10-22): its health is the player's health | Several parts | Few parts, often one |
| Typical effect of a break | **The break is death** (no limb loss for players) | Gameplay: grabbable, disarmed, slowed | Cosmetic plus grabbable |
| `StaggerThreshold` (hitstun condition) | Low (most hits stagger: fighter feel; *Proposal*) | Per enemy and per part: weak points low, armoured parts and bosses high | Per type and per part |
| `MaxReaction` | `Launch` | Per enemy and part (heavy: `PushBack` or `StaggerInPlace`) | Per type; the tier is carried by `ActionState` (M8 §8.2) |
| Weak points | Rare (e.g. head, hard to hit by design) | Core of PvE | Few, often one |
| Death | KO event then shatter | KO then shatter | Dying state then shatter (tiered, §6.4) |

Players: the single part's cap is the KO condition, so "part broken" and "player dead" are the same event for a player
(`Combat.Event.Part.AllBroken` followed by KO). Enemies: `BrokenMask` only changes back through an explicit restore request (owner 2026-10-22 18:22: no self-regeneration, but e.g. a healer enemy can restore parts; plan P7b) or an entity reset (*Proposal*:
no healing of broken parts in the first version), which also keeps the rollback story simple.

## 5. Simulation and network

### 5.1 Sync state fragment (`FGSTN_PartsFragment`, `GSTNPartsNP`)

A `FGSTN_SyncFragment` (§5b) registered with a fixed type id from the central registry file. Entities without a part set
carry **no fragment: zero bytes and no compare cost**.

```
FGSTN_PartsFragment { uint16 BrokenMask; uint8 PartDamage[N]; uint8 BreakSerial (4 bits used); }
```

`PartDamage[i]` = part damage / `PartCap` quantised to 8 bits (0 = pristine, 255 = at the cap). Rule: stored =
`min(255, ceil(Percent/Cap × 255))` after every hit, so a nonzero hit always moves the byte and every machine reads the
same integer; the next hit starts from the dequantised value (the rounding is part of the rule, not an error to hide).
`BreakSerial` increments per break event so presentation and events can be deduplicated across rollback.

| Case | Bits on the wire (custom `NetSerialize`) | Bytes |
|---|---|---|
| No damage yet | 1 flag | ~0 |
| Fighter, N = 6 | 1 + 6 + 48 + 4 | ~7 |
| Fighter, N = 8 | 1 + 8 + 64 + 4 | ~10 |
| Maximum, N = 16 | 1 + 16 + 128 + 4 | ~19 |

`N` is the part count of the set (not sent: both sides load the same asset; a mismatch is refused at connect like a plugin-set
mismatch, §5b). `ShouldReconcile`: exact integer equality (no tolerance: nothing here is a float). `Interpolate`: take the
later sample (integers; presentation reads the mask, not a blend). `ToString` prints mask and per-part percent for the
harness. The main pool is not in the fragment: it lives in the attribute, where M3/M4 already put it. The fragment is cheap
enough that the 2-fragment test of §5b (fragment round trip with zero, one and two fragments) is extended with this one.

### 5.2 Snapshot and victim view

A snapshot contributor (`FGSTN_SnapshotExtension`, §5b) adds per entity a POD `FGSTN_PartsSnapshotExt { uint16
BrokenMask; uint8 PartDamage[16]; uint8 PartSetId; }` read from the fragment **at end of F-1** (or from the minion store, §5.5),
plus the derived `GrabLocked` bit. It is counted in the lag-compensation history memory (≤ 20 B per entity per frame).
`FGSTN_HitVictimView` (M8 §3) carries the pointer to it; the step reads parts only through the view, so the fighter and
minion paths share the code.

### 5.3 Pipeline step and order

`UGSTN_HitStep_PartResolve` (`GSTNPartsCore`, const, pure, no component access). Position in the resolution pipeline
(M6 §9 is normative for the other steps):

1. `TeamFilter` 2. `HurtState` 3. `GrabImmunity` 4. `LinkRequest` 5. `Parry` 6. `Shield` (blocked, parried, grabbed or
   intangible hits have already stopped: **a blocked hit damages no part**)
7. **`PartResolve`**: locate part from `HurtboxIndex`, band, `PartDelta`, `MainDelta`, break test (with cascade), the
   reaction modifiers (weight scale, stagger threshold and scale, `MaxReaction`, juggle, hitstop and knockback scales; §4.3, §4.4),
   `bLethal` from cap and `MainDamage`. Writes `Ctx.Outcome.Damage`, `Ctx.Reaction`, an
   `FGSTN_PartHitOutcome` in `Ctx.OutcomeExtensions` (part index, new quantised damage, newly broken mask, events).
8. `ComputeKnockback` (reads `Ctx.Reaction`), `ApplyDamage` (adds `Outcome.Damage`), `Hitstop` (reads `Ctx.Reaction`),
   `MeterGain`
9. `LinkBreakOnHit`, `Tumble/Tech` (M6)

`IsDataValid` on the rules asset reports `PartResolve` placed before `Parry`/`Shield` or after `ComputeKnockback`.
A step from a missing plugin is skipped with a warning (§5b).

**Outcome applier** (M4 seam 3, `GSTNPartsNP`): when the combat tick applies the outcome at F+1 it (a) writes the new
`PartDamage`/`BrokenMask`/`BreakSerial` into the fragment, (b) requests the one-shot break effects (stats GE, extra main
damage already folded in step 7, launch, stagger), (c) pushes events to the outbox. All of it is state at frame F+1, so a
rollback of the victim restores it with the combat state and the inbox rule of M4 §7 (entries with `Frame ≥` restored are
re-delivered by the attackers) re-creates the break once.

**Tick extension** (`UGSTN_PartsTickExtension`, ordered by an explicit `Priority`): (1) at tag projection, projects derived
tags from the mask (§7); (2) before candidates, supplies the `HitGroupMask` (§3.5); (3) the snapshot builder asks it for the
hurtbox overrides of broken parts (§3.3 policies). The ability system ticks before Mover, so tags projected in a frame are
visible to Mover the same frame (CLAUDE.md rule 5); nothing here touches Mover directly.

### 5.4 Events and rollback safety

Events go through the outbox of combat.md §5.6 / M2 §7b with the envelope of the hit (command, move, hit type,
relationship) plus `FGSTN_PartEventPayload { uint8 PartIndex; uint8 BreakSerial; uint8 Band; }`:

| Event | When | Consumers |
|---|---|---|
| `Combat.Event.Part.Damaged` | Part took damage (not a ricochet) | UI, hit marker |
| `Combat.Event.Part.WeakPointHit` | `bWeakPoint` part hit and not a ricochet | UI marker, audio |
| `Combat.Event.Part.Ricochet` | `bRicochet` band | Spark cue, audio (the attacker's recoil is its own ability, not this event) |
| `Combat.Event.Part.Broken` | Part (or cascaded child) broke | Presentation chunk, UI, objectives |
| `Combat.Event.Part.AllBroken` | Optional: every part with `PartCap > 0` is broken | Design hooks |

Death uses the existing `Combat.Event.KO`; the shatter reads the last `HitReceived` context (hit location, direction,
launch). Rollback rules, all the existing ones plus: (1) no `IsServer()`/resimulation branches in any part code; (2) every
value read by `PartResolve` comes from the snapshot or the hit; (3) state is written only in the outcome applier at the
victim's tick; (4) events and presentation are deduplicated by `(EntityRef, PartIndex, BreakSerial)`; (5) the mask is
monotonic, so a late correction can add a break but a rollback to before it removes it only for predicted frames that never
confirmed (§6.5).

### 5.5b Listen server (owner direction §6b)

The reference topology is a host who is also a player; budgets are for **the host's machine, which renders too**.

- **Host advantage.** The host's own hits resolve with zero latency, against zero-lag victims: its part choice, break and
  chunk spawn happen the same frame, while clients see them one round trip later (the client-attacker case of M4 §6 and §13
  is the norm for everyone else). Parts add no new advantage beyond that of M4, but the effects are visible: a host breaks a
  limb instantly. Mitigations (*Proposal*, owner decides): none by default; optional `bHostHitFeelParity` delays only the
  host's *presentation* of irreversible effects by the mean client delay, never the simulation.
- **Host CPU/GPU.** Simulation (§5.5 hit cost, ≤ 2 µs) and presentation (§6.4) share one frame. The §6.4 budget is therefore
  a **host-tier** budget: pool 48 chunks, 600 shards, 2 GC are the ceiling on the host, and `UGSTN_DebrisPoolSubsystem`
  scales by scalability level (and by measured frame time, dropping to Niagara bursts only when the host exceeds its frame
  budget). Frame-time impact of presentation is measured on the host configuration, not on a client.
- **Host upload.** The limiting resource. Fragment cost is multiplied by connected clients (§5.1 bytes × clients, only on
  change); minions pay the +1 B ext byte per item update per client (§5.5). Both stay inside M8's per-client target (≤ 6 KB/s at N = 40, M8 §8.3) and fit the
  M8 budget table; the harness reports host bytes/s with and without parts.
- **Dedicated server** stays supported (same simulation code and fragment; nothing forks on topology).

### 5.5 Minion path (`GSTNPartsMinion`, compact)

Minions are not NP actors (M8). The bridge plugin gives the minion store optional columns and the same pure code:

- **Definition**: `FGSTN_PartsMinionExtension { PartSet }` on `UGSTN_MinionDefinition`; set limited to **8 parts**, no
  `OnBreak` stat GEs (a `FGSTN_PartBreakEffect_Stats` is converted to the minion's stats struct fields or rejected by the
  validator), only derived effects, `MainDamage`, `Launch`, `Stagger`.
- **Server store** (struct-of-arrays, only for slots of types with a part set): `uint8 BrokenMask`, `uint8 PartDamage[8]`
  (9 B per such minion; 40 minions = 360 B). The minion hit flow of M8 §9.2 builds the same `FGSTN_PartsSnapshotExt` from
  it, runs the **same** `UGSTN_HitStep_PartResolve`, and the batch applies the outcome to the store directly (no GE, no
  fragment) and mutates `Health`, hitstun and launch as M8 does for any hit.
- **Wire** (requires an M8 change, listed in §9): `FGSTN_MinionNetItem` gets an optional extension byte `ExtByte`
  guarded by a type-level `bHasExt` (0 bits for types without parts) owned by the bridge: `BrokenMask`. Dirty only when
  the mask changes. Cost: +8 bits on items of part-bearing types (about +1 B on the 20 B planning-size update of M8 §8.3, +5 %); items do not carry
  per-part damage. Clients drive **the damage material from the existing 5-bit `Health`** and **limbs from the mask**; weak-point flashes use
  the existing `HitCounter` plus the local knowledge of where the attacker hit.
- **Reaction**: the same step writes the part's `StaggerThreshold` and `MaxReaction`, so a minion's hitstun condition and reaction tier
  (`StaggerInPlace`, `PushBack`, `Launch`) follow M4 §3 per part; the tier is carried by the minion's `ActionState` (`Hitstun`,
  `PushedBack`, `Launched`), with no extra wire bits (M8 §8.2).
- **Cost per hit** (server): one table lookup, a few integer operations, no allocation, no GE: budgeted at ≤ 2 µs per hit,
  measured in the M8 bench (§8). `GSTNMinionRuntime` needs no knowledge of parts.
- Minions stay GAS-free: part events are emitted through M8's outbox equivalent (the `Combat.Event.*` envelope is shared).
- Promotion (M8 §10): the store's `BrokenMask`/`PartDamage` copy into the promoted fighter's fragment, and back on demotion.

## 6. Presentation (never in the simulation; every machine that renders, including the listen-server host)

All of it lives in `GSTNPartsPresentation`, driven from `FinalizeFrame`/events, with **no feedback into gameplay**: no
collision with pawns, no gameplay hits, no input, nothing the simulation reads.

### 6.0 Crystal body and per-player crystal colour (owner input, 2026-10-24)

Characters are **human outside, crystal inside**, and **the inner crystal's colour differs per player**. The colour shows
wherever the crystal shows: the exposed-crystal blend of §6.1, flakes, stump caps and chunks (§6.2), shatter pieces (§6.3), and
the optional light styles of the depth marker (god rays, [depth-marker.md](depth-marker.md) §4.1).

- **Source**: a palette data asset `UGSTN_CrystalPaletteDefinition` (per colour slot: crystal colour, emissive strength, a
  pattern id for colour-blind players) and a **colour slot** per player, which is the player's number in the match.
  **Owner decision (CR.1, 2026-10-24): player 1 is red, player 2 blue, player 3 yellow, player 4 green**; further slots are
  designer entries in the palette. **Player numbers follow the alphabetical order of the players' names**, so a group of
  friends who always play together always gets the same colours.
- **Assigning the numbers** (*Proposal* for the details):
  - The **host** sorts the players present when the match starts by display name, case-insensitive and culture-invariant,
    ties broken by the unique net id. Only the host sorts, so every machine agrees whatever its language.
  - The result is a replicated byte on each player state: ordinary property replication, never NP state, never read by the
    simulation, so it has no rollback rules.
  - A player who joins later takes the **first free number**; a player who leaves frees theirs. Numbers are never reshuffled
    during a match, so nobody's colour changes mid-game.
  - Red and green are the pair colour-blind players confuse most, so the palette's **pattern id** per slot stays (and the depth
    marker's glyph, [depth-marker.md](depth-marker.md) §4.4).
- **Coupling**: `UGSTN_PartsPresentationComponent` asks a **tint source** (an interface the project or the experience
  implements) for the colour; without one it falls back to the palette by player index, so the plugin still works alone. The
  depth marker reads the same colour through its own adapter (`GetTint`, [depth-marker.md](depth-marker.md) §4.4): neither plugin
  depends on the other.
- **Delivery**: one material for every character. The colour is set **once per character**, as a vector parameter on the dynamic
  material instance that §6.1 already writes (or as custom primitive data); never one material per player. Pooled pieces
  (flakes, chunks, shards) take the dying entity's colour when they spawn: Niagara particle colour or custom primitive data on
  the chunk actor.
- **Enemies and minions**: colour per enemy class (designer data on the presentation set); minions get it as per-instance
  custom data from their type, with no wire bits.
- **Damage glow** (*Proposal*, question CR.3): the crystal's emissive may grow with the §6.1 damage parameter, so a badly
  damaged character leaks light through the cracks; the same value can drive the "rays from the cracks" variant of the god rays.
- All of it is replaceable content (palette, material, Niagara) under §3.9 of [modularity-and-tags.md](modularity-and-tags.md).
  Questions CR.1–CR.5 are in [open-questions-m4-m10.md](open-questions-m4-m10.md).

### 6.1 Damage inside a part: material only (diegetic health)

Owner direction: inside a part, damage is **a material, a shader**, the diegetic way to show that part's health; nothing is
spawned. `UGSTN_PartsPresentationComponent` writes, per part, a **damage parameter** (`0..1` = part damage / part cap, or
`Progress` for a single-part player) into the mesh's dynamic material instances: the skin's crack and exposed-crystal blend,
masked per part by a vertex-colour or UV region (*Proposal*: one `PartIndex` channel in the mesh, so one material serves all
parts; for per-part materials, one parameter per material slot). The parameter is **driven directly from replicated state**
every frame (no thresholds, no events), smoothed for display; it follows rollbacks and corrections naturally because it is
not an effect but a view of the state. Cost: one scalar parameter write per part when the value changes. A small hit spark
(`GameplayCue.Combat.Part.Hit`) is the per-hit cosmetic.
**Cosmetic flakes** (owner, 2026-10-22: "i pezzi del material che si staccano di una parte sono solo estetici"): as the damage
parameter crosses designer thresholds, small skin/crystal flakes may fly off the material (pooled Niagara or mesh particles, seeded
by `(EntityRef, PartIndex, ThresholdIndex)` so every machine shows the same ones). They are **pure presentation**: no collision with
pawns, no gameplay effect, never read by the simulation, monotonic (a rollback does not take them back). Players are a single part,
so for them flakes and material are only aesthetic too; a player loses no limbs.

### 6.2 Limb detachment: options and recommendation

| Option | How | For | Against |
|---|---|---|---|
| **A. Hide bone + pooled chunk** | `HideBoneByName` (physics bodies off) on the entity mesh, a **stump cap** crystal mesh attached to the parent socket, a pooled **chunk actor** (static/skeletal copy of the limb, simple Chaos rigid body, scripted impulse) | Cheap, one asset per limb, works with any skeletal mesh, predictable memory and pooling, identical on 40+ entities | The cut is always at the bone; skinned verts at the joint collapse, hidden by the stump cap |
| B. Modular mesh swap | Master pose component per limb, swap to a stump variant | Best silhouette control, per-limb LODs | Many mesh components per character, skin and material duplication, authoring cost scales with entity count; costly for minions |
| C. Geometry Collection per limb | Fracture the limb mesh, activate Chaos on break | Beautiful break, crystal looks right | Per-entity GC component is heavy, authoring and cooking cost, not poolable cheaply at 40+, needs Chaos solver budget |

**Recommendation (*Proposal*): A** for limbs of players, fighters and minions; **C** only for hero crystals (boss weak-point
reveals, scripted moments) where one or two are alive. Triggered by the `Combat.Event.Part.Broken` event (confirmed state,
§6.5), keyed `(EntityRef, PartIndex, BreakSerial)`; the **hidden-bone set is idempotent**: every frame the visible mesh
applies `HideBone` for the bits of the confirmed `BrokenMask`, so a late join or a relevancy re-entry shows the right body
without replaying events. The chunk is spawned once per key, flies on `Launch` data of the presentation set plus the hit
direction.

### 6.3 Death shatter

| Option | For | Against |
|---|---|---|
| Chaos Geometry Collection of the whole body | Physically convincing | One GC per dying entity: unaffordable for crowds |
| **Pre-fractured pieces** (a fixed set of crystal shard meshes, per entity class) spawned as pooled debris/Niagara mesh particles at the bone locations | Fixed cost, same code for every entity, looks "crystal" if the pieces are authored well | Pieces do not match the body exactly; mitigated by hiding the body in the same frame behind a flash |

**Recommendation (*Proposal*): pre-fractured pieces for everyone; Geometry Collection only for players and bosses**
(≤ 2 active at once, from the budget below). The shatter reads the KO event and the last hit's direction and impulse; the
skin mesh disappears in the same frame (replaced by pieces carrying the skin material on the outer faces).

### 6.4 Budget and pooling (40+ enemies)

Rules in the rules extension (`FGSTN_PartsPresentationBudget`), enforced by `UGSTN_DebrisPoolSubsystem` (client):

| Item | Budget (*Proposal*, to be measured) |
|---|---|
| Physical chunks alive (limbs and large shards), all entities | 48 pooled actors, 4 s lifetime then shrink-fade |
| Shatter shards via Niagara mesh particles | 600 live particles total, CPU sim, simple ground collision, no pawn collision |
| Geometry Collections alive | 2 (players/bosses) |
| Damage-material parameter writes | only on change, no cap needed |
| Cost control | By distance and tier (M8 §7): beyond tier T1 only the Niagara burst, no chunk; beyond T2 nothing; priority to chunks near the camera and to local-player hits; sleeping bodies stop simulating after 1.5 s |
| Memory | Pieces and chunks are shared assets (instanced), loaded by the presentation set on demand, never on a dedicated server (the listen-server host loads them like any client) |

When the budget is exhausted the **oldest** chunk is recycled; a skipped chunk never changes gameplay (it is cosmetic). The
M8 bench adds a "40 deaths in 2 seconds" case (§8) with a frame-time ceiling.

### 6.5 Rollback and what may be shown when

Reversible cosmetics (hit flash, the damage material) may use predicted data; **irreversible** ones (limb detach, shatter) fire only from
confirmed sources: the outbox event, or a mask bit that arrived in replicated/corrected state. For interpolated victims on
the attacker's client that is the replicated fragment (M4 §9); for the local player as victim it is the server correction
(M4 §13: hits on the local player arrive as corrections). If a future forward-prediction milestone predicts victim hits
locally, irreversible presentation stays gated on confirmation; the component exposes `bRequireConfirmed` (default true).

## 7. Gameplay tags

Owner `GSTNParts` unless stated. Native in `GSTNPartsCore` (`GSTN_PartsTags.h`, `namespace GSTN_PartsTags`); ini only for
designer vocabulary (`Plugins/GSTNParts/Config/Tags/GSTNParts.ini`). Rules of modularity §4 apply.

| Tag | Source | Meaning | Written by | Read by |
|---|---|---|---|---|
| `Combat.Part.Broken` | native, **projected** | At least one part is broken | tick extension (from mask) | UI, AI, rules |
| `Combat.Part.GrabLocked` | native, **projected** | Grab gate unmet (see below) | tick extension | `FGSTN_GrabRules::Requirements` (M6), data only |
| `Combat.Part.AllBroken` | native, projected | All breakable parts broken | tick extension | design hooks |
| `Combat.Part.Effect.*` | native | Leaves granted by `FGSTN_PartBreakEffect_Tags` of the shipped content, e.g. `Combat.Part.Effect.ArmLost` | break effects | kits, abilities, AI |
| `Combat.Entity.Destructible` | native | The entity has a part set (marker in `FGSTN_HitVictimView::Tags`) | fragment registration | steps, UI |
| `Combat.Event.Part.Damaged`, `.WeakPointHit`, `.Ricochet`, `.Broken`, `.AllBroken` | native | §5.4 | pipeline outbox | presentation, UI, objectives |
| `Message.Combat.PartBroken` | native | Message-router channel for UI | presentation or UI bridge | HUD |
| `Part.Type.Head`, `.Torso`, `.Arm`, `.Leg`, `.Weapon`, `.Armour`, `.Crystal` | **ini** | Part classification on `FGSTN_PartDefinition::Types`; unrelated to `Combat.Hurtbox.*` (which classifies hurtboxes, M5 §tags) | designers | rules, cues, AI |
| `GameplayCue.Combat.Part.Hit`, `.Break`, `.Ricochet`, `.Shatter` | ini | Cue tags used by presentation sets | designers | presentation |

`Combat.State.*` is **never** declared or granted by parts: stagger is the existing `Combat.State.Hitstun`, hitstop the
existing `Combat.State.Hitstop`. Parts add no `Combat.Mover.*` and no `Combat.Lock.*`.

**Grabbability** (owner §1 and §3). A tag named `Combat.Capability.Grabbable` is **not** used: `Combat.Capability.*` is the
adapter-capability root of modularity §4.2 (granted by adapters), and `Combat.Link.*` belongs to `GSTNLink`. *Proposal*:
the large entity is ungrabbable by a **negative** projected tag, `Combat.Part.GrabLocked`, present while
`FGSTN_GrabGate { int32 MinBrokenParts; TArray<int32> RequiredParts; bool bAnyOf }` is unmet (empty gate = never locked). The
designer adds `Combat.Part.GrabLocked` to the ignore list of `FGSTN_GrabRules::Requirements` (M6 §3) in the rules asset: **no
change to M6 code and no foreign tag declaration**. A boss keeps `Combat.Immunity.Grab` (M6 §8) and never has a gate. The
same projected tag drives the minion path (the store mask feeds the same function).

## 8. Tests, acceptance, risks, extension

### 8.1 Automation (`GSTNParts.Core.*`, no world; `GSTNParts.Runtime.*` with the editor world)

- **Band lookup**: every `Delta` from −8 to +8 against hand-computed rows; `IgnoreHardness`; table with one band; empty
  table rejected.
- **`UGSTN_PartMath::ApplyHit` golden**: sequence of hits → expected part bytes, broken mask, `MainDelta`, cascade, weak
  point, ricochet, overflow policy, broken-part policy (`Keep`, `Disable`, `Replace`); a nonzero hit always changes the byte.
- **Determinism**: the same inputs produce identical outputs across 1,000 randomised sequences and two compile configs; the
  weight curve table equals hand values; `EffectiveWeight` never below the clamp.
- **Quantisation and fragment**: `NetSerialize` round trip (0, 1, 2 fragments, N = 1, 8, 16, empty and full), `ShouldReconcile`
  exactness, byte sizes match §5.1.
- **Step and context**: `PartResolve` through fake contexts of a fighter and of a minion view gives the same outputs;
  order validator (before `Parry`, after `ComputeKnockback`); blocked hit damages no part; attacker hitstop independent of
  the victim's dynamic part state; stagger condition per part (force equal to the threshold staggers, one unit below does not; part,
  body and rules fallback; a weak point staggers on a hit an armoured part shrugs off; a ricochet band never staggers and never freezes); `MaxReaction`
  caps the tier; a lower effective weight never gives a milder tier.
- **Events/dedupe**: key `(EntityRef, PartIndex, BreakSerial)`; two entries in one drain are order independent.
- **Presentation (Runtime)**: damage material follows the replicated value (also after a rollback), idempotent hidden-bone set from the mask, pool never exceeds the
  budget, recycle order.
- **Validators**: duplicate hurtbox, parent cycle, mask > 16 (> 8 for minions), missing bone.

### 8.2 Harness scenarios (`Tools/GASNPPTest/parts_scenarios.py`, imports `gasnpp_test`; 60/15 ms, with and without 2 % loss)

`scenario_part_break` (c1 hits a limb of c2 until it breaks: mask, bytes and percent equal on all machines, `Part.Broken`
once), `scenario_part_weakpoint` (weak point takes more main damage than the torso for the same hit), `scenario_part_ricochet` (attacker recoils with the attack's montage, no hitstop on either side, predicted on the attacker's client without correction)
(hard part, low penetration: no part damage, no hitstun, attacker hitstop present), `scenario_part_stagger` (c1 hits an armoured part
with a light and a heavy attack, then the weak point with the light one: light on armour = damage, no hitstun; heavy on armour = hitstun and,
with `MaxReaction = PushBack`, a grounded push back; light on the weak point = hitstun; equal on all machines), `scenario_part_rollback` (forced reconcile
around the break: no duplicate event or chunk), `scenario_part_grabbable` (grab refused while `GrabLocked`; after the
required parts break, accepted), `scenario_part_cap` (main percent reaches the cap: KO exactly once), `scenario_part_hitgroups`
(lost arm stops its own hitbox), `scenario_minion_parts` (M8 bench, 40 minions with part sets: wire bytes within +5 %,
per-hit server cost), `scenario_shatter_budget` (40 deaths in 2 s on a client **and on the listen-server host**: frame time and pool counters within §6.4). Every scenario runs in the **listen-server configuration (host + 2 clients)** as well as dedicated + 2 clients (owner direction §6b).
Clean = state snapshots equal and no reconciles beyond the expected one per hit on the victim's client (M4 §10).

### 8.3 Acceptance criteria

1. M1–M4 (and M6/M8 when present) tests and scenarios clean with `GSTNParts` **disabled and enabled**.
2. Scenarios of §8.2 clean; `FGSTN_PartsFragment` stays within the §5.1 byte table; an entity without parts costs 0 bytes.
3. Penetration rules change only by editing the `UGSTN_PenetrationTable` asset (no code change); a new part set works with
   no C++. Which hits stagger a part (its `StaggerThreshold`) and its `MaxReaction` are data too.
4. Grab gating works with a rules-asset edit only (§7); no M6 code change.
5. Minion path adds ≤ 5 % to an item update and ≤ 2 µs per hit on the authority; 40+ entities presentation inside the §6.4 budget **on the host machine** (it also renders), host upload within M8's budget.
6. Death shatter, the damage material and detached limbs appear on players and enemies, and never alter simulation (a client with presentation
   disabled produces identical state).
7. combat.md updated (§3 modules, §12 milestone row M4b, §17 spikes, §15 byte budgets).

### 8.4 Spikes first (record in combat.md §17)

1. **Overlap to part**: the `Priority`/`HurtboxIndex` rule on moving, overlapping hurtboxes under lag compensation: the
   attacker's and the server's choice must agree (measure disagreement rate).
2. **Presentation cost**: 40 simultaneous shatters with option §6.3 pieces versus GC for hero; limb chunk pool at 48.

### 8.5 Risks

- **Combat seams slip out of M4**: without §1 seams M4b edits the core. Mitigation: seams are acceptance items of M4.
- **Part choice disagreement** attacker/server: cosmetic if it is only the hurtbox flash, a rollback if it changes damage.
  The inbox carries `HurtboxIndex` to avoid re-derivation; spike 1.
- **Balance explosion** (hardness, transfer, curves, stagger thresholds, reaction tiers): validator prints "hits to break/kill" and "hits to
  stagger" per part set.
- **Minion wire growth** if many minion types use parts: cap 8 parts, ext byte only for those types, measured in the bench.
- **Host-only costs** (listen server): hit resolution, minions and presentation share the host's frame, and host upload limits clients; host advantage on instant breaks (§5.5b).
- **Chunk and shard budget** on low-end clients and hosts: tier rules of §6.4, quality scalability settings.

### 8.6 How to extend

- **New part type**: add a `Part.Type.*` leaf to the ini; author a `FGSTN_PartDefinition` row (hurtboxes, hardness, caps,
  transfer, reaction) in a `UGSTN_PartSetDefinition`; add a presentation row (bones, cap, chunk). No code. Validators run.
- **New on-break effect**: derive from `FGSTN_PartBreakEffect` in your own plugin; if derived, implement `Project(const
  FGSTN_PartsState&, FGSTN_PartsProjection&)` (pure, in a `*Core`); if one-shot, implement `Request(...)` returning a GE
  or outcome request, applied by the outcome applier. Register with the effect registry; unknown struct types in assets are
  ignored with a warning. An effect must not read live components.
- **New penetration behaviour** (e.g. armour-piercing hits that ignore broken parts): add a flag to `FGSTN_PartHitData`
  and a rule in `UGSTN_PartMath`; test in `GSTNParts.Core`.
- **Another entity kind** (a held object with parts, M10): produce a `FGSTN_PartsSnapshotExt` and call the same step.
