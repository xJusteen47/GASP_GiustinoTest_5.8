# M4b implementation plan: destructible body parts (plugin `GSTNParts`) (draft for user review)

Plan only. Checkpoints are named **P0..P16**. M4 checkpoints are NOT named here: every dependency on M4 is expressed as a
capability (**D1..D14**, section 2) with its spec section in `combat-m4-hits.md`; the lead maps D-numbers to the M4 H-numbers.

Sources (short ids used below):

| Id | File | Used for |
|---|---|---|
| CP | `docs/architecture/combat-parts.md` | The spec of this milestone; section numbers are CP's local ones |
| M4 | `docs/architecture/combat-m4-hits.md` | Hit pipeline, seams (§3b), victim side, lag compensation |
| M8 | `docs/architecture/combat-m8-minions.md` | Minion store, `FGSTN_HitVictimView` (§3), hit flows (§9), part path (§4d.2) |
| M6 | `docs/architecture/combat-m6-defence-links.md` | Grab rules and pipeline order (only P9) |
| MT | `docs/architecture/modularity-and-tags.md` | Layering §3, reuse §3.7, Blueprint §3.8, replaceable content §3.9, tags §4, extension surface §5b |
| DD | `docs/architecture/game-design-direction.md` | Owner direction (wins over CP): §1, §2b, §2e, §2h, §5, §5b, §6b |
| OQ | `docs/architecture/open-questions-m4-m10.md`, section "Parti distruttibili (M4b)" (Q1..Q16) | Owner answers and open items |
| M3P | M3 plan (checkpoints C0..C15) | Format model and the M3 prerequisites (C2, C3a, C3b, C4, C5, C10) |

Owner direction folded in (DD wins over CP where they differ; CP revision 2026-10-22 already applies it):

- Damage **inside** a part is a material parameter only (DD §5b): no spawned chips; flakes are cosmetic and optional (DD §2f).
- A part that **detaches** is a real separation: stump cap + pooled chunk (CP §6.2).
- A **player is a single part**; when it breaks the player dies (OQ Q1 answer, DD §5b, CP §4.5). No limb loss for players.
- Hardness acts on **damage only** (DD §2h): three bands (above 100 %, equal about 65 %, below = ricochet); stagger and hitstop scales of damaging bands are 1.
- Ricochet: no hitstop on either side, attacker recoil montage per attack (DD §2e, M4 §3b item 3c).
- Hitstun is per part: `StaggerThreshold` against the hit's stagger force; weight feeds the reaction tier (DD §2b, CP §4.4).
- LISTEN server is the reference topology (DD §6b).

## 0. Ground rules for every checkpoint

- One checkpoint = one commit (short imperative subject, reason in the body). Gate before each commit: full build (Rider, not
  Live Coding, for header/reflection changes) **with `GSTNParts` disabled, then enabled** (CP §2); `Automation RunTests
  GSTNParts+GSTNCombat+GSTNInput` green; combat tests identical in both builds; harness scenarios named in the checkpoint clean at
  60 ms/15 ms jitter, with and without 2 % loss, once NP code is touched. Prior M1..M4 scenarios still green.
- Network gate for every NP checkpoint: **listen server (host + 2 clients) primary**, dedicated + 2 clients secondary. The host has
  no prediction: scenarios assert "host authoritative, zero reconciles on the host" and "host-local events fire exactly once". No
  `OnRep` on the host: any replicated glue (kit part-set attach, presentation) runs through a function the server also calls
  (`ApplyPartSet()`), as in M3P section 1 item 2. Simulation code has **no `IsServer()` branch** (CLAUDE.md rule 1, CP §5.4).
- Epic coding standard (UE 5.8 conventions, `GSTN_` prefix, `F/U/E` prefixes, `TObjectPtr`, `TStrongObjectPtr` for non-UPROPERTY
  holders, no `auto` outside lambdas/iterators).
- Dependency order inside the plugin (CP §2): `GSTNPartsCore` < `GSTNPartsGAS` < `GSTNPartsNP`; `GSTNPartsPresentation` depends
  on `GSTNPartsGAS` only (never NP); `GSTNPartsEditor` on Presentation; `GSTNPartsMinion` is a separate plugin. Each dependency
  mirrored in the owning `.uplugin`/`Build.cs`; the project module gets only PRIVATE deps.
- Native tags: `UE_DEFINE_GAMEPLAY_TAG_COMMENT`, one doc line each (who writes, projected or granted), declared in the module that
  owns them (`GSTNPartsCore`: `GSTN_PartsTags.h`, `namespace GSTN_PartsTags`). **No tag in sync state or snapshot POD** (part
  indices and masks only). Designer vocabulary (`Part.Type.*`, `GameplayCue.Combat.Part.*`) is ini only, in
  `Plugins/GSTNParts/Config/Tags/GSTNParts.ini`; code never names a leaf (MT §4.2b). Parts never declare `Combat.State.*`,
  `Combat.Mover.*`, `Combat.Lock.*` or `Combat.Link.*` (CP §7).
- README of `Plugins/GSTNParts` is updated in the same checkpoint that changes a module, tag or setting (module table, tag table,
  "Extending in Blueprint", settings). `docs/` is NOT edited in these commits except P14 (one docs commit: combat.md §3 rows, §12
  row M4b, §15 byte budgets, §17 spike results; CLAUDE.md one line). Doc deltas are collected per checkpoint in
  `Saved/Notes/m4b-doc-deltas.md`.
- **R-B Independence acceptance line (every checkpoint):** the plugin builds and its tests run **without the project module**;
  dependencies only on other plugins' `*Core` modules (`GSTNCombatCore`, plus `GSTNCombatGAS`/`GSTNCombatNP` only inside
  `GSTNPartsGAS`/`GSTNPartsNP`, which CP §2 lists); no `/Game/` reference, no project include, no project ini key, no project tag;
  settings in the plugin's own `UDeveloperSettings` with defaults in `Plugins/GSTNParts/Config/DefaultGSTNParts.ini`; optional
  integrations (minions) are bridge plugins, never `#if WITH_<PLUGIN>`; tests carry the `GSTNParts.` prefix. Grep gate in the
  commit: no `ConstructorHelpers`, no `"/Game/`, no `TEXT("/` asset path, no hard `TObjectPtr`/`TSubclassOf` to content where a
  soft reference works.
- **Blueprint extension (MT §3.8):** every extension class (`UGSTN_HitStep_PartResolve` is C++-default but `Blueprintable`
  subclass-able; `UGSTN_PartBreakEffect` bases; outcome applier and attacker-outcome adapters from M4 §3b) is `UCLASS(Abstract,
  Blueprintable, EditInlineNew, DefaultToInstanced)` with ONE `BlueprintNativeEvent` entry point, const view in, result struct out,
  pure (no world/actor/component reads, no latent nodes). Per-frame tick extension stays C++. Tuning structs are
  `USTRUCT(BlueprintType)` with `EditAnywhere, BlueprintReadOnly`; tag fields carry `meta=(Categories=...)`; stateless math in a
  `BlueprintPure` library (`UGSTN_PartMath`). Events reach Blueprint through gameplay messages/cues, not multicast delegates.
- **Replaceable content (MT §3.9):** part sets, penetration tables, presentation sets, chunk/shard meshes, Niagara systems, GEs are
  `TSoftObjectPtr`/`TSoftClassPtr`/Primary Asset IDs; plugin defaults come from `UGSTN_PartsSettings` (soft refs). Simulation-read
  content (part set, penetration table, baked curve) resolves before the pawn's first NP tick and is identical on host and
  clients; N (part count) is not on the wire, so a set mismatch is refused at connect (CP §5.1, MT §5b).
