# GSTNParts

Destructible body parts for the GSTN combat on Unreal Engine 5.8: part sets (hardness, break caps, transfer of damage to the
main pool), the penetration table that turns a hit's penetration against a part's hardness into damage or a ricochet, break
effects as data, and the pure, integer-quantised math that applies a hit to a part. An entity without a part set behaves as it
did before the plugin.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoints P1-P6 of
[m4b-parts-plan.md](../../docs/architecture/plans/m4b-parts-plan.md) are built (skeleton and tags, data, pure math, the pipeline
step, the GAS half, the sync fragment); the design is
[combat-parts.md](../../docs/architecture/combat-parts.md). The NP tick extension and applier (P7), hit feel (P8)
and the presentation (P10-P12) are not built. Owner direction in
[game-design-direction.md](../../docs/architecture/game-design-direction.md) wins over the spec: hardness acts on damage only
(three bands: above, equal about 65 %, below ricochets), a player is a single part and its break is death, parts do not
regenerate by themselves.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNPartsCore` | exists (Runtime, Default) | Native tags, settings, the part set and penetration table data, break-effect and hit-data structs, validators, the integer baked curve, the part state and the deterministic part math, and the part resolve step of the hit pipeline. No GAS, Network Prediction or Mover, so it needs no fork. Covered by `GSTNParts.Core.*`. |
| `GSTNPartsGAS` | exists (Runtime, Default) | The one-shot break effects (`FGSTN_PartBreakEffectApplier`: the Stats effect per part through `UGSTN_GE_PartStats`, the launch request in the shape of the M4 reaction move, the stagger floor), the part events (`FGSTN_PartEvents`, `FGSTN_PartEventPayload`, the dedupe key and window) and the tag projection helpers (`FGSTN_PartTagProjection`). Stock GAS: no Network Prediction, Mover or `GSTNCombatNP`. Covered by `GSTNParts.GAS.*`. |
| `GSTNPartsNP` | exists (Runtime, Default), P6 + P7 | `FGSTN_PartsFragment` (fixed type id 16 of `GSTNFeatureIds`, 3 + N bytes, empty = 0 bytes) and the simulation hooks the module registers at startup: `FGSTN_PartsSnapshotContributor` (the parts at the end of F-1 in the combat snapshot extension blob, hurtboxes of broken Disable parts Intangible), `FGSTN_PartsTickExtension` (priority 100: hit groups before candidates, tags and stats as a projection of the mask), `FGSTN_PartsOutcomeApplier` (the only writer of the fragment, monotonic mask, makes the `Combat.Event.Part.*` events), `FGSTN_PartsAttach` and `UGSTN_PartsAttachComponent` (the part set from the kit extension through the loadout-applied event, same path on host and client, set mismatch refused), `UGSTN_PartsKitLibrary` (editor and harness helpers). Needs the GAS-NPP fork and the D9 snapshot seam of GSTNCombatNP. Covered by `GSTNParts.NP.*`. |
| `GSTNPartsPresentation` | not built (P10-P12) | Damage material, hidden bones, chunks and shatter. Loaded by a listen-server host, skipped on a dedicated server. |
| `GSTNPartsEditor` | not built (P13) | Validators and report. |

The module class (`Private/GSTNPartsCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager: the
engine reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins, so the ini vocabulary
would otherwise be missing.

## Dependencies

- Plugins: `GSTNCombat` (`GSTNCombatCore`), and for the GAS half `GSTNCombatGAS`, `GSTNInput` (`GSTNInputCore`, the simulation event type)
  and `GameplayAbilities`.
