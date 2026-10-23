# GSTNParts

Destructible body parts for the GSTN combat on Unreal Engine 5.8: part sets (hardness, break caps, transfer of damage to the
main pool), the penetration table that turns a hit's penetration against a part's hardness into damage or a ricochet, break
effects as data, and the pure, integer-quantised math that applies a hit to a part. An entity without a part set behaves as it
did before the plugin.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoints P1-P3 of
[m4b-parts-plan.md](../../docs/architecture/plans/m4b-parts-plan.md) are built (skeleton and tags, data, pure math); the design is
[combat-parts.md](../../docs/architecture/combat-parts.md). The hit pipeline step (P4), the GAS appliers (P5), the NP sync
fragment, tick extension and applier (P6, P7), hit feel (P8) and the presentation (P10-P12) are not built: they wait for the hit
pipeline of M4 (see "Waiting for M4"). Owner direction in
[game-design-direction.md](../../docs/architecture/game-design-direction.md) wins over the spec: hardness acts on damage only
(three bands: above, equal about 65 %, below ricochets), a player is a single part and its break is death, parts do not
regenerate by themselves.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNPartsCore` | exists (Runtime, Default) | Native tags, settings, the part set and penetration table data, break-effect and hit-data structs, validators, the integer baked curve, the part state and the deterministic part math. No GAS, Network Prediction or Mover, so it needs no fork. Covered by `GSTNParts.Core.*`. |
| `GSTNPartsGAS` | not built (P5) | Break-effect appliers, events, tag projection helpers. |
| `GSTNPartsNP` | not built (P6-P8) | Sync fragment, tick extension, snapshot contributor, outcome applier. Needs the GAS-NPP fork. |
| `GSTNPartsPresentation` | not built (P10-P12) | Damage material, hidden bones, chunks and shatter. Loaded by a listen-server host, skipped on a dedicated server. |
| `GSTNPartsEditor` | not built (P13) | Validators and report. |

The module class (`Private/GSTNPartsCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager: the
engine reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins, so the ini vocabulary
would otherwise be missing.

## Dependencies

- Plugins: `GSTNCombat` (its `GSTNCombatCore` module only).
- `GSTNPartsCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNCombatCore; private
  Projects (to find the plugin's own folder). No edge to the project module, to a sibling plugin or to GAS, NP and Mover.
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
`IsDataValid` forward their findings, and the editor module and the minion bridge call them with their own limits. Errors:
more than 16 parts, a duplicate or empty name, a hurtbox in two parts, a bad or cyclic parent, hardness outside 0-15, a
non-finite or negative number, an empty penetration table, two bands with one `MinDelta`. Warnings: a bone the preview mesh
lacks, a part with no hurtbox, an unknown break effect, a band table that stops short of the lowest delta.

## The part math

`UGSTN_PartMath` (static, allocation free on the hit path, no tag, string, world or component access) over plain views of the
authored data and a plain `FGSTN_PartsState` (`BrokenMask` uint16, `PartDamage[16]` bytes, `BreakSerial` 4 bits: the contract
of the sync fragment and the snapshot; no floats, tags or pointers):

- **Quantisation.** The stored byte is `min(255, ceil(damage / cap x 255))`; a nonzero hit always moves it; the next hit starts
  from the dequantised value (worked in byte units, so there is no drift); 255 is "at the cap" and is what breaks a part.
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
- **Restore.** `RestoreParts` unbreaks the masked parts (0 = all) and sets their damage to a fraction of the cap, never higher than
  it was; the serial advances so restore then break again is a new event key. It does not give main-pool percent back.

Blueprint gets the stateless helpers (`GetPenetrationBandIndex`, `QuantisePartDamage`, `DequantisePartDamage`, `IsPartBroken`,
`CountBrokenParts`, `GetEffectiveWeight`); `ApplyHit` is C++ only (hot path, MT section 3.8).

## Gameplay tags

| Tags | Source | Declared in |
|---|---|---|
| `Combat.Part.Broken`, `.GrabLocked`, `.AllBroken` | native, **projected** from the broken mask, never granted | `Public/GSTN_PartsTags.h`, namespace `GSTN_PartsTags` |
| `Combat.Part.Effect` (root; leaves such as `ArmLost` are content) | native root | same |
| `Combat.Entity.Destructible` | native (marker in the victim view) | same |
| `Combat.Event.Part.Damaged`, `.WeakPointHit`, `.Ricochet`, `.Broken`, `.AllBroken` | native | same |
| `Message.Combat.PartBroken` | native (UI channel, raised outside the simulation) | same |
| `Part.Type.Head`, `.Torso`, `.Arm`, `.Leg`, `.Weapon`, `.Armour`, `.Crystal` | ini, designer vocabulary | `Config/Tags/GSTNParts.ini` |
| `GameplayCue.Combat.Part.Hit`, `.Break`, `.Ricochet`, `.Shatter` | ini, designer vocabulary | same |

The plugin declares no `Combat.State.*`, `Combat.Mover.*`, `Combat.Lock.*` or `Combat.Link.*` tag (checked by
`GSTNParts.Core.Tags.NoStateTagDeclared`): stagger is the existing `Combat.State.Hitstun`, hitstop the existing
`Combat.State.Hitstop`. No part tag is ever stored in a sync state or a snapshot: those carry part indices and masks.

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
Blueprintable extension classes (the pipeline step, outcome appliers) arrive with their checkpoints.

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
curve against hand values, weight clamp). The canary build
(plugin alone in a blank project, modularity §3.7 rule 7) is not run yet: the canary script does not exist.

## Changelog

- 0.1 (2026-10-23): part state, quantisation, hit application, cascade, weight, projection, grab gate and restore math
  (plan P3); plugin skeleton, native tags, ini vocabulary, settings (plan P1); part set, penetration table, break
  effects, hit data, kit and rules extensions, validators, baked curve (plan P2).