- Test prefixes: `GSTNParts.Core.*`, `GSTNParts.GAS.*`, `GSTNParts.NP.*`, `GSTNParts.Runtime.*` (editor world),
  `GSTNParts.Editor.*`. Harness: `Tools/GASNPPTest/parts_scenarios.py` (imports `gasnpp_test`), names as CP §8.2.
- Disabled-plugin check is a hard gate: a combat build with `GSTNParts` removed passes every M1..M4 test and scenario, and the
  M4 seams stay inert (CP §2, §8.3 item 1).

## 1. Network and cost target: LISTEN server (host also plays)

| CP/M4 assumption | Consequence in the plan |
|---|---|
| DD §6b reference topology | Every NP scenario runs listen first (host + 2 clients), dedicated second. Scenario code asserts host zero reconciles |
| Break/chunk events are outbox events (CP §5.4) | The host's local UI/presentation listens to the **same** outbox once; test "event fires once on host" (P7, P10) |
| Fragment/state applied on `OnRep` | Host has no OnRep: the outcome applier writes the fragment locally at F+1, and presentation reads the sim state, not the replication callback (P6, P10) |
| Presentation is client-only | `GSTNPartsPresentation` is a normal runtime module gated at runtime with `ShouldCreate = false` on `NM_DedicatedServer`; a listen host loads it (CP §2, §5.5b) |
| Budgets (48 chunks, 600 shards, 2 GC) | Host-tier ceilings; measured on the host configuration, scalability-scaled (P12, `scenario_shatter_budget`) |
| Host upload is the cap | Fragment bytes x clients only on change; harness reports host bytes/s with and without parts (P6, P7, P14) |
| Host hits resolve with zero latency | Parts add no advantage beyond M4's; optional `bHostHitFeelParity` is NOT built (assumption A15, section 5) |
| Host leaving ends the session | No part-state migration code |

## 2. Dependencies on M4 (and M3), by capability

M4 is planned by another writer (checkpoints H0, H1, ...). Capability ids D1..D14 are mine; the lead maps them to H-numbers.
"Needed by" lists the first P checkpoint that cannot be done without it. **Seams of M4 §3b are acceptance items of M4**
(CP §1, §8.5 first risk): if a seam is missing, STOP and ask, do not patch the combat core from this plugin.

| Id | M4 capability needed | Spec section (M4 unless noted) | Needed by |
|---|---|---|---|
| D1 | **Victim view + pipeline step classes ready**: `UGSTN_CombatRules` with `Pipeline` as instanced `UGSTN_HitResolutionStep` array, `FGSTN_HitResolutionContext`, `FGSTN_HitVictimView` (fighter and minion fill it), steps const/pure | §3 (context, rules asset), §3b rules paragraph and item 7; M8 §3 (`FGSTN_HitVictimView`) | P4 |
| D2 | **Reaction modifiers in the context**: `FGSTN_HitReactionModifiers` (WeightScale, KnockbackScale, HitstunScale, HitstopScale, StaggerScale, StaggerThreshold, MaxReaction, bNoFlinch) read by `ComputeKnockback`, `Hitstop` and the stagger test; `OutcomeExtensions` array in the context | §3 (three dials, stagger force/threshold, reaction tiers), §3b item 1 | P4, P8 |
| D3 | **`HurtboxIndex` + `FGSTN_Hurtbox::Priority`**: the attacker picks the hurtbox (highest Priority, ties lowest index), stores it in the inbox entry and context; victim never re-derives it | §3 (hurtbox, Priority, line ~139), §3b item 2, §6 step 4 | P0 (spike 1), P4, P7 |
| D4 | **Outcome applier registry**: `IGSTN_OutcomeExtensionApplier` (and its Blueprintable `UGSTN_OutcomeApplier` adapter) called by the combat tick when the outcome is applied at F+1, for each `OutcomeExtensions` entry whose struct type it owns | §3b item 3 and Blueprint paragraph, §7 (inbox rule, state at F+1) | P7 |
| D5 | **Attacker outcome seam + ricochet response**: `IGSTN_AttackerOutcomeExtension::Evaluate(FGSTN_HitContextView) -> Normal/Ricochet`, `EGSTN_AttackerHitOutcome`, `FGSTN_RicochetResponse` on the timeline, `UGSTN_GA_BaseCombat` recoil path (needs the M3 ricochet room, M3P "Ricochet room") | §3b item 3c, §7 (RecoilFrames, cancellable tail), §6 step 6 | P8 |
| D6 | **`DamageMode::PercentCapped`** (`MaxHealth` = cap in percent points, `DamagePercent` clamped, KO raised at the cap, `KOReason`), `UGSTN_KnockbackModel_WeightDriven` standard and `UGSTN_KnockbackModel_CappedPercent` | §3b item 4 and rules paragraph, §3 (knockback models), §4 (attribute set `DamagePercent`) | P3 (math only), P7 (`scenario_part_cap`) |
| D7 | **Hit-data `ExtensionData`** (`TArray<FInstancedStruct>` of base `FGSTN_HitExtension`) on `FGSTN_HitData` and the per-field override variant `FGSTN_HitDataOverride`; unknown extension types ignored with a warning | §3 (`FGSTN_HitData`), §3b item 5; MT §5b | P2 (`FGSTN_PartHitData`), P4 |
| D8 | **Tick-extension point "before candidates"** returning a `HitGroupMask` for the entity's own hitbox groups; tag-projection and snapshot-build hook points of the tick extension | §3b item 6, §6 step 1; M3 C3b (`IGSTN_CombatTickExtension`, S §7.2) | P7 |
| D9 | **Snapshot with extension POD**: snapshot builder at end of F-1 (`UGSTN_CombatWorldSubsystem`), `FGSTN_SnapshotExtension` contributors, per-entity extension memory kept in the lag-compensation history, hurtbox override hook for hidden hurtboxes | §5 (snapshot, restore invalidation), §3b item 7, §9 (history); M8 §3 | P7 |
| D10 | **Sync fragment for victim state**: M3's `FGSTN_SyncFragment` registry (M3 C3a) must carry M4's victim state; no M4 change needed except that M4 reads victim reaction state from the snapshot. Part state lives in OUR fragment | M3 C3a; M4 §11 (sync state additions), §9 (replicated state used by the attacker) | P6 |
| D11 | **Attacker hitstop predicted from static data only**: `Auto` mode from the hit's damage and nominal knockback, `Override`, `AttackerHitstopFramesOverride`; the hook that multiplies by `HitstopScale` | §3 (hitstop), §8 | P8 |
| D12 | **Events/outbox with the hit envelope** (command, move, hit type, relationship), `Combat.Event.KO`, `Combat.Event.HitReceived` last-hit context | §6, §7; combat.md §5.6 | P5, P7, P12 |
| D13 | **Harness hit scenarios and a test character with a hurtbox set** (`scenario_hit_basic`, `scenario_hit_host`, `scenario_stagger_threshold`, `UGSTN_HurtboxSetDefinition`, debug draw) | §10 | P7 (first PIE scenario of ours) |
| D14 | **Lag compensation** (`RewindActors`, `MaxRewindMs`) consistent with the chosen `HurtboxIndex` | §9, §9b | P0 (spike 1), P7 |