- `GSTNPartsCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNCombatCore; private
  Projects (to find the plugin's own folder). No edge to the project module, to a sibling plugin or to GAS, NP and Mover.
- `GSTNPartsGAS` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, GSTNInputCore, GSTNCombatCore,
  GSTNCombatGAS, GSTNPartsCore. No NetworkPrediction, AbilitySystemSimulation, Mover or `GSTNCombatNP` (grep gate in the P5 commit).
- `GSTNCombat` builds and passes its tests with this plugin absent: the plugin adds behaviour by registering into the combat
  extension surface (modularity-and-tags.md §5b), never by editing the combat core.

## Data

| Type | What it is |
|---|---|
| `UGSTN_PartSetDefinition` (primary asset type `GSTN_PartSet`) | The parts of one kind of entity: up to 16 `FGSTN_PartDefinition`, an optional penetration table, the body reaction, a grab gate and an optional weight curve (baked to 64 integers at load). A player's set is one part whose break is death. |
| `FGSTN_PartDefinition` | One part: name, `Part.Type.*` tags, hurtbox names, bones, parent, `Hardness` 0-15, `PartCap` (0 or less = unbreakable), damage scale, `MainTransfer` / `BrokenMainTransfer`, broken-hurtbox policy, weak-point flag, `FGSTN_PartReaction`, `LostMassFraction`, `OnBreak` effects. |
| `UGSTN_PenetrationTable` | The penetration rule as data: bands sorted by `MinDelta` descending, the first band with `Penetration - Hardness >= MinDelta` wins. A new table starts as the default: above full, equal 65 %, below ricochets, with stagger and hitstop scales of 1 in the damaging bands (hardness acts on damage only). |
| `FGSTN_PartBreakEffect` and derived structs | Derived (recomputed from the mask): `Tags`, `DisableHurtbox`, `DisableHitGroups`. One-shot (applied once at the break): `Stats`, `MainDamage`, `Launch`, `Stagger`. Plain structs in `FInstancedStruct` arrays; unknown ones are ignored with a warning. |
| `FGSTN_PartHitData` | Penetration, part damage scale and flags (`IgnoreHardness`, `MainOnly`) of a hit. |
| `FGSTN_PartsKitExtension` | An entry of the combat kit's extension array that gives a fighter a part set (soft reference). |
| `FGSTN_PartsRulesExtension` | Game-wide values (default table, weight defaults, debris budget); inherits the settings by default. |
| `FGSTN_PartsPresentationBudget` | The debris ceilings, shared by the settings and the rules extension. |
| `FGSTN_PartsBakedCurve` | A 0..1 curve as 64 `uint16` entries with integer interpolation, so every machine reads the same number. |

`FGSTN_PartsValidation` holds the rules as pure functions over arrays (`ValidateParts`, `ValidateBands`); the assets'
`IsDataValid` forward their findings, and the editor module and the minion bridge planned for M8 (not built) call them with their own limits. Errors:
more than 16 parts, a duplicate or empty name, a hurtbox in two parts, a bad or cyclic parent, hardness outside 0-15, a
non-finite or negative number (also in the `Launch`, `Stagger`, `Stats` and `DisableHurtbox` effects), a grab gate that can never
open (a required part, or a minimum count, that no break can reach: an unbreakable part with no breakable ancestor), an empty
penetration table, two bands with one `MinDelta`. Warnings: a bone the preview mesh lacks, a part type outside `Part.Type`, a part
with no hurtbox, an unknown break effect, a band table that stops short of the lowest delta, with no damaging band, or whose top
band ricochets.

## The part math

`UGSTN_PartMath` (static, allocation free on the hit path, no tag, string, world or component access) over plain views of the
authored data and a plain `FGSTN_PartsState` (`BrokenMask` uint16, `PartDamage[16]` bytes, `BreakSerial` 4 bits: the contract
of the sync fragment and the snapshot; no floats, tags or pointers):

- **Quantisation.** The stored byte is `min(255, ceil(damage / cap x 255))`; a nonzero hit always moves it; the next hit starts
  from the dequantised value (worked in byte units, so there is no drift); 255 is "at the cap" and is what breaks a part.
  The `ceil` rounds up on every hit (on average half a byte, 0.2 % of the cap, per hit, and a hit of any size is at least one
  byte): a weapon that lands many tiny hits breaks a part a little sooner than its nominal damage says. Chosen so a nonzero hit
  always moves the state; weigh it when tuning multi-hit or damage-over-time attacks.
- **`ApplyHit`.** `PartDelta = D x band part scale x hit scale x part scale`, `MainDelta = D x band main scale x MainTransfer`
  (`BrokenMainTransfer` on a stump). Overflow beyond the cap is not carried to the main pool. Children of a broken part break in
  the same hit (also unbreakable ones: they go with the limb) and each newly broken part adds its `MainDamage` effects once, in
  index order. A ricochet band changes nothing and reports zero stagger and hitstop scales. A hit on the body, or on a broken part
  whose policy is `Disable`, is the full damage. `Replace` damages like `Keep` (its shape is not declared yet).
- **Inbox order.** Two hits in one frame must be applied in the inbox's sorted order; each sees the state the previous one left.
- **Weight.** `EffectiveWeightScale = curve(Progress) x Product(1 - LostMassFraction of the broken parts)`, floored at
  `MinWeightScale`; the curve is the baked integer table, the progress is quantised to 16 bits first.
- **Derived projection.** `ProjectDerived` fills an `FGSTN_PartsProjection` (granted and removed tags, hurtbox names to hide, hit
  group mask, Broken / AllBroken / GrabLocked) from the mask; derived break effects implement `Project`. `IsGrabLocked` evaluates
  the grab gate (an empty gate never locks).
- **Restore.** `RestoreParts(Parts, State, Mask, Fraction)` unbreaks the masked parts (0 = all, so check a computed mask for
  emptiness first) and sets their damage to a fraction of the cap, never higher than it was; the serial advances so restore then
  break again is a new event key. A child cannot stay intact under a broken parent, so the broken ancestors of the masked parts
  are restored too, and a later break never cascades onto an intact child. It does not give main-pool percent back.

Blueprint gets the stateless helpers (`GetPenetrationBandIndex`, `QuantisePartDamage`, `DequantisePartDamage`, `IsPartBroken`,
`CountBrokenParts`, `GetEffectiveWeight`); `ApplyHit` is C++ only (hot path, MT section 3.8).

## The part resolve step

`UGSTN_HitStep_PartResolve` (`Pipeline/`) is the pipeline step of the combat plugin's hit resolution (combat-parts.md 4.2, 4.4, 5.3). Put it in
the rules asset's `HitPipeline` after `TeamFilter`, `HurtState`, `HurtboxReaction`, `Parry` and `Shield` and before `ComputeKnockback`. It
**replaces `HealthWeight` and `ApplyDamage`**: it writes the weight (the health weight and the mass lost by broken parts), and it requests the damage
itself, because `ApplyDamage` requests the hit's own damage and would ignore the part's share. For an entity without a part set it writes exactly what
`ApplyDamage` writes, so one pipeline serves both kinds of entity.

- Reads the victim only through the view: `FGSTN_PartsSnapshotExt` (`Pipeline/`), the broken mask, the damage bytes and the part set, handed over as
  `FGSTN_HitVictimView::SnapshotExtension` (a `const void*` the extension identifies by a type word). The NP adapter fills it (P7); a test fills it by hand.
- The part comes from `HurtboxIndex` (the attacker's choice, never re-derived) through the part's hurtbox names; a hurtbox no part names is the body.
- Writes the reaction modifiers (weight scale times the juggle factors, knockback and hitstop scales, the band's stagger scale, the part's stagger threshold
  else the body's else unset, the lower reaction cap), the main pool damage, and one `FGSTN_PartHitOutcome` (the state the hit left, the events as flags) for the outcome applier.
- A ricochet band stops the pipeline (`bHit` false): no damage, no freeze on either side, no flinch.
- The table is the set's, else the rules extension's (`FGSTN_PartsRulesExtension`, in `UGSTN_CombatRules::RulesExtensions`), else the settings', else the built-in three bands.
- `FGSTN_PartsPipelineValidator` reports `PartResolve` before `Parry`/`Shield`, `ApplyDamage` next to it, and a part set under rules without the step. The
  combat rules validator already reports it after `ComputeKnockback` and next to `HealthWeight`.

`FGSTN_PartHitData` is a `FGSTN_HitExtension` (an entry of `FGSTN_HitData::ExtensionData`) and `FGSTN_PartsRulesExtension` a `FGSTN_RulesExtension`.

## Gameplay tags

| Tags | Source | Declared in |
|---|---|---|
| `Combat.Part.Broken`, `.GrabLocked`, `.AllBroken` | native, **projected** from the broken mask, never granted | `Public/GSTN_PartsTags.h`, namespace `GSTNPartsTags` |
| `Combat.Part.Effect` (root; leaves such as `ArmLost` are content) | native root | same |
| `Combat.Entity.Destructible` | native (marker in the victim view) | same |
| `Combat.Event.Part.Damaged`, `.WeakPointHit`, `.Ricochet`, `.Broken`, `.AllBroken`, `.Restored` | native | same |
| `Message.Combat.PartBroken` | native (UI channel, raised outside the simulation) | same |
| `Part.Type.Head`, `.Torso`, `.Arm`, `.Leg`, `.Weapon`, `.Armour`, `.Crystal` | ini, designer vocabulary | `Config/Tags/GSTNParts.ini` |
| `GameplayCue.Combat.Part.Hit`, `.Break`, `.Ricochet`, `.Shatter` | ini, designer vocabulary | same |

The plugin declares no `Combat.State.*`, `Combat.Mover.*`, `Combat.Lock.*` or `Combat.Link.*` tag (checked by
`GSTNParts.Core.Tags.NoStateTagDeclared`). No `Combat.State.Hitstun` or `Combat.State.Hitstop` tag exists yet in any plugin: the
stagger and hitstop a part causes are values in the hit result, and the M4 hit pipeline decides how they reach the victim. No
part tag is ever stored in a sync state or a snapshot: those carry part indices and masks.

## Settings

Project Settings > Plugins > GSTN Parts (`UGSTN_PartsSettings`, `GSTNParts.ini`): `DefaultPenetrationTable` (empty = the
built-in three bands), `DefaultWeightScaleAtCap` (0.6),
`MinWeightScale` (0.35), `PresentationBudget` (48 chunks, 4 s, 600 shard particles, 2 Geometry Collections, 1.5 s sleep).
Defaults ship in `Config/DefaultGSTNParts.ini`; a project overrides them in its own `Config/DefaultGSTNParts.ini`. The weight
values are read by the simulation, so every machine of a match must run the same; the presentation budget is local.

## Extending in Blueprint

A new part set, a new penetration table and new break-effect entries need no C++ and no build: author the assets. Tuning structs
are `BlueprintType` with `EditAnywhere, BlueprintReadOnly` fields (checked by `GSTNParts.Core.Data.StructsAreBlueprintTypes`) and
tag fields carry `Categories`. A new kind of break effect derives from `FGSTN_PartBreakEffect` in your own plugin. The
Blueprintable extension classes (the pipeline step, outcome appliers) arrive with their checkpoints. The simulation hooks of `GSTNPartsNP` (snapshot contributor, tick extension, native outcome applier) are C++ only by design: they run for every entity on every simulated and re-simulated frame. To give a pawn parts: put `UGSTN_PartsAttachComponent` on it and add `FGSTN_PartsKitExtension` to its kit (the helper `UGSTN_PartsKitLibrary.SetKitPartSet` does the second from Python); to use them in a match: `UGSTN_PartsContentLibrary.ConvertRulesToParts` turns a rules asset into one with `PartResolve` in place of `HealthWeight` and `ApplyDamage`.

The Stats break effect needs one Blueprint class: a child of `UGSTN_GE_PartStats` (infinite) whose modifiers on the combat
attribute set read `Combat.SetByCaller.Stat.*` magnitudes, exactly like a weapon stats effect. One class serves every part of an
entity (the part index rides in the spec level), and its modifiers must read only keys that every Stats entry of the set
defines, because the engine logs an error for a key the spec never set. Parts that need a different set of stats use a second
child class. The effect is applied once per broken part and removed only by a restore; a part's `OnBreak` list needs no C++.

## Replacing content

The plugin ships no content yet; every reference between assets is soft (checked by `GSTNParts.Core.Data.NoHardContentReferences`). A game adds its own `Part.Type.*` and `GameplayCue.Combat.Part.*` leaves in its own
`Config/Tags` ini and its own part sets and penetration tables as assets; the defaults are soft references in the settings.

## Tests

`Automation RunTests GSTNParts`: `Core.Smoke`, `Core.Tags.*` (documented, no state tag, owned subtrees, ini vocabulary),
`Core.Settings.Defaults`, `Core.Data.*` (part set valid, duplicate hurtbox, parent cycle, mask over 16, missing bone, numbers,
unknown effect, asset helpers, extensions, Blueprint types, no hard references, default table), `Core.Penetration.*` (band
lookup from -8 to +8, ignore hardness, one band, empty table, sorted bands), `Core.BakedCurve.*` (hand values, monotonic,
integer only), `Core.Math.*` (quantise rule and hand values, golden hit sequence, cascade, weak point, ricochet, overflow, the
three broken-hurtbox policies, nonzero hit moves the byte, single-part player, inbox order, restore, projection, AllBroken,
no allocation), `Core.GrabGate.Evaluation`, `Core.Determinism.*` (2000 random sequences with a pinned fingerprint, weight
curve against hand values, weight clamp). `GSTNParts.GAS.*` (stock ability system component, no fork): `Effects.*` (Stats
applied and removed per part, magnitudes by set-by-caller key, the launch request shape, the stagger floor), `Events.*`
(payload round trip, dedupe key and window, the hit envelope on every event, two entries of a drain in either order),
`Projection.*` (Broken, the removed base tag, GrabLocked, AllBroken counting breakable parts only). The canary build
(plugin alone in a blank project, modularity §3.7 rule 7) is not run yet: the canary script does not exist.
`GSTNParts.NP.Restore.*` and `GSTNParts.GAS.Restore.*` (P7b: mask cleared at F+1, rollback redelivery restores once, restore then break is a new serial, one-shots stay, the GE component requests). `GSTNParts.NP.*` (P7 part 1): `Snapshot.*` (end of F-1, bytes, lag history), `Applier.*` (writes at F+1, the real victim drain rebuilds between entries, rollback redelivery, event keys, monotonic mask), `TickExtension.*` (order, tags from the mask, hit groups, hidden hurtboxes), `Attach.*`, `Content.*`.

## Restoring parts

Parts never heal by themselves; an explicit restore clears broken bits and gives back damage. The request is an INFINITE gameplay effect on the target carrying `UGSTN_GEComponent_RestoreParts` (fields `PartMask`, 0 = every part, and `DamageFraction`, 0 = full repair); `UGSTN_GE_PartRestore` is the native default and any Blueprint effect with the component works, so a healer enemy's simulated ability just applies it. `UGSTN_PartsLibrary::RequestRestore(Target, PartMask, DamageFraction, EffectClass, Instigator)` does the same from authority code outside the simulation. The parts tick extension consumes the effect at the target's next combat tick (hit of the same frame first), restores the fragment, publishes `Combat.Event.Part.Restored` (part index and the new break serial, so restore then break again is a new event key) and removes the effect; rollback re-delivers it and gets the same result. The wire format is unchanged. Derived effects (tags, hit groups, hidden hurtboxes, Stats effects) reverse with the mask; one-shot effects (MainDamage, Launch, Stagger) leave nothing to undo. A knocked out single-part set (a player) is not restored: KO wins.

## Changelog

- 0.1 (2026-10-25): part restore (plan P7b): `UGSTN_GEComponent_RestoreParts`, `UGSTN_GE_PartRestore`, `UGSTN_PartsLibrary`, `Combat.Event.Part.Restored`, the consumer in the tick extension; harness scenarios `part_break/cap/hitgroups/rollback/host/restore` (plan P7 part 2) and the part event log hooks of `UGSTN_PartsKitLibrary`.
- 0.1 (2026-10-25): `GSTNPartsNP`: snapshot contributor, tick extension, outcome applier, attach, content helpers (plan P7 part 1).
- 0.1 (2026-10-25): `GSTNPartsNP`: `FGSTN_PartsFragment` and its registration (plan P6).

- 0.1 (2026-10-25): `GSTNPartsGAS`: break-effect applier (Stats per part, launch request, stagger floor), part events with the hit
  envelope and the dedupe key, tag projection helpers (plan P5).
- 0.1 (2026-10-25): the part resolve step, its snapshot extension and outcome entry, the pipeline validator (plan P4).
- 0.1 (2026-10-23): part state, quantisation, hit application, cascade, weight, projection, grab gate and restore math
  (plan P3); plugin skeleton, native tags, ini vocabulary, settings (plan P1); part set, penetration table, break
  effects, hit data, kit and rules extensions, validators, baked curve (plan P2).