Dependencies on M3 (named in M3P): C3a (fragment registry, `FGSTN_CombatSyncCore`) for P6; C3b (rules, tick extension, kit
extension base, victim view placeholder) for P4/P7; C4 (`UGSTN_CombatAttributeSet`) for P3 math tests and P5; C5 (kit
`ExtensionData`, `FGSTN_KitExtension`) for P7 attach; C10 (`UGSTN_GA_BaseCombat`) for P8 recoil. Dependency on M6 (P9 only): the
grab rules (M6 §3 `FGSTN_GrabRules::Requirements`, M6 §8 `Combat.Immunity.Grab`). Dependency on M8 (P15, P16): `FGSTN_MinionNetItem`,
minion store, minion hit flow (M8 §8.2, §9.2, §4d.2).

**What can start in parallel with late M3/M4:**

- P1, P2, P3 depend on nothing beyond M3 C3b-level core types (D7 only for the hit-data struct shape; stub the extension base in
  Core if D7 is not merged, and swap in the real base in the P4 commit). They are pure and can start now.
- P10 (damage material, hidden bones, idempotent apply, dedup) and P11 start once P5's event payload types exist; they need
  no hit pipeline at all (drive them from a test `FGSTN_PartsState`).
- P0 spike 2 (presentation cost) needs no M4 code at all.
- P0 spike 1 needs D3 + D14.

## 3. Checkpoints (in order)

Legend: [module] - [files] - [tags] - [tests] - [acceptance] - [spec] - [size S/M/L]. Every checkpoint also carries the R-B
Independence line and the Blueprint/replaceable-content gates of section 0.

### P0 Spikes (no plugin code; scratch branch or `Saved/` only)

Output `Saved/Notes/m4b-spike-report.md` (to become combat.md §17). Four sub-steps, each with pass/fail and numbers.

- **P0a Overlap to part (CP §8.4 spike 1)** - needs D3, D14. Rule: highest `Priority`, ties lowest index, `HurtboxIndex` stored by the
  attacker. Build the throwaway measurement on the M4 test character with 6 hurtboxes, two of them deliberately overlapping, under
  lag compensation (60/15 ms, 2 % loss), listen host as attacker, remote client as attacker, jittery victim movement. Measure the
  disagreement rate of the chosen index between the predicting attacker client and the server (and between listen host and remote
  client). **STOP and ask** if the disagreement rate is above 0.1 % of hits, or if any disagreement changes damage in a way the
  inbox does not already resolve (the inbox carries the index, so the expected result is: client may flash a different hurtbox
  for one frame, the server value wins; confirm this is cosmetic).
- **P0b Presentation cost (CP §8.4 spike 2)** - no M4 needed. 40 simultaneous shatters with pre-fractured pieces via Niagara mesh
  particles (600 live) versus a Geometry Collection per hero; limb chunk pool at 48 with Chaos rigid bodies. Measure on the
  **listen host configuration** (simulation + render in one process) at the M8 N=40 load if M8 is present, else with 40 dummy
  actors. **STOP and ask** if the host frame time goes above 16 ms (62.5 Hz) on the reference machine with the proposed budgets;
  the answer is lower budgets or a Niagara-only tier, which changes CP §6.4 numbers (owner open question Q16).
- **P0c Fragment wire format** - needs M3 C3a. Fragment `NetSerialize` size with N = 1, 6, 8, 16 and 0/1/2 fragments, bits against
  the CP §5.1 table. **STOP and ask** if a 16-part fragment exceeds 24 B or if zero parts is not exactly the flag bit.
- **P0d Seam audit of M4** - reads only. Check that D1..D9 exist as merged code (grep for `FGSTN_HitReactionModifiers`,
  `HurtboxIndex`, `IGSTN_OutcomeExtensionApplier`, `IGSTN_AttackerOutcomeExtension`, `PercentCapped`, `ExtensionData`, `HitGroupMask`).
  Output: a table "seam, present, location". **STOP and ask** for any absent seam (see section 4).
- Acceptance: report with numbers; each STOP condition evaluated. Gates P7 (P0a, P0c), P12 (P0b). Size: M.

### P1 Plugin skeleton: `GSTNParts` + `GSTNPartsCore` + tags

- Files: `Plugins/GSTNParts/GSTNParts.uplugin` (`EnabledByDefault: false`; dependency `GSTNCombat` plugin only), `README.md`,
  `Config/DefaultGSTNParts.ini`, `Config/Tags/GSTNParts.ini`; `Source/GSTNPartsCore/{GSTNPartsCore.Build.cs (deps: Core,
  CoreUObject, GameplayTags, GSTNCombatCore), Public/GSTNPartsCoreModule.h, Public/GSTN_PartsTags.h, Private/GSTN_PartsTags.cpp,
  Public/GSTN_PartsSettings.h (UDeveloperSettings: default penetration table soft ref, default weight curve, presentation
  budgets as soft/data fields)}`. `.uproject` stays untouched until the owner enables the plugin (section 6).
- Native tags (CP §7), all with `UE_DEFINE_GAMEPLAY_TAG_COMMENT`: `Combat.Part.Broken`, `Combat.Part.GrabLocked`,
  `Combat.Part.AllBroken` (projected); root `Combat.Part.Effect` (leaves ini); `Combat.Entity.Destructible`;
  `Combat.Event.Part.{Damaged,WeakPointHit,Ricochet,Broken,AllBroken}`; `Message.Combat.PartBroken`. Ini only: `Part.Type.*`,
  `GameplayCue.Combat.Part.{Hit,Break,Ricochet,Shatter}`.
- Tests: `GSTNParts.Core.Tags.AllRegisteredAndDocumented`, `GSTNParts.Core.Tags.NoStateTagDeclared` (no `Combat.State.*`,
  `Combat.Mover.*`, `Combat.Lock.*`, `Combat.Link.*` declared by this plugin).
- Acceptance: build with the plugin disabled and enabled; editor loads; tags visible in the picker; `GSTNCombat` tests unchanged.
  Grep gate: `GSTNPartsCore` includes no GAS/NP/Mover header and depends only on `GSTNCombatCore` of other plugins.
- Spec: CP §2, §7. Size: S.

### P2 Part set data, penetration table, break-effect structs, validators (`GSTNPartsCore`)

- Files: `Public/Data/{GSTN_PartSetDefinition.h (UGSTN_PartSetDefinition DataAsset), GSTN_PartDefinition.h
  (FGSTN_PartDefinition, FGSTN_PartReaction, EGSTN_BrokenHurtboxPolicy, FGSTN_GrabGate), GSTN_PenetrationTable.h
  (UGSTN_PenetrationTable, FGSTN_PenetrationBand), GSTN_BakedCurve.h (FGSTN_BakedCurve: 64-entry uint16 table, integer
  interpolation), GSTN_PartHitData.h (FGSTN_PartHitData : FGSTN_HitExtension, flags IgnoreHardness/MainOnly),
  GSTN_PartBreakEffects.h (base FGSTN_PartBreakEffect + Tags, DisableHurtbox, DisableHitGroups, Stats (declared), MainDamage,
  Launch, Stagger), GSTN_PartsRulesExtension.h (FGSTN_PartsRulesExtension : FGSTN_RulesExtension), GSTN_PartsKitExtension.h
  (FGSTN_PartsKitExtension : FGSTN_KitExtension)}`, `Private/Data/*.cpp` incl. `IsDataValid`.
- Rules: part limit 16 (`uint16` mask), `Hardness` 0..15 integer, `PartCap <= 0` = unbreakable, a hurtbox in at most one part, no
  parent cycle, finite caps/transfers, empty band set = error, mask > 16 error, missing bone = warning. Break effects are plain
  structs in `FInstancedStruct` arrays; unknown struct types in assets ignored with a warning (MT §5b). Primary asset type
  registration for the part set (soft refs everywhere, no literal paths).
- Tags: none new.
- Tests: `GSTNParts.Core.Data.{PartSetValid,DuplicateHurtbox,ParentCycle,MaskOver16,MissingBoneWarning}`,
  `GSTNParts.Core.Penetration.{BandLookupMinus8ToPlus8,IgnoreHardness,OneBandTable,EmptyTableRejected,BandsSortedDescending}`,
  `GSTNParts.Core.BakedCurve.{MatchesHandValues,Monotonic,IntegerOnly}`, `GSTNParts.Core.Data.UnknownEffectIgnored`,
  `GSTNParts.Core.Data.DefaultTableThreeBands` (above 1.0/1.0, equal 0.65/0.65, below ricochet with zero scales, stagger and hitstop
  scales 1 in damaging bands per DD §2h).
- Acceptance: tests green; a new part set and a new table work with no C++; structs BlueprintType; default table matches CP §3.4.
- 2026-10-22 evening, traversal surfaces ([traversal-surfaces.md](../traversal-surfaces.md) §4.1, plan T3/T6): **no surface field is
  added to `FGSTN_PartDefinition`**. An enemy's wall-slideable or trampoline body portions are platform shapes (`GSTNPlatform`)
  keyed by the part's `Name`; the bridge `GSTNPlatformParts` resolves names to indices and disables the shapes from the confirmed
  `BrokenMask`. P2 only guarantees that `Name` is unique (already validated) and stable across restores.
- Spec: CP §3.1, §3.3, §3.4, §3.6 (data only); M4 §3b item 5 (D7 base). Size: M.

### P3 Pure part math: state, quantisation, hit application, weight (`GSTNPartsCore`)

- Files: `Public/Math/{GSTN_PartsState.h (FGSTN_PartsState: BrokenMask uint16, PartDamage uint8[16], BreakSerial), GSTN_PartMath.h
  (UGSTN_PartMath BlueprintPure + static: LookupBand, ApplyHit, Quantise/Dequantise, ComputeCascade, EffectiveWeight,
  ProjectDerived), GSTN_PartsProjection.h (FGSTN_PartsProjection: granted/removed tag containers, hurtbox mask, hit group mask,
  GrabLocked, AllBroken)}`; `Private/Math/*.cpp`.
- Rules implemented exactly as CP §4.2/§5.1: `PartDelta = D x b.PartScale x h.PartDamageScale x p.PartDamageScale`, `MainDelta =
  D x b.MainScale x p.MainTransfer` (`BrokenMainTransfer` for a broken part), stored byte = `min(255, ceil(Percent/Cap x 255))`
  (a nonzero hit always moves the byte; next hit starts from the dequantised value), no overflow to the main pool (A3), cascade of
  children in the same hit in index order, `MainDamage` of newly broken parts added once each. `EffectiveWeight = BaseWeight x
  WeightCurve(Progress) x Product(1 - LostMassFraction)` clamped at `MinWeightScale` (default 0.35). No floats compared for
  determinism decisions except the integer band delta. Pure statics, no allocation.
- Tests: `GSTNParts.Core.Math.{ApplyHitGolden (hits to expected bytes, mask, MainDelta), Cascade, WeakPointMoreMainThanTorso,
  RicochetNoDamage, OverflowNotCarried, BrokenPolicyKeep/Disable/Replace, NonzeroHitChangesByte, SinglePartPlayerBreakIsAllBroken}`,
  `GSTNParts.Core.Determinism.{1000RandomSequencesTwoConfigs, WeightCurveTableEqualsHandValues, EffectiveWeightNeverBelowClamp}`,
  `GSTNParts.Core.Math.TwoHitsSameFrameOrderIndependentGivenSortedInbox`, `GSTNParts.Core.Math.RestoreParts{ClearsBitsAndBytes,
  KeepsOthers, RestoreThenBreakAgain}` (pure `UGSTN_PartMath::RestoreParts(State, Mask, Fraction)` for P7b; restoring a part does not
  give back main-pool percent, which is the healer's own GE).
- Acceptance: golden and determinism green on Win64 editor and a second compile config (e.g. Development vs DebugGame); zero
  allocations in `ApplyHit` (allocation counter test). No dependency on any M4 type except `GSTNCombatCore` value structs.
- Spec: CP §4.2, §4.3, §5.1, §8.1. Size: M.

### P4 `UGSTN_HitStep_PartResolve`: the pipeline step (`GSTNPartsCore`) - needs D1, D2, D3, D7

- Files: `Public/Pipeline/{GSTN_HitStep_PartResolve.h (UGSTN_HitStep_PartResolve : UGSTN_HitResolutionStep, Blueprintable-extensible,
  const), GSTN_PartHitOutcome.h (FGSTN_PartHitOutcome : the OutcomeExtensions entry: part index, new quantised damage, newly broken
  mask, band, event flags), GSTN_PartsPipelineValidator.h}`; `Private/Pipeline/*.cpp`.
- Behaviour (CP §4.2, §4.4, §5.3): reads the victim only through `FGSTN_HitVictimView` + the parts snapshot extension POD
  (`FGSTN_PartsSnapshotExt`, plain struct declared here, filled in P7 or by the minion path); finds the part from `HurtboxIndex`;
  band from `h.Penetration` vs `Hardness`; writes `Ctx.Outcome.Damage = MainDelta`, `Ctx.Reaction` (`WeightScale = EffectiveWeight /
  BaseWeight` x part x body `JuggleFactor` x lost mass, `KnockbackScale`, `HitstopScale = b x part`, `StaggerThreshold` part else
  body else unset, `StaggerScale = b.StaggerScale`, `MaxReaction = min(part, body)`), `bLethal` from cap and `MainDamage`, a ricochet
  band zeroes damage/reaction and flags `Ricochet` for the attacker-outcome seam. Appends one `FGSTN_PartHitOutcome`. A blocked,
  parried, grabbed or intangible hit never reaches the step (order). No part set (no snapshot ext) = identity, so `ApplyDamage` adds `D`
  unchanged.
- Rules validator: `UGSTN_CombatRules::IsDataValid` hook (through the rules-extension validate seam of M4, or our own
  `FGSTN_PartsRulesExtension::Validate`) reports `PartResolve` placed before `Parry`/`Shield` or after `ComputeKnockback`, and a kit
  with a part set under rules without the step.
- Tests: `GSTNParts.Core.Step.{FighterViewAndMinionViewSameOutput, BlockedHitDamagesNoPart, IdentityWithoutPartSet,
  StaggerThresholdEqualStaggersOneBelowDoesNot, PartBodyRulesFallback, WeakPointStaggersWhereArmourDoesNot, RicochetBandNoStaggerNoHitstop,
  MaxReactionCapsTier, LowerWeightNeverMilderTier, AttackerHitstopIndependentOfDynamicPartState, OrderValidator}`; M4 fake-step pipeline
  test still green with the plugin loaded.
- Acceptance: with the plugin present and no part set on any entity, M4 scenarios are identical (byte for byte state compare); step
  has no component or world access (grep gate); Blueprint subclass example compiles in test content later (P14).
- Spec: CP §3.4, §4.2, §4.4, §5.3, §8.1; M4 §3, §3b items 1-2, 5. Size: L.

### P5 `GSTNPartsGAS`: break-effect appliers, events, tag projection

- Files: `Source/GSTNPartsGAS/{Build.cs (deps GSTNPartsCore, GSTNCombatGAS, GameplayAbilities, GameplayTags), Public/Effects/{GSTN_PartBreakEffectApplier.h
  (applies one-shot effects: Stats via infinite GE with `Combat.SetByCaller.Stat.*`, Launch via the M4 knockback layered move request,
  Stagger as minimum hitstun frames), GSTN_GE_PartStats.h}, Public/Events/GSTN_PartEvents.h (FGSTN_PartEventPayload { PartIndex,
  BreakSerial, Band } + emit helpers onto the M2 outbox with the hit envelope, dedupe key (EntityRef, PartIndex, BreakSerial)),
  Public/Projection/GSTN_PartTagProjection.h (projection helpers for the tick extension)}`.
- `MainDamage` is NOT applied here (already folded into `MainDelta` in P4, no double count).
- Tags: none new.
- Tests (transient stock ASC, no fork): `GSTNParts.GAS.Effects.{StatsApplyRemoveByBreak, StatsMagnitudesBySetByCaller, LaunchRequestShape,
  StaggerMinimumFrames}`, `GSTNParts.GAS.Events.{PayloadRoundTrip, DedupeKey, EnvelopeCarriesHitData, TwoEntriesOrderIndependent}`,
  `GSTNParts.GAS.Projection.{BrokenTagAfterBreak, RemovedBaseTagWhileBroken, GrabLockedGate, AllBrokenCountsBreakableOnly}`.
- Acceptance: module builds with no NP/Mover dependency (grep gate); tests green on a stock ASC.
- Spec: CP §3.3, §5.3 (applier), §5.4, §7; M4 §6, §7 (D12). Size: M.

### P6 `GSTNPartsNP`: `FGSTN_PartsFragment` (sync-state fragment) - needs D10 (M3 C3a)

- Files: `Source/GSTNPartsNP/{Build.cs (GSTNPartsGAS, GSTNCombatNP, NP fork), Public/State/GSTN_PartsFragment.h (FGSTN_PartsFragment :
  FGSTN_SyncFragment: BrokenMask, PartDamage[N], BreakSerial 4 bits; custom NetSerialize with the 1-bit empty flag, bit-for-bit
  CP §5.1 table; ShouldReconcile = exact integer equality; Interpolate = later sample; ToString prints mask and per-part percent),
  Private/State/*.cpp, Public/GSTN_PartsFragmentRegistration.h (fixed type id from the central registry file)}`. Entities without a
  part set carry no fragment: zero bytes, no compare cost. `N` not sent; set mismatch refused at connect.
- Tests: `GSTNParts.NP.Fragment.{RoundTrip0,1,2Fragments, N1_8_16EmptyAndFull, ShouldReconcileExact, BytesMatchTable, ZeroPartsZeroBytes,
  UnknownTypeIdIgnored, ToStringStable}` (extends the 2-fragment test of MT §5b); byte count printed for the harness report.
- Acceptance: `GSTNCombat.NP.Fragments.*` and M2/M3 scenarios identical with the fragment registered; per-client sync bytes report
  (host bytes/s = per-client x clients, listen) printed.
- Spec: CP §5.1, §8.1; MT §5b; M4 §11. Size: M.

### P7 `GSTNPartsNP`: snapshot contributor, tick extension, outcome applier, kit attach; first PIE scenarios - needs D3, D4, D6, D8, D9, D13, D14, P0a, P0c

- Files: `Public/Sim/{GSTN_PartsSnapshotExt.h (POD per CP §5.2: BrokenMask, PartDamage[16], PartSetId, derived GrabLocked bit; <= 20 B),
  GSTN_PartsSnapshotContributor.h (FGSTN_SnapshotExtension: reads the fragment at END OF F-1), GSTN_PartsTickExtension.h
  (UGSTN_PartsTickExtension : IGSTN_CombatTickExtension, explicit Priority: tag projection from the mask; HitGroupMask before
  candidates; hurtbox overrides for broken parts per policy), GSTN_PartsOutcomeApplier.h (IGSTN_OutcomeExtensionApplier for
  FGSTN_PartHitOutcome: writes PartDamage/BrokenMask/BreakSerial into the fragment at F+1, requests one-shot break effects through
  P5, pushes events to the outbox), GSTN_PartsAttach.h (reads FGSTN_PartsKitExtension from the kit ExtensionData, `ApplyPartSet()`
  callable on host and clients, registers fragment and marker tag `Combat.Entity.Destructible` in the victim view tags)}`.
- Rollback rules (CP §5.4) as tests: values read by `PartResolve` come from snapshot or hit; state written only in the outcome applier
  at the victim's tick; events deduplicated by `(EntityRef, PartIndex, BreakSerial)`; mask monotonic.
- Pipeline: `UGSTN_CombatRules` test asset pipeline lists `PartResolve` after `Shield` and before `ComputeKnockback` (a test content
  edit; needs owner OK, section 6).
- Tests: `GSTNParts.NP.Snapshot.{ReadsEndOfFrameMinusOne, BytesWithinBudget, LagHistoryMemory}`, `GSTNParts.NP.Applier.{WritesAtFPlus1,
  RollbackRedeliveryCreatesBreakOnce, EventsOncePerKey}`, `GSTNParts.NP.TickExtension.{OrderByPriority, ProjectsTagsFromMask,
  HitGroupMaskFromBroken, HiddenHurtboxPolicy}`, `GSTNParts.NP.Attach.{HostWithoutOnRep, ClientAfterRep, SetMismatchRefused}`.
- Harness (`parts_scenarios.py`, listen primary then dedicated, 60/15 ms, with and without 2 % loss):
  `scenario_part_break` (c1 hits a limb of c2 until broken: mask, bytes, percent equal on all machines; `Part.Broken` exactly once,
  also on the host), `scenario_part_cap` (main percent reaches the cap: KO exactly once), `scenario_part_hitgroups` (lost arm stops its
  own hitbox: no hit, no hitstop, no confirm), `scenario_part_rollback` (forced reconcile around the break: no duplicate event or
  state), `scenario_part_host` (host attacker, host victim, remote client against host: states equal, host zero reconciles, host
  events once). Clean = snapshots equal, no reconciles beyond the one per hit on the victim's client (M4 §10).
- Acceptance: all scenarios clean in both topologies; no `IsServer()` in plugin simulation code (grep gate); with the plugin disabled
  M4 scenarios unchanged; host bytes/s report with and without parts.
- Spec: CP §3.5, §5.2, §5.3, §5.4, §5.5b, §8.2; M4 §3b, §5, §6, §7, §9, §9b, §10. Size: L.

### P7b Part restore (owner answer 2026-10-22 18:22: no self-regeneration, explicit restore allowed) - needs P7

- Goal: an ability or effect (e.g. an enemy that heals others) restores some or all broken parts of a target, rollback-safe.
- Request path: `FGSTN_PartRestoreRequest { PartMask (0 = all), float DamageFraction (0 = full repair), Frame }`. Default (b): a
  parts-owned pending request in `FGSTN_PartsFragment`, written by a simulated GE component `UGSTN_GEComponent_RestoreParts`
  (`GSTNPartsGAS`, Blueprint-visible fields) and consumed by `UGSTN_PartsTickExtension` at the victim's next tick, so prediction and
  reconcile follow the fragment; no combat-core change. Alternative (a): an M4 inbox entry with a `Support` hit type (needs the M4
  `Support` step, M4.13): only if P0d shows (b) cannot be rollback-safe. `UGSTN_PartsLibrary::RequestRestore` (BlueprintCallable,
  authority, outside simulation) applies that GE.
- State: clears the `BrokenMask` bits, sets `PartDamage` to the restored fraction, increments `BreakSerial` (so restore + break again
  is a new event key), re-applies removed break effects' inverse only where the effect declares `bReversible` (Stats GE removed,
  Launch/Stagger one-shots not reversed). Grab gate and hit groups re-derive from the mask with no extra code.
- Tags/events (`GSTNPartsCore`): `Combat.Event.Part.Restored` (payload part index, BreakSerial).
- Presentation (P10/P11): the idempotent hidden-bone set already un-hides from the confirmed mask; stump caps and pooled chunks
  are released on `Part.Restored`; a restore cue tag on the request for VFX.
- Tests: `GSTNParts.NP.Restore.{ClearsMaskAtFPlus1, RollbackRedeliveryRestoresOnce, RestoreThenBreakNewSerial,
  IrreversibleEffectsStay}`, `GSTNParts.GAS.Restore.GEComponentRequests`. Scenario `scenario_part_restore` (c1 breaks a limb of an
  enemy, the host's healer dummy restores it, c2 breaks it again: masks equal everywhere, `Restored` and `Broken` once each, host too).
- Acceptance: as P7; players (single part) cannot be "restored" from death by this path (KO wins; validator warns on player sets).
- 2026-10-22 evening: `Combat.Event.Part.Restored` and the confirmed mask are the inputs of the `GSTNPlatformParts` bridge
  ([traversal-surfaces-plan.md](traversal-surfaces-plan.md) T6: restored parts re-enable their traversal shapes); nothing to add here.
- Spec: CP §4.5, §5.4, game-design-direction §5b. Size: M.

### P8 Hit feel through parts: stagger, weak point, ricochet, attacker hitstop - needs D2, D5, D11

- Files: `GSTNPartsNP/Public/Sim/GSTN_PartsAttackerOutcome.h` (IGSTN_AttackerOutcomeExtension implementation: returns `Ricochet` when the
  chosen part's band is `bRicochet`, from static data and the snapshot mask only), a static-data hitstop contributor (band + part
  `HitstopScale`, never dynamic state), content for ricochet (default `FGSTN_RicochetResponse` fallbacks live in M3/M4 data, not here).
- The attacker predicts ricochet exactly: penetration and hardness are static and the broken mask is replicated (CP §3.4).
- Tests: `GSTNParts.NP.Ricochet.{AttackerPredictsFromStaticData, NoHitstopEitherSide, NoHitConfirm, VictimGetsCueAndEventOnly}`,
  `GSTNParts.NP.Hitstop.{AttackerIgnoresBrokenMaskPercentWeight, PartHitstopScaleAppliesToAutoOnly, OverrideIsFinal}`,
  `GSTNParts.NP.Reaction.{WeightTierMovesWithBrokenLimbs, JuggleFactorPerPart, AirborneJuggleScale}`.
- Harness: `scenario_part_ricochet` (hard part, low penetration: no part damage, no hitstun, attacker recoils with the attack's montage,
  no hitstop on either side, predicted on the attacker's client without correction), `scenario_part_stagger` (light and heavy on an armoured
  part, light on the weak point: light on armour = damage without hitstun; heavy on armour = hitstun and with `MaxReaction = PushBack` a
  grounded push back; light on weak point = hitstun; equal on all machines), `scenario_part_weakpoint` (weak point takes more main damage
  than the torso for the same hit).
- Acceptance: scenarios clean listen and dedicated at 2 % loss; the attacker's recoil does not trigger a reconcile.
- Spec: CP §3.4, §4.4, §8.2; M4 §3, §3b item 3c, §7, §8. Size: M.

### P9 Grab gate (data and projection only; scenario waits for M6) - optional-late

- Files: gate evaluation inside `UGSTN_PartMath::ProjectDerived` (P3) already; here: tick extension projects `Combat.Part.GrabLocked`
  (P7) and a **rules-asset edit documented** (designer adds `Combat.Part.GrabLocked` to the ignore list of `FGSTN_GrabRules::Requirements`;
  no M6 code change, no foreign tag declaration, CP §7). A boss keeps `Combat.Immunity.Grab` and never has a gate.
- Tests (no M6 needed): `GSTNParts.Core.GrabGate.{EmptyNeverLocked, MinBrokenParts, RequiredParts, AnyOf}`, `GSTNParts.NP.GrabGate.TagProjected`.
  Scenario `scenario_part_grabbable` (grab refused while locked, accepted after the required parts break) is **deferred until M6's grab
  ability exists**; record as an open item in the commit body.
- Acceptance: projection correct on all machines; no change to M6 sources.
- Spec: CP §7 (Grabbability), §8.2. Size: S.

### P10 `GSTNPartsPresentation`: component, damage material, hidden bones (client + host) - needs P5 payload types only

- Files: `Source/GSTNPartsPresentation/{Build.cs (GSTNPartsGAS, Engine, Niagara, GeometryCollectionEngine, ChaosSolverEngine), Public/
  GSTN_PartsPresentationComponent.h (drives the damage parameter from replicated state every frame: `0..1` = part damage / cap, or
  `Progress` for a single-part player; one PartIndex channel in the mesh (vertex colour or UV region) so one material serves all parts;
  dynamic material instances created lazily; smoothed for display; follows rollbacks), GSTN_PartPresentationSet.h
  (UGSTN_PartPresentationSet: per part bones, stump cap mesh + socket, chunk class + impulse, parameter names, mask channel, shatter
  profile; all soft refs), GSTN_PartsPresentationSubsystem.h (event listener keyed by (EntityRef, PartIndex, BreakSerial);
  `ShouldCreate = false` on NM_DedicatedServer, true on listen host and clients)}`.
- Idempotent hidden-bone set: every frame, `HideBone` for the bits of the **confirmed** `BrokenMask` (late join and relevancy re-entry
  show the right body with no event replay). `bRequireConfirmed = true`: irreversible presentation only from the outbox event or a
  mask bit that arrived in replicated/corrected state (CP §6.5); reversible cosmetics (flash, material) may use predicted data.
- Optional cosmetic flakes: Niagara or pooled mesh particles seeded by `(EntityRef, PartIndex, ThresholdIndex)`, monotonic, no collision
  with pawns (CP §6.1); default off in the first version unless owner wants them (assumption A17).
- Tests (`GSTNParts.Runtime.*`, editor world, no PIE): `Presentation.{MaterialFollowsReplicatedValue, MaterialFollowsRollback,
  HiddenBonesIdempotentFromMask, LateJoinShowsBrokenBody, DedupeByKey, DedicatedServerCreatesNothing, ListenHostCreatesSubsystem,
  NoWriteIntoSimulation (disabled presentation => identical state)}`.
- Acceptance: a client with presentation disabled produces identical simulation state (CP §8.3 item 6); host path equals client path;
  host events presented once.
- Spec: CP §2, §6.1, §6.2, §6.5, §8.1, §5.5b. Size: M.

### P11 Limb detachment: stump cap and pooled chunks (option A)

- Files: `Public/GSTN_DebrisPoolSubsystem.h` (pool of 48 chunk actors, 4 s lifetime then shrink-fade, oldest recycled, no pawn collision,
  no gameplay hits, no input), `GSTN_PartChunkActor.h`, stump-cap attach in the component. Chunk spawned once per `(EntityRef, PartIndex,
  BreakSerial)`, impulse from presentation `Launch` data plus the hit direction (last `HitReceived`). Pool scaled by scalability level
  and tier (M8 §7: beyond T1 Niagara burst only, beyond T2 nothing); sleeping bodies stop after 1.5 s. Geometry Collection only for
  hero crystals (hook, not content).
- Tests: `GSTNParts.Runtime.Debris.{PoolNeverExceedsBudget, OldestRecycled, SkippedChunkNoGameplayEffect, OnePerKey, ScalabilityScaling,
  NoCollisionWithPawn}`.
- Acceptance: breaking a limb on a test character hides the bones, shows the stump cap, spawns one chunk on host and on both clients;
  dropping the chunk changes no simulation state.
- Spec: CP §6.2, §6.4. Size: M.

### P12 Death shatter and budget (option: pre-fractured pieces) - needs D12, P0b

- Files: `Public/GSTN_ShatterDriver.h` (reads `Combat.Event.KO` and the last hit direction/impulse; hides the skin mesh in the same frame
  behind a flash; spawns pre-fractured crystal pieces as pooled Niagara mesh particles (600 live, CPU sim, simple ground collision, no
  pawn collision)), `GSTN_PartsPresentationBudget` use from the rules extension, frame-time governor on the host (drops to bursts when the
  host exceeds its frame budget), Geometry Collection cap 2 for players/bosses.
- Tests: `GSTNParts.Runtime.Shatter.{OnceOnKO, ParticleBudget, GCCapTwo, RespectsTierRules, SkinHiddenSameFrame}`.
- Harness: `scenario_shatter_budget` (40 deaths in 2 s) on a **client and on the listen-server host**: frame time and pool counters
  within CP §6.4 ceilings (numbers from P0b).
- Acceptance: scenario clean; budget ceilings not exceeded; a skipped shatter never changes gameplay.
- Spec: CP §6.3, §6.4, §8.2, §5.5b. Size: M.

### P13 `GSTNPartsEditor`: validators and report (M5 hook)

- Files: `Source/GSTNPartsEditor/{Build.cs (GSTNPartsPresentation, UnrealEd), Public/GSTN_PartSetValidator.h (asset validator wrapper),
  GSTN_PartBalanceReport.h (prints "hits to break/kill" and "hits to stagger" per part against a chosen hit), GSTN_PhysicsAssetToPartSet.h
  (helper: Physics Asset bodies to a draft part set; the M5 hook, thin)}`. Plain editor module, no tools UI beyond the validator and a
  console/commandlet report in this checkpoint; the full part gizmo/editor belongs to M5.
- Tests: `GSTNParts.Editor.{DuplicateHurtbox, ParentCycle, MaskOver16, MinionMaskOver8, MissingBone, BalanceReportNumbers,
  PhysicsAssetDraftHasUniqueNames}`.
- Acceptance: invalid sets are reported with the asset path; the report matches hand-computed hits-to-kill from P3 goldens.
- Spec: CP §3.1, §8.1, §8.5 (balance explosion risk), §2 (editor module). Size: S.

### P14 Sample content, Blueprint examples, full sweep, docs commit

- Content under `Plugins/GSTNParts/Content` (plugin-owned test content only, no `/Game/` reference): example part set for a fighter
  (6 parts incl. a weak point), a player single-part set, penetration table asset, presentation set, example Blueprint subclasses of a
  break effect and of `UGSTN_HitStep_PartResolve` (MT §3.8). Test map reuses the M4 test character (owner OK needed to touch shared
  assets, section 6).
- Full sweep: all `GSTNParts.*` tests; all CP §8.2 scenarios except the minion and grabbable ones, in listen (host + 2 clients) and
  dedicated + 2 clients, 60/15 ms, with and without 2 % loss; M1..M4 scenarios with the plugin disabled and enabled; byte table check
  (`FGSTN_PartsFragment` within CP §5.1, zero bytes without parts); host bytes/s with and without parts.
- Docs commit (the only `docs/` edit): combat.md §3 module rows, §12 milestone row M4b, §15 byte budgets, §17 spike results; CLAUDE.md
  one line; README "Extending in Blueprint", "How to extend" (CP §8.6).
- Acceptance: CP §8.3 items 1, 2, 3, 6, 7 verified (items 4 and 5 completed by P9's later scenario and P16).
- Size: M.

### P15 `GSTNPartsMinion` interface only (late, optional; M8 not required to build) - needs M8 §3 view only

- Files: new tiny plugin `Plugins/GSTNPartsMinion/` skeleton (`.uplugin` deps `GSTNParts`, `GSTNMinion`; module stub), plus in `GSTNPartsCore` the
  already-pure pieces made minion-ready: 8-part limit validator, `FGSTN_PartsMinionExtension` data type declaration, conversion rule
  (`Stats` break effect rejected or converted to the minion stats struct), `FGSTN_PartsSnapshotExt` builder from `{uint8 mask, uint8
  damage[8]}`. No store columns, no wire change.
- Tests: `GSTNParts.Core.Minion.{StepSameOutputAsFighter, MaskOver8Rejected, StatsEffectRejectedOrConverted, SnapshotExtFromStore}` (fake
  store arrays, no M8 code).
- Acceptance: plugin builds with `GSTNMinion` absent for the pure part (the bridge module is the only one needing it, and is not
  built if `GSTNMinion` is not enabled); Core tests green. This checkpoint is done only when the owner wants the minion path; otherwise
  skip P15/P16 entirely.
- Spec: CP §5.5 (first half), §3.1 (8 parts); M8 §3, §4d.2. Size: S.

### P16 `GSTNPartsMinion` full path (needs M8 complete: store, wire, hit flow, bench)

- Files: bridge module: store columns for part-bearing types (`uint8 BrokenMask`, `uint8 PartDamage[8]`, 9 B each), `FGSTN_PartsMinionExtension`
  on `UGSTN_MinionDefinition`, hit-flow glue building `FGSTN_PartsSnapshotExt` from the store and applying the outcome directly (no GE, no
  fragment), optional wire byte `ExtByte` (`BrokenMask`, guarded by type-level `bHasExt`) in `FGSTN_MinionNetItem` (**an M8 change**: owner and
  M8 writer agree first, section 6), promotion/demotion copy to and from the fragment (M8 §10).
- Tests: `GSTNParts.Minion.{StoreOutcomeNoGE, ExtByteOnlyOnTypesWithParts, DirtyOnlyOnMaskChange, PromotionCopiesToFragment,
  AttackerHitstopStaticOnly, PartEventsViaOutbox}`; harness `scenario_minion_parts` (M8 bench: 40 minions with part sets: wire bytes within
  +5 % of the item update, per-hit server cost <= 2 us), listen host included (host upload x clients reported).
- Acceptance: CP §8.3 item 5 minion half; M8 acceptance unchanged without the bridge plugin.
- Spec: CP §5.5, §8.3 item 5; M8 §4d.2, §8.2, §8.3, §9.2, §10. Size: L.

### P17 Test-map stations for parts

- Add the parts dummy to TM2 `L_Test_Combat` (station 8) after P12, and the parts boss to TM3 `L_Test_Enemies` (station 5) after P16, as in
  [test-maps.md](test-maps.md). Content only, built by Ludus AI or a person from primitives. Size: S.

## 4. Spikes first and stop-and-ask conditions

Spikes are P0 (above). Summary of stop conditions (do not continue past them without the owner or lead):

| Stop condition | Where | Why |
|---|---|---|
| Any of D1..D9 seams missing or reshaped incompatibly in M4 | P0d, P4, P7 | M4b must not edit the combat core (CP §8.5); ask for an M4 follow-up commit |
| Index disagreement attacker/server above 0.1 % or damage-changing | P0a | Changes the hit-selection rule or forces re-derivation (CP §3.4) |
| Host frame time over 16 ms with the budgets | P0b | CP §6.4 numbers change (OQ Q16) |
| 16-part fragment above 24 B, or non-flag zero-part cost | P0c | Revisit quantisation or part cap |
| `HitVictimView`/snapshot ext cannot carry 20 B/entity in the lag history | P7 | Memory budget conflict with M4 §9 |
| A scenario needs `IsServer()` to pass | P7, P8 | Design smell, stop and ask (CLAUDE.md rule 1) |
| M8 wire change refused by the M8 owner | P16 | Fall back to "mask not on the wire, limbs from `Health` only" |

## 5. Assumptions for unanswered owner questions

OQ ids refer to "Parti distruttibili (M4b)" in `open-questions-m4-m10.md`. A = assumption id used in the commit bodies.

| OQ | Assumption planned on | Affects | Cost to change |
|---|---|---|---|
| Q1 (rest) | Break of the single player part = death; no gameplay limb effects for players; effects on enemies via `OnBreak` data | P3, P7, P14 | Low: data only (add break effects to the player set) |
| Q2 (A2) | **Answered 2026-10-22 18:22**: no self-regeneration; parts stay broken by default, but an explicit restore (e.g. a healer enemy) can clear bits of `BrokenMask` and reset `PartDamage`. Built in P7b; the break path stays monotonic within a hit | P3, P7b, P10, P11 | done (P7b) |
| Q3 (A3) | No overflow of damage beyond `PartCap` into the main pool; bonus only by explicit `MainDamage` | P3 | Low: one rule in `UGSTN_PartMath` plus golden |
| Q4 (A4) | Lethal parts configurable (`bKill` in `MainDamage`) but no shipped default; values of weak-point transfer are data | P2, P3, P14 | Low |
| Q5 | Integer hardness/penetration 0..15 (answer reads as integer armour levels; confirm) | P2, P3 | Medium: changes struct types and the band lookup, but isolated in `GSTNPartsCore` |
| Q6 | Ricochet leaves the attacker punishable only during `RecoilFrames`, then cancellable tail (answered via DD §6c, M4 §7) | P8 | Low: M4 data |
| Q7 (A7) | Highest `Priority` wins, ties lowest index; resolved attacker-side (spike P0a) | P0, P7 | Low if still attacker-side |
| Q8 (A8) | Weight falls with remaining health and with `LostMassFraction` per broken part, clamp 0.35 | P3, P4 | Low: data |
| Q9 (A9) | Juggle = weight factor per part and entity; global `AirborneJuggleScale` default 1; no decay | P4, P8 | Low |
| Q10 (A10) | Threshold on stagger force; hitstun duration depends on knockback only (does not grow with force above the threshold) | P4 | Low: M4 owns the duration rule |
| Q11 | Attacker hitstop uses only static data (band + part scale), in both modes | P8 | Low: contributor only |
| Q12 (A12) | `FGSTN_GrabGate` = min broken parts and/or required parts, empty gate never locks; values authored per enemy | P2, P9 | Low: data |
| Q13 (A13) | Detached limbs purely cosmetic (never in the simulation); throwing is M10 | P11 | Medium: M10 would need real entities |
| Q14 | Exposed crystal has no gameplay rule; vulnerability from hardness and transfer | P2, P10 | Medium: a second hardness layer |
| Q15 (A15) | No host mitigation; `bHostHitFeelParity` not built | P7, P10 | Low: presentation delay only |
| Q16 (A16) | Budgets 48 chunks, 600 particles, 2 GC; scaled down by scalability and host frame time | P11, P12 | Low: settings |
| (new) A17 | Cosmetic flakes optional and off by default in the first version | P10 | Low |
| (new) A18 | Players use one part whose `PartCap` mirrors the entity cap, so `Progress` drives the material | P3, P10 | Low |
| (new) A19 | Minion part count 8, ext byte only for part-bearing types (+5 % item) | P15, P16 | Medium: M8 wire |

## 6. Blueprint and shared-asset edits that need the owner's OK before they happen

- Enabling `GSTNParts` in `GASP.uproject` (plugin is `EnabledByDefault: false`).
- Adding `UGSTN_HitStep_PartResolve` to the test `UGSTN_CombatRules` asset used by the M4 scenarios (P7), and adding
  `Combat.Part.GrabLocked` to the grab rules ignore list (P9, M6 content).
- Adding `FGSTN_PartsKitExtension` to the shared test kit/weapon assets and a `UGSTN_PartSetDefinition` to the shared test character
  (P7, P14); adding `Priority` values to the shared `UGSTN_HurtboxSetDefinition` (the M4 test character).
- Changes to project Blueprints, maps (`L_GASNPPTest`), `BS_GASNPP_Test`, project `Config/DefaultGameplayTags.ini` or
  `DefaultEngine.ini` (none planned: tags live in plugin ini).
- The mesh authoring convention for the damage material: one `PartIndex` channel in vertex colour/UV of the character meshes
  (P10); owner/artist decision, affects content, not code.
- The M8 wire change (`ExtByte`, P16) and the M8 store extension: needs the M8 writer and owner agreement.
- Docs: CP/combat.md edits only in P14, only with the owner's rule lifted.

## 7. Risks and parallelisation hints

| Risk | Mitigation |
|---|---|
| M4 seams slip or arrive in a different shape | P0d audit; D-list is the interface contract; P1-P3 are pure and survive any seam shape |
| Part choice differs attacker/server | Inbox carries `HurtboxIndex`; P0a measures; the server value wins |
| Rollback duplicates events/chunks | Dedupe key (EntityRef, PartIndex, BreakSerial), mask monotonic, state only at F+1 in the applier; `scenario_part_rollback` |
| Balance explosion (hardness, transfer, thresholds, tiers) | P13 report "hits to break/kill/stagger" per set; all numbers are data |
| Listen host cost (sim + render + upload in one process) | Host-tier budgets, frame-time governor, P0b, `scenario_shatter_budget` on host, bytes x clients report |
| Host advantage on instant breaks | Accepted by default (A15) |
| Presentation leaks into simulation | `NoWriteIntoSimulation` test; module never depends on NP; disabled-presentation client has identical state |
| Fragment bytes with many part-bearing types | Cap 16 parts (8 minions), zero bytes without parts, bytes table asserted |
| Plugin coupling creeping into `GSTNCombat` | Disabled-plugin build is a gate of every checkpoint; seams stay inert |

**Parallel threads (separate implementation threads can take at once):**

- Thread A (pure, starts now): P1 -> P2 -> P3 -> P4 (P4 waits for D1/D2/D3/D7); P15 pure part afterwards.
- Thread B (presentation, starts after P5 payload types exist): P10 -> P11 -> P12; P0b spike first; P13 can run beside it.
- Thread C (net, needs M3 C3a and M4 seams): P5 -> P6 -> P7 -> P8; P9 beside P7 (S).
- P0a/P0c need M4 and M3 C3a respectively; P0b and P0d are free at any time.
- Late: P14 (sweep and docs) after A, B, C; P16 only after M8 and the owner's go-ahead.
- Serial constraints: P4 before P7 (step before applier); P6 before P7 (fragment before applier); P5 before P7 (effects/events); P7
  before P8; P10 before P11 before P12; P7 and P10 meet only through the event payload and the confirmed mask (no code dependency).
