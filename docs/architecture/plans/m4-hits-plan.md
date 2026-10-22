# M4 implementation plan: Hits (draft for owner review)

Plan only, no code. Checkpoints are **H0..H13**; other plans reference them by that name.

## 0. Sources and what each checkpoint implements

| Id | Source | Used for |
|---|---|---|
| S | [combat-m4-hits.md](../combat-m4-hits.md) | the spec; section numbers below are S §n |
| DD | [game-design-direction.md](../game-design-direction.md) | owner direction, wins over S (§2, §2b–§2h, §4, §6, §6b, §6c) |
| MT | [modularity-and-tags.md](../modularity-and-tags.md) | layers §3, reuse §3.7, Blueprint §3.8, replaceable content §3.9, tags §4, extension surface §5b |
| OQ | [open-questions-m4-m10.md](../open-questions-m4-m10.md) | M4 questions 1–36 and "Lacune" (lines 135–347); answered ones carry **Risposta** |
| CM | [combat.md](../combat.md) | §5.6 outbox, §12.4–§12.7, §15 budget, §17 spike results |
| M3 | [combat-m3-moves-kits.md](../combat-m3-moves-kits.md), M3 plan (C0..C15) | types this plan builds on |
| P | [combat-parts.md](../combat-parts.md) §1, §3.4, §4.4 | the seams M4b needs (planned by another writer; here only the seams) |
| M6 / M8 | combat-m6-*.md §6 (contact record), combat-m8-minions.md §3 (victim view, snapshot extension) | contracts reused, not built here |
| T | [testing.md](../testing.md) | harness, test prefixes |

| Checkpoint | Spec sections |
|---|---|
| H0 spikes | S §2, §9b (topologies) |
| H1 hit geometry and data | S §3 (shapes, hit data, hitbox, hurtboxes, modifier track), §1b (AffectedTags) |
| H2 reaction model | S §3 (knockback models, three dials, stagger force, hitstun condition, hitstop, tiers, weight/juggle, `FGSTN_VictimReactionState`), DD §2b, §2h |
| H3 DI and impact math | S §3 (DI), §7b (impact resolve, precedence), DD §2c, §2d, §4 |
| H4 pipeline, rules and seams (Core) | S §3 (pipeline, rules), §3b (all items + Blueprint forms), MT §5b, P §1 |
| H5 GAS: damage, attributes, effects, events | S §4, §3 (KO, DamageMode), CM §5.6 |
| H6 NP: sync state, world subsystem, snapshot, inbox | S §5, §11, §7 (inbox), §3b items 2/7, M8 §3 |
| H7 NP: hitstop and freeze on both sides | S §8, CM §12.6 |
| H8 NP: attacker side | S §6, §3b 3c/6 |
| H9 NP: victim side, KO, events (**seams complete**) | S §7, §3 (reaction apply), §3b 3, DD §6c |
| H10 lag compensation and presentation | S §9, §9b, CM §12.7 |
| H11 surface impacts and trampolines | S §7b, M6 contact record |
| H12 base dodges, ricochet response, dodge gating | S §1b, §3b 3c, DD §6, §2c |
| H13 content, tools, harness, validators, docs | S §10, §12, §13 |

Current M3 state (takeover-state.md, 2026-10-22 evening): C1–C6 committed on `feature/gas-npp-5.8` (C3 PIE gate green). C7–C15
pending. This repo branch is docs only.

## 1. Ground rules for every checkpoint

- One checkpoint = one commit (H4 may be two: Core seams reviewed on their own, P §1 "separate commits"). Gate before each
  commit: full build (Rider, not Live Coding, for header/reflection changes); `Automation RunTests GSTNCombat` (and `GSTNInput`)
  green; the listed PIE scenarios clean in the **listen-server** configuration (host + 2 clients, primary) and in dedicated + 2
  clients (secondary), at 60/15 ms with and without 2 % loss once NP code is touched; M1–M3 tests and scenarios still green.
- Clean (T, S §10) = snapshots equal on every machine (combat state, attributes, positions after the launch settles), no reconciles
  besides forced ones and **the expected one per hit on the victim's client**; host pawn: zero reconciles.
- Epic coding standard. Layer order `GSTNCombatCore` (no GAS/NP/Mover) < `GSTNCombatGAS` (no NP/Mover) < `GSTNCombatNP`; new
  deps mirrored in the `.uplugin`; the project module gets PRIVATE deps only.
- Native tags with `UE_DEFINE_GAMEPLAY_TAG_COMMENT` in the owning module (MT §4.2, §4.4), one doc line each. **Roots native,
  leaves ini** for designer vocabularies (MT §4.2b): `Combat.Hurtbox.*` and `Surface.Type.*` leaves live in
  `Plugins/GSTNCombat/Config/Tags/GSTNCombat.ini`, never named in C++. No tag in any sync state or snapshot (indices only).
- README of each touched plugin updated in the same checkpoint (module table, "Extending in Blueprint", settings).
- **R-B Independence** (M3 plan, MT §3.7), acceptance line of every checkpoint: plugin builds and tests without the project
  module; depends on other plugins only through their `*Core` (documented exception: NP may depend on NP of the same layer);
  no `/Game/` reference, no project ini key or tag; settings in `UGSTN_CombatSettings` (UDeveloperSettings) with defaults in the
  plugin's `Config/`; optional integrations are bridge plugins, never `#if WITH_<PLUGIN>`.
- **Blueprint extension (MT §3.8)**: every behaviour class is `Abstract, Blueprintable, EditInlineNew, DefaultToInstanced` with one
  `BlueprintNativeEvent` taking a const view struct and returning a result struct; hooks inside the simulation are pure (no
  world/actor/component reads, no latent nodes, seeded random only). Per-frame per-entity hooks stay C++.
- **Replaceable content (MT §3.9)**: soft references in data (`TSoftObjectPtr`/`TSoftClassPtr`, primary asset ids), no
  `ConstructorHelpers`, no literal asset path in plugin C++; the standard rules asset is referenced from `UGSTN_CombatSettings`.
- Simulation code: no `IsServer()` branch (CLAUDE.md rule 1); "server" in S means the authority, which is the listen host.
- Doc deltas go to `Saved/Notes/m4-doc-deltas.md` per checkpoint; one docs commit at the end (H13), same rule as M3.
- Test prefixes: `GSTNCombat.Core.Hits.*`, `GSTNCombat.Core.Reaction.*`, `GSTNCombat.Core.Impact.*`, `GSTNCombat.GAS.Damage.*`,
  `GSTNCombat.NP.Hits.*`. Harness scenarios as S §10 in `Tools/GASNPPTest/gasnpp_test.py`.

## 2. Dependencies on M3

| M4 checkpoint | Needs from M3 | Can run in parallel with |
|---|---|---|
| H0 | C0b harness (listen primary, done); C10 `UGSTN_GA_BaseCombat` for spike 3 (montage + pause); C12 movement track for spike 2 | C7–C9 (spikes 1, 2, 4 need only M2 state + harness) |
| H1 | C2 timeline data (`FGSTN_HitboxTrack`, `FGSTN_HurtboxModifierTrack` declared with name/tag targeting, `BakedSockets`), C3b `FGSTN_HitVictimView` placeholder | all of C7–C15 (pure Core) |
| H2 | C3b `UGSTN_CombatRules` shell; C4 attribute set names | C7–C15 |
| H3 | C13 ORG-1: gravity direction exposed by the tracker (only for the NP consumer; the pure math takes `Up` as input) | C7–C15 |
| H4 | C3b (`UGSTN_CombatStep` pipeline as step classes, `IGSTN_CombatTickExtension` hook points incl. "before candidates", `FGSTN_KitExtension`, `FGSTN_RulesExtension`), C5 `ExtensionData` on kit/weapon | C7–C15 |
| H5 | C4 attribute sets, C1 tags module, C5 data (`FGSTN_KitExtension` for `FGSTN_ImpactResponse`) | C7–C15 |
| H6 | C3a fragments + type-id registry, C8 sync state additions and `Requests[4]`, C13 ORG-1 (gravity in the tracker snapshot) | C14, C15 |
| H7 | C8/C9 (`WaitCombatFrames`, hitstop debug path from M0), C10 (montage synced to the combat clock), C12 (`FGSTN_LayeredMove_MoveTrack` freeze-ready) | C14, C15 |
| H8 | C10 (`GA_BaseCombat` cancel windows OnHit/OnBlock, ricochet-outcome flag path, tick order 7.2 step 5 "candidates"), C12 (`MoveOriginTransform` set at move start) | C14 |
| H9 | C10, C12, C13 (`FGSTN_JumpResetRules` react to the launch move as an external launch), R-A `FGSTN_TimelineTail` (C2/C10) for the reaction tail | C14 |
| H10 | H6–H9; fork `UNetworkPredictionLagCompensation` / `RewindActors` (present in `NetworkPredictionWorldManager.h`) | — |
| H11 | H9; C13 launch move type (`FGSTN_LayeredMove_Launch` named by C13 for trampolines) | H10 |
| H12 | C14 dodge shells (timeline abilities with `HurtboxModifier` data, blocked tags), C10 ricochet path, C5 kit fallback for `FGSTN_RicochetResponse` | H10, H11 |
| H13 | C15 test content (`KIT_Test`, `BP_GASNPP_TestCharacter` combat setup) | — |

H1–H5 are headless and can start now, on top of C6. H6+ wait for C8/C10/C12 (sync state, abilities, movement track).

## 3. Checkpoints (in order)

Legend: goal; module; files; tags; tests; acceptance; spec refs; size.

### H0 Spikes (no plugin code; `Saved/Notes/m4-spike-report.md`, to become CM §17) — size M

Both topologies, listen first. Record pass/fail and numbers.

- **H0a cross-entity cost** (S §2.1): c1 hits c2 (host resolves) with a throwaway hit path (M0 hitstop debug button on c1 writes a
  request into c2's state on the authority). Count c2's rollbacks per hit, visible delay on c1 (presentation off), host rollbacks
  (expected 0), bytes/s per client. Stop-and-ask if c2 sees > 1 rollback per hit in the clean case or the host reconciles.
- **H0b freeze** (S §2.2): `FGSTN_LayeredMove_Freeze` prototype (OverrideAll, zero velocity) in Walking, Falling and each project
  wall mode, forced reconciles inside the freeze. Confirms whether a layered move can expose a Mover gameplay tag
  (`UMoverComponent::HasGameplayTag` sources). Stop-and-ask: layered moves cannot carry tags **and** a modifier cannot either
  (then the freeze flag goes into combat sync state and the wall lock reads it through the tracker: a design change for H7).
- **H0c hitstop with a montage** (S §2.3): M0 pause path with a real C10 montage, forced reconcile inside the window. Stop-and-ask
  if the montage position diverges after the reconcile (fork montage-sync fix needed before H7).
- **H0d interpolation buffer** (S §2.4): lower `FixedTickInterpolationBufferedMS` (100) step by step under 60/15 ms, 2 % loss,
  keep the smallest without visible stutter. Output: the value for H10 (ini change needs the owner's OK, §6).
- **H0e lag compensation API**: confirm `UNetworkPredictionLagCompensation` + `RewindActors/UnwindActors` on the test pawn rewind
  Mover transforms in the fork at the needed window (250 ms); measure cost of one rewind of 3 pawns on the host. Stop-and-ask if
  the component is missing or rewinds only physics bodies (H10 then builds its own ring from `FGSTN_PoseHistory`, M8 §3).
- Acceptance: report filed; H0b/H0c/H0e verdicts gate H7/H10. Independence: nothing in plugins.

### H1 Hit geometry and data (`GSTNCombatCore`) — size L

- Files: `Core/Public/Hits/GSTN_HitShape.h` (`FGSTN_HitShape`, `EGSTN_ShapeType`), `GSTN_HitShapeUtils.h/.cpp` (`UGSTN_HitShapeUtils`
  BlueprintPure: `Overlap` for all six pairs, `SweptOverlap` with N = ceil(distance / smallest extent) capped at 4; doubles),
  `GSTN_HitData.h` (`FGSTN_HitData` complete per S §3 incl. `StaggerForceMode/StaggerForce/StaggerForceScale`, `HitstopMode`,
  `Impact`, `ExtensionData`; `FGSTN_HitDataOverride`, `FGSTN_HitVariant`, `EGSTN_SpecialAngle`, `EGSTN_YawSource`,
  `EGSTN_HitstopMode`, `EGSTN_StaggerForceMode`), `GSTN_HitboxTypes.h` (`FGSTN_HitboxSpec`, `FGSTN_HitboxKey`, `EGSTN_HitboxSpace`;
  `FGSTN_HitboxTrack` completed where C2 declared it), `GSTN_HurtboxTypes.h` (`FGSTN_Hurtbox` with `Priority`, `Tags`,
  `ReactionScale`; `UGSTN_HurtboxSetDefinition` with `StaggerThreshold`, `MaxReaction`, `WeightCurve`;
  `FGSTN_HurtboxModifierTrack` completed: `State`, `ArmorThreshold` as `FGSTN_StaggerThreshold { float Value; bool bInfinite }`,
  `AffectedHurtboxes`, `AffectedTags`, `ShapeOverrides`), `Timeline/GSTN_TimelinePlayer` extension: active hitboxes and hurt
  state/overrides per frame (`FGSTN_TimelineFrameResult`, M8 §3 shape; no allocation, caller scratch).
- Hurtbox selection helper (pure): `UGSTN_HitShapeUtils::PickHurtbox(overlapped, set) → index` (highest `Priority`, ties lowest
  index; S §3b item 2, P §3.4). `ApplyVariant(HitData, Variant) → resolved FGSTN_HitData` (override fields, multipliers, extension
  override entries by struct type).
- Tags (`GSTNCombatGAS` tags module is GAS-layer; these are Core vocabulary, so declare in `GSTNCombatCore/GSTN_CombatHitTags`):
  roots `Combat.Hurtbox` (leaves `Feet`, `Head`, `Torso` in the plugin tag ini), `Combat.HitType.{Normal,Grab,Projectile,Unblockable}`
  (native: S names them and code switches on them), `Combat.Relationship.{Self,Ally,Enemy,Neutral}` (native, keys of
  `RelationshipVariants`).
- Tests `GSTNCombat.Core.Hits.*`: `ShapePairs` (every pair: touching, separated by ε, contained), `SweptCatchesFastBox`,
  `SweptSubstepCap`, `KeyInterpolation`, `VariantOverrideAndMultiplier`, `VariantExtensionOverride`, `PickHurtboxPriorityThenIndex`,
  `ModifierTrackByNameOrTag` (union), `BakedSocketLookup`, `TimelinePlayerHitboxesNoAlloc`, `ValidatorHitTypeNonNormalWarns` (OQ gap).
- Acceptance: identical results on two runs with the same inputs (bitwise); no GAS/NP/Mover include in Core (grep gate); structs
  `BlueprintType`, tag fields with `Categories`. R-B: pure.
- Refs: S §3 shapes/hit data/hitbox/hurtboxes, §1b, §3b 2/5. OQ gap "HitType other than Normal": data only, validator warning.

### H2 Reaction model (`GSTNCombatCore`) — size L

- Files: `Core/Public/Hits/GSTN_ReactionTypes.h` (`FGSTN_HitReactionModifiers` identity defaults, `EGSTN_HitReaction
  {StaggerInPlace, PushBack, Launch}`, `FGSTN_ReactionTuning { StaggerPerDamage, StaggerPerKnockback, HitstopPerDamage,
  HitstopPerKnockback = 0, HitstopBase, MaxHitstop, PushBackThreshold = 0, LaunchThreshold = 0, TumbleThreshold, HitstunFactor,
  MinHitstunFrames, PushBackSpeedScale, MaxPushBackSpeed, WeightReference = 100, MinWeight }`, `FGSTN_BakedCurve` (64 × uint16,
  integer interpolation), `FGSTN_KnockbackInput`, `FGSTN_KnockbackResult`, `FGSTN_VictimReactionState` (POD: `uint16
  DamagePercentQ; uint8 WeightScaleQ; uint8 Flags (Armor/Intangible/Invincible window, InHitstop, Tumble); uint8 ExtMask;
  FGSTN_StaggerThreshold EffectiveThresholdQ`), `GSTN_KnockbackModel.h` (`UGSTN_KnockbackModel` abstract, Blueprintable,
  `BlueprintNativeEvent Compute(const FGSTN_KnockbackInput&) → FGSTN_KnockbackResult`, shared rules 1–4 of S §3 in protected
  pure helpers so every model evaluates them once), `GSTN_KnockbackModels.h` (`WeightDriven` standard, `PercentScaling`, `Fixed`,
  `CappedPercent`), `GSTN_HitVictimView.h` (C3b struct completed: M8 §3 fields + `HealthFraction`, `Weight`, `ReactionState`,
  pointer to the snapshot extension POD).
- Rule order inside every model (S §3 "Weight, stagger and juggle"): `Weight × WeightScale` → K → `× KnockbackScale` → stagger test
  (`StaggerForce × StaggerScale ≥ max(0, Threshold)`, `bInfinite` never staggers, `bNoFlinch` same) → tier (`MaxReaction` cap) →
  hitstun `× HitstunScale` (floor `MinHitstunFrames`; locked floor `MinStaggerLockFrames` for StaggerInPlace/PushBack applied in H9)
  → hitstop `× HitstopScale` (Auto from real K and real damage; Override final, clamped to `MaxHitstop`). `KnockbackResistance`
  is applied in H9 to the velocity only.
- Tests `GSTNCombat.Core.Reaction.*` (S §10 list): `StaggerForceDerived`, `StaggerForceOverride`, `StaggerForceScale`,
  `HitstunBoundary` (equal staggers, one unit below does not), `ThresholdInfinite`, `NoFlinch`, `HitstopAutoHandValues`,
  `HitstopOverrideFinal`, `HitstopClamp`, `AttackerHitstopFromSnapshotOnly`, `TierBoundaries`, `MaxReactionCap`,
  `LighterNeverMilder` (monotone in weight), `ZeroThresholdsAlwaysLaunch` (old behaviour), `OwnerTwoHitExample` (hit 1 staggers,
  hit 2 same move still below launch, stronger hit 2 launches), `ModifiersAppliedOnce` (per model), `WeightDrivenHandValues`,
  `PercentScalingHandValues`, `FixedIgnoresPercent`, `CappedPercentCap`, `BakedCurveDeterministic`, `FlatCurveNoChange`,
  `ValidatorPercentModelWithNonFlatCurve`, `ValidatorThresholdOrder`.
- Acceptance: model is a pure const function of (`FGSTN_HitData`, `FGSTN_KnockbackInput`); no victim field other than the input
  changes `AttackerHitstopFrames`. R-B: pure; models Blueprintable.
- Refs: S §3 (knockback model, three dials, rules 1–4, weight/stagger/juggle), DD §2b, §2h, OQ 27/28/35/36.

### H3 DI and impact math (`GSTNCombatCore`) — size M

- Files: `Core/Public/Hits/GSTN_HitMathUtils.h/.cpp` (`UGSTN_HitMathUtils` BlueprintPure: `ApplyDI(Launch, StickOnPlane, Up,
  FGSTN_DIParams)` steps 1–8 of S §3, `LaunchDirection(YawSource, AngleDeg, SpecialAngle, Up, facing, hitbox→victim, hitbox
  velocity)`, `ProjectOnPlane`); `FGSTN_DIParams` (defaults 15°, 0.2, −0.1, 0.98). `Core/Public/Hits/GSTN_ImpactTypes.h`
  (`EGSTN_ImpactOutcome`, `FGSTN_ImpactProfile`, `FGSTN_ImpactResponse` (per surface type: allowed outcomes, `MinImpactSpeed`,
  `Restitution`, `SplatFrames`, `ImpactDamageScale`), `UGSTN_SurfaceImpactTable` DataAsset (rows by `Surface.Type.*` tag, compiled
  to an index table at load), `FGSTN_ImpactInput` (quantised), `FGSTN_ImpactResult`), `GSTN_ImpactMath.h/.cpp`
  (`UGSTN_ImpactMath::Resolve(Input, table, response, profile)`: Forbid removes, Force wins if surface allows or
  `bForceIgnoresSurface`, else victim restricts the surface default; `MinImpactSpeed` else `None`).
- Tags: root `Surface.Type` native in Core (leaves `Wall`, `Glass`, `Crate`, `Trampoline` in the plugin tag ini).
- Tests `GSTNCombat.Core.Hits.DI.*`: `WorkedExamples` (0.4→0.6/0.2, 0→0.2/0, −0.5→−0.7/−0.3, −0.05→0.15/unchanged),
  `LateralRotatesAroundUp`, `OutNeverCrossesHorizontal`, `ElevationClamp`, `StraightUpUsesYawSource`, `TiltedGravity` (Up ≠ Z,
  inverted), `SpeedUnchanged`. `GSTNCombat.Core.Impact.*`: `PrecedenceHitVictimSurface`, `ForbidRemovesForce`,
  `ForceIgnoresSurfaceFlag`, `MinSpeedGivesNone`, `TrampolineRowForcesBounce`, `GoldenMatrix` (resolved outcome matrix printed by the
  validator, compared with a golden file).
- Acceptance: all math takes `Up` as an input (never world Z); doubles; bitwise-equal across runs.
- Refs: S §3 DI, §7b, DD §2c, §2d, §2f, OQ 8/29/30/32.

### H4 Pipeline, rules and generic seams (`GSTNCombatCore`) — size L, two commits (H4a types + steps, H4b seams + BP adapters)

This is the **Core seam checkpoint** for M4b (P §1 items 1, 2 (context half), 4, 5, 6 (declaration), 7).

- Files: `Core/Public/Hits/GSTN_HitResolution.h` (`FGSTN_HitResolutionContext { incoming resolved hit; FGSTN_HitVictimView Victim;
  FGSTN_HitReactionModifiers Reaction; uint8 HurtboxIndex; outcome { bHit, bBlocked, bParried, bClank, Damage, FGSTN_KnockbackResult,
  MeterGains, Events (tags), Effects (soft GE classes), Requests }; TArray<FInstancedStruct> OutcomeExtensions; }`, `FGSTN_HitContextView`
  (const BlueprintReadOnly view of the context), `UGSTN_HitResolutionStep` (derives from C3b `UGSTN_CombatStep`; entry
  `BlueprintNativeEvent Resolve(const FGSTN_HitContextView&, FGSTN_StepResult&) → EGSTN_StepResult`; a step writes only through
  `FGSTN_StepResult` (modifier deltas, outcome fields), merged by the runner so Blueprint steps stay pure)), `GSTN_HitSteps.h`
  (`TeamFilter`, `HurtState`, `HurtboxReaction`, `HealthWeight`, `ComputeKnockback`, `ApplyDamage` (writes `Outcome.Damage` and the
  damage request; the GE class comes from the rules asset), `Hitstop`, `MeterGain`; documented placeholders `Shield`, `Parry`,
  `PartResolve` as comments and validator-known names, not classes), `GSTN_HitRunner.h` (`FGSTN_HitPipelineRunner::Run(rules, ctx)`),
  `GSTN_HitExtensions.h` (`FGSTN_HitExtension` base, `FGSTN_HitExtensionOverride` entry for `FGSTN_HitDataOverride`;
  `IGSTN_OutcomeExtensionApplier` + registry by struct type (`FGSTN_OutcomeApplierRegistry`, static, deterministic order by type
  name hash), `UGSTN_OutcomeApplier` Blueprintable adapter (instanced in the rules asset); `IGSTN_AttackerOutcomeExtension::Evaluate(const
  FGSTN_HitContextView&) → EGSTN_AttackerHitOutcome {Normal, Ricochet}` + registry, `UGSTN_AttackerOutcome` BP adapter;
  `FGSTN_HitGroupMask` return of the "before candidates" tick-extension point (C3b interface gains the out-parameter, default empty)),
  `GSTN_CombatRules.h` (C3b asset completed: `Pipeline` (instanced steps), `KnockbackModel`, `DamageMode {Percent, Health,
  PercentCapped}`, `bFriendlyFire`, `HittableRelationships`, `RelationshipResolver` (`UGSTN_RelationshipResolver` Blueprintable,
  default: self / same team Ally / NoTeam Neutral / else Enemy), `bUseLagCompensation`, `MaxRewindMs = 250`, `DI`, `StaggerThreshold =
  0`, `AirborneJuggleScale = 1`, `MinStaggerLockFrames = 31`, `DefaultWeightCurve`, `SurfaceImpactTable` (soft), `DamageEffect`,
  `MeterGainEffect`, `KnockbackEffect`, `FreezeEffect` (soft classes, filled by H5), `OutcomeAppliers`, `AttackerOutcomes`,
  `RulesExtensions`), `GSTN_CombatRulesValidation.h` (`IsDataValid`: step order, unknown step reported and skipped, `HealthWeight`
  and `PartResolve` both present, hurtbox `ReactionScale` and a part reaction both set, percent model with non-flat curve,
  threshold order, `MaxHitstop` exceeded by an override; reaction-matrix printer "per victim weight, which hits stagger and which
  tier").
- Standard rules asset `GSTN_CombatRules_Standard` (data, `Plugins/GSTNCombat/Content/Rules/`): `WeightDriven`, `HealthWeight`
  in the pipeline, `PushBackThreshold = 0.3 × Tumble`, `LaunchThreshold = 0.6 × Tumble`, friendly fire off, `HittableRelationships =
  {Enemy, Neutral}`; soft-referenced from `UGSTN_CombatSettings::DefaultRules`.
- Tests `GSTNCombat.Core.Hits.Pipeline.*`: `StepOrderFromAsset`, `UnknownStepSkippedReported`, `StopHalts`, `FakeStepWritesModifiers`
  (modifiers reach the model exactly once), `FakeApplierRecordsCalls` (no plugin present), `AttackerOutcomeDefaultNormal`,
  `FakeAttackerOutcomeRicochet`, `HitGroupMaskDefaultEmpty`, `ExtensionDataUnknownTypeIgnored`, `HurtboxReactionResolution`
  (hurtbox → entity → rules; Armor window raises to the larger), `HealthWeightUsesDamageBeforeHit`, `TwoEntriesSeeEachOther`
  (drain order), `RelationshipResolverDefault`, `BlueprintStepRoundTrip` (a test BP step compiled in test content, H13),
  `ValidatorRulesMatrixGolden`.
- Acceptance (S §12 4a): seams exist and are inert; every default is the identity; `GSTNCombat` tests pass with no optional plugin.
  R-B: pure, no NP type in Core; adapters Blueprintable.
- Refs: S §3 pipeline/rules, §3b 1–7 + Blueprint forms, MT §5b, P §1, M3 plan C3b.

### H5 GAS: damage, attributes, effects, events (`GSTNCombatGAS`) — size M

- Files: `GAS/Public/Attributes/GSTN_HealthAttributeSet.h` (coordinator decision "one set per concern": `Health`, `MaxHealth`,
  `DamagePercent`, `OutgoingDamageMultiplier`, `IncomingDamageMultiplier`, `KnockbackResistance`, `HitstunResistance`; clamps in
  `PreAttributeChange`; `PercentCapped` clamp at `MaxHealth` as cap), `Effects/GSTN_GE_Damage.h` + `GSTN_DamageExecution.h`
  (SetByCaller `Combat.SetByCaller.Damage`, multipliers, `DamageMode` from the rules carried in the effect context), `Effects/GSTN_GE_MeterGain.h`,
  `GSTN_HitEffectContext.h` (`FGSTN_HitEffectContext : FGameplayEffectContext`, `NetSerialize`: hit id (timeline index + hitbox
  index + start frame), location, relationship, `Reaction`, `bStaggered`), `Events/GSTN_HitEvents.h` (`FGSTN_HitEventPayload` for
  `Combat.Event.Hit.Received` gameplay event: `Reaction`, `bStaggered`, hurtbox index, relationship, `KOReason`),
  `GSTN_CombatRulesComponent.h` (`UGSTN_CombatRulesComponent` on GameState: replicated soft rules reference, resolved before the
  first NP tick on every machine; host applies without OnRep through the same `ApplyRules()`).
- Tags (`GSTNCombatGAS`): `Combat.SetByCaller.Damage`, `Combat.SetByCaller.Meter` (if not in C4), `Combat.Event.{HitLanded,HitReceived,KO}`
  (outbox, CM §5.6), `Combat.Event.Hit.Received` (gameplay event), `Combat.Event.Impact` root (leaves `Splat/Bounce/Break` native,
  H11), `Combat.State.{Hitstun,Tumble,Hitstop}`, `GameplayCue.Combat.Hit` root (leaf cues in ini), `Combat.KOReason.{Health,Zone}`
  (event tags only). `Input.BufferEvent.HitReceived` belongs to the input vocabulary: one-line addition in `GSTNInputCore` tags
  (coordinator decision 1; ask, §6).
- Tests `GSTNCombat.GAS.Damage.*` (transient stock ASC): `PercentModeAdds`, `HealthModeSubtracts`, `PercentCappedClampsAtMax`,
  `MultipliersApplied`, `EffectContextNetSerializeRoundTrip`, `MeterGainSetByCaller`, `RulesComponentHostAppliesWithoutOnRep`,
  `TagsRegisteredAndDocumented`.
- Acceptance: GAS plugin builds without NP; KO condition itself is in H9 (simulation), here only the attribute cap. R-B: tags
  native roots only, GE classes soft-referenced from the rules asset.
- Refs: S §4, §3 rules (`DamageMode`, death conditions), CM §5.6, OQ 1/4 (answered: both via PercentCapped; KO by health or zone).

### H6 NP: sync state, world subsystem, snapshot, inbox (`GSTNCombatNP`) — size L

Second seam checkpoint for M4b (P §1 item 2 inbox half, item 7 snapshot extension; wire format).

- Files: `NP/Public/Simulation/GSTN_CombatSimTypes.h` additions (S §11): `MoveOriginTransform` (quantised), `HitTargets[8] { EntityId,
  TimelineStartFrame, HitGroup }` ring with `HitTargetsHead`, `HitConfirmFlags`, `HitstopStartFrame/EndFrame` (real),
  `HitstunEndFrame`, `PendingLaunch` (int16 × 3, cm/s / 4), `PendingReaction` (2 bits packed with flags), `SDICount` (reserved),
  `ImpactEndFrame`; `EGSTN_ActionState` gains `Hitstun`, `Tumble`, `Splat`; `Requests` types: `ApplyDamage`, `ApplyEffect`,
  `Cue`, `Freeze`, `Knockback`, `PushBack`, `MeterGain`, `ImpactSurface`. All serialized only when `Frame == state frame`;
  idle cost ≈ 0 via end-frame stamps. `NP/Public/World/GSTN_CombatWorldSubsystem.h` (registry `{ EntityId, weak component, Team,
  hurtbox set }` sorted by id; `RegisterProvider(IGSTN_HittableProvider*)`, M8 §9 shape; `FGSTN_StepSnapshot { Frame; entries {
  transform, velocity at end of F-1 from the tracker, facing, Up (= −GravityDir, ORG-1), hurt state + active overrides, team,
  bInHitstop, action state, FGSTN_VictimReactionState, TArray<FInstancedStruct> Extensions (FGSTN_SnapshotExtension POD) };
  uniform grid, cell = 2 × largest hurtbox extent (default 400 cm) }`; built by the first combat tick of F when `Snapshot.Frame != F`
  or invalidated; every component's `RestoreFrame` invalidates it (development assert that it is called for every instance);
  interpolated proxies contribute their presentation state). `NP/Public/Simulation/GSTN_HitInbox.h` (`FGSTN_HitInboxEntry` as an
  inbox kind of the C3 inbox registry: `{ Frame, AttackerId, resolved FGSTN_HitData, HitLocation, HitboxVelocity, AttackerYaw,
  Relationship index, uint8 HurtboxIndex, uint8 AttackerHitboxIndex }`; written only where the victim ticks; drained at `Frame + 1`
  in (AttackerId, hitbox) order; rollback drops entries with `Frame ≥` restored frame).
- Tests `GSTNCombat.NP.Hits.State.*`: `RoundTrip`, `IdleZeroCost` (bytes unchanged when no hit), `HitTargetsRingWrap`, `Interpolate`,
  `ShouldReconcile`. `GSTNCombat.NP.Hits.Snapshot.*`: `BuiltOncePerStep`, `InvalidatedByRestoreFrame`, `EntriesAreEndOfPrevFrame`,
  `ProviderOrderDeterministic`, `BroadphaseCandidates`. `GSTNCombat.NP.Hits.Inbox.*`: `DrainOrder`, `RollbackTrim`,
  `OnlyWhereInstanceTicks`.
- Scenarios: M2/M3 regressions only (no hits yet); byte count per state printed (host upload budget, S §9b).
- Acceptance: no behaviour change without hits; +0 bytes idle. R-B: NP depends on `GSTNInputNP` only through the documented
  exception; no tags in state.
- Refs: S §5, §7 inbox, §11, §3b 2/7, M8 §3/§9, MT §5b.

### H7 NP: hitstop and freeze on both sides (`GSTNCombatNP`, project wall extension point) — size M

- Files: `NP/Public/Mover/GSTN_LayeredMove_Freeze.h` (`OverrideAll`, zero velocity, stores the incoming velocity, restores it on
  finish unless `bZeroVelocityOnFinish`; carries the Mover tag `Combat.Mover.Frozen` per H0b, else queues `FGSTN_FrozenModifier`),
  `GSTN_FreezeMoveCreator` (GE + creator: queued once with a duration), combat tick: `MovePausedFrames++` per hitstop frame,
  `PausedFramesTotal`, buffer `PausedStart/PausedEnd` to `FGSTN_MatchParams`, resolver skipped during hitstop, montage pause/resume
  at `HitstopEndFrame` (M0 path), `FGSTN_LayeredMove_MoveTrack` does not advance while frozen (C12 flag). Project:
  `UGSTN_WallMovementModeBase::IsMovementLocked(const FMoverTickStartData&)` (virtual, default: Mover has `Combat.Mover.Frozen`);
  locked = skip `CheckForWallExit`, `TryWallJump`, `TryDetachByPullingAway`, no timer advance. **Owner OK needed** (§6).
- Tags (`GSTNCombatNP`): `Combat.Mover.Frozen`.
- Tests `GSTNCombat.NP.Hits.Hitstop.*`: `PausedFramesCount`, `BufferAgeExcludesHitstop`, `FreezeRestoresVelocity`,
  `FreezeZeroOnFinishFlag`. Scenarios: `scenario_hitstop_montage` (forced reconcile in the window, montage position equal),
  `scenario_hit_on_wall` first half (freeze while wall sliding, wall timers stopped), M0 `combat_spike` hitstop button rewired to
  the real window.
- Acceptance: freeze works in Walking, Falling and every project wall mode; host and client positions equal after the window.
- Refs: S §8, CM §12.6, H0b/H0c.

### H8 NP: attacker side (`GSTNCombatNP`) — size L

- Files: `NP/Private/Simulation/GSTN_CombatTick_Attacker.cpp` (tick step after M3 7.2 step 4, before candidates): 1) active hitboxes
  from the timeline player (paused frames excluded; groups in the `HitGroupMask` of the "before candidates" extension skipped);
  world shapes by space (`Root`, `BakedSocket`, `WorldAtMoveStart` = `MoveOriginTransform`, `Projectile` reserved); 2) broadphase;
  3) relationship (resolver), `HittableRelationships`, `bFriendlyFire`, variant (`bIgnore`, `ShapeScale`); 4) overlap (swept if
  flagged) against hurtboxes with overrides, `Intangible` skipped, `PickHurtbox`; 5) `HitTargets` once per group per execution
  (overflow: oldest entry replaced, logged once; `MaxTargetsPerGroup` on the hitbox, default 8, OQ gap); 6) per new hit: attacker
  outcome (`Normal`/`Ricochet` via H4 registry with the snapshot's `FGSTN_VictimReactionState`), confirm flags (`OnHit`; `OnBlock`
  false until M6), attacker hitstop window from the model's `AttackerHitstopFrames` (snapshot view), meter request, deliver to the
  victim inbox (also on ricochet, for cue/event). `GA_BaseCombat`: cancel windows `OnHit` go live (C10 flag), ricochet flag consumed
  (response in H12). Debug draw cvars `gstn.Combat.Debug.Hitboxes/Hurtboxes` (`NP/Private/Debug/`).
- Deterministic attacker order: candidates sorted by `EntityId`, hitboxes by index; the victim inbox sorts again, so trades are
  order-independent (OQ gap "ordine deterministico").
- Tests `GSTNCombat.NP.Hits.Attacker.*`: `OncePerGroupPerExecution`, `RingOverflowReplacesOldest`, `MaskSkipsGroup`,
  `IntangibleNotHit`, `InvincibleRegistersNoDelivery` (hit registers for confirm/hitstop; inbox entry flagged), `RelationshipVariant`,
  `SelfExcludedUnlessVariant`, `WorldAtMoveStartUsesOrigin`, `AttackerHitstopFromSnapshotView`.
- Scenarios (attacker half, victim resolves in H9; run both together at H9): `scenario_hit_basic` attacker checks (c1 predicted hitstop,
  cancel-on-hit, zero rollbacks on c1 in the clean case), `scenario_hit_trade` (both hit on the same frame), `scenario_friendly_fire`
  (off → no hit; on → variant), host as attacker (`scenario_hit_host` half).
- Acceptance (S §12 3): the three hitbox spaces hit where the debug draw shows them on the host and on clients.
- Refs: S §6, §3b 3c/6, §9b.

### H9 NP: victim side, KO, events — **seams complete (SC)** (`GSTNCombatNP`) — size L

After H9 every seam M4b needs exists and is exercised: Core (H4), wire (H6), appliers called at outcome apply and attacker outcome
at step 6 (H8/H9), events with hit type and relationship (H9).

- Files: `NP/Private/Simulation/GSTN_CombatTick_Victim.cpp` (drain at F+1 per S §7; pipeline run through `FGSTN_HitPipelineRunner`
  with the victim view filled from the attribute snapshot at end of F-1 plus earlier entries of the same drain (`HealthWeight`
  reads damage before this hit, OQ 35); outcome apply: staggered → `ActionState = Hitstun`/`Tumble`, hitstop window,
  `HitstunEndFrame`, `PendingReaction`, `PendingLaunch`, cancel current move; not staggered → hitstop window + flinch only, move
  resumes; `PushBack` needs ground (airborne or wall mode at end of F-1 → promoted to `Launch`); appliers run per
  `OutcomeExtensions` entry; requests: damage GE, extra GEs, cue, freeze, buffer event `Input.BufferEvent.HitReceived`, gameplay
  event `Combat.Event.Hit.Received`; hitstop end by `PendingReaction`: `Launch` → DI from this frame's stick with `Up` from the
  tracker → `FGSTN_LayeredMove_Knockback { Velocity, DecayPerFrame, GravityScale, ElapsedFrames }` forcing Falling, `KnockbackResistance`;
  `PushBack` → `FGSTN_LayeredMove_PushBack` (ground mode kept, stops at wall/ledge, ends at zero speed or `HitstunEndFrame`);
  `StaggerInPlace` → freeze ends with zero horizontal velocity; hitstun end → `Actionable` (or `Tumble` until landing); reaction
  tail: `MinStaggerLockFrames` floor, reaction montage tail cancellable by anything after `HitstunEndFrame` (R-A `FGSTN_TimelineTail`);
  victim already in hitstun/hitstop: the new entry recomputes the tier with no special case (OQ 27), sub-threshold entry during
  hitstun changes nothing (assumption A-27). `NP/Public/Mover/GSTN_LayeredMove_Knockback.h`, `GSTN_LayeredMove_PushBack.h` + creators
  (GE path). `NP/Public/World/GSTN_KOZone.h` (`AGSTN_KOZone`: the combat tick reads the sim position against registered zone bounds
  at end of F-1; `Combat.Event.KO` with `KOReason = Zone`; `Health` cap → `KOReason = Health`). Outbox (`UGSTN_SimEventSubsystem`,
  CM §5.6): `HitLanded`, `HitReceived`, `KO` from `FinalizeFrame` state on the authority, tags: command, move, hit type,
  relationship, `bStaggered`, reaction; **host fires each once** (host-local listeners use the outbox only). Tag projection
  `Combat.State.{Hitstun,Tumble,Hitstop}`. Dodge abilities' blocked tags now take effect (C14 data).
- Tests `GSTNCombat.NP.Hits.Victim.*`: `DrainAtNextFrame`, `TradeBothResolve`, `StaggeredSetsStateAndWindows`, `NotStaggeredKeepsMove`,
  `AppliersCalledPerExtension`, `PushBackPromotedWhenAirborne`, `HitstopEndLaunchWithDI`, `PushBackStaysGrounded`,
  `StaggerInPlaceZeroVelocity`, `SecondHitRecomputesTier`, `SubThresholdDuringHitstunNoChange`, `KOByHealthCap`, `KOByZone`,
  `OutboxOncePerHitOnHost`, `TailCancelAfterHitstunEnd`, `RollbackReDelivers`.
- Scenarios (full, listen + dedicated, 60/15, ±2 %): `scenario_hit_basic`, `scenario_hit_trade`, `scenario_friendly_fire`,
  `scenario_stagger_threshold`, `scenario_reaction_tiers`, `scenario_di` (tilted-gravity variant: Up ≠ Z), `scenario_hit_host`
  (host attacker, host victim, client against host: states equal, host zero rollbacks), `scenario_ko_zone`, `scenario_dodge_blocked_in_hitstun`
  (C14, now real).
- Acceptance (S §12 1, 2, 4c): one rollback per hit on the victim's client at most; hitstun by stagger force vs the hurtbox's
  threshold; tier by weight and `MaxReaction`; friendly fire changed by the rules asset only (4). R-B: no project type; KO zone in
  the plugin.
- Refs: S §7, §3 rules 2–4, §3b 3, DD §6c, §2h, CM §5.6, OQ 25/26/27.

### H10 Lag compensation and presentation (`GSTNCombatNP`) — size M

- Files: `NP/Private/Simulation/GSTN_LagCompensation.cpp` (authority with a remote-controlled attacker and `bUseLagCompensation`:
  `RewindActors(attacker, interpolation time of its last input)` capped at `MaxRewindMs`, per-attacker snapshot overlay (transforms
  + `FGSTN_VictimReactionState` from a per-entity history ring of `MaxRewindMs` frames kept by the world subsystem), `UnwindActors()`
  after; host pawn and AI never rewind), `NP/Public/Presentation/GSTN_HitPresentationComponent.h` (attacker's machine: on a new
  predicted `HitTargets` entry in `FinalizeFrame`, flash parameter, `ExecuteGameplayCueLocal`, mesh pause for the hitstop length,
  predicted victim reaction (tier, frames, launch direction without DI) as a short cosmetic extrapolation blended onto the
  authoritative motion; cancelled when the entry disappears; real cue deduplicated by hit id). `FixedTickInterpolationBufferedMS`
  set to the H0d value (ini, owner OK).
- Tests `GSTNCombat.NP.Hits.LagComp.*`: `OverlayNotSharedSnapshot`, `ReactionStateRewoundWithTransform`, `HostNeverRewinds`,
  `CapAtMaxRewind`, `PresentationCancelledOnCorrection`, `CueDedupByHitId`.
- Scenarios: `scenario_hit_basic` with presentation on, at 2 % loss (c1's view of c2 reacts immediately; corrected entries cancel
  effects); `scenario_hit_host` (client vs host: host victim authoritative, zero host rollbacks; host vs client: client sees it
  ~RTT/2 + buffer later, logged); cross-entity numbers vs H0a; count of hits on the local player arriving as corrections (CM §12.7
  decision input, S §13).
- Acceptance: attacker hitstop predicted = authority's value in the clean case (same rewound view); mismatch only when the victim's
  state changed in the window (logged, no correction). R-B: fork component usage documented in README; dedicated still works.
- Refs: S §9, §9b, §3 hitstop "which knockback", CM §12.7, OQ 15/16/17/24/28.

### H11 Surface impacts and trampolines (`GSTNCombatNP`, optional `GSTNLevelObjects`) — size M

- Files: `GSTN_LayeredMove_Knockback` gains the minimal contact probe writing `FGSTN_TumbleContactSyncState { kind, normal, impact
  speed, contact frame, uint8 SurfaceTypeIndex }` (M6's modifier is the same record); `UPhysicalMaterial → Surface.Type.*` table
  baked at load in the world subsystem (actor override via `IGSTN_ImpactSurface::GetSurfaceType()`); combat tick reads the record at
  end of F-1 and applies `UGSTN_ImpactMath::Resolve` (hit profile from the inbox entry kept in state as `ImpactProfileIndex`, victim
  response from the kit extension `FGSTN_ImpactResponseExtension : FGSTN_KitExtension` or the rules): `Splat` (pinned, `ImpactEndFrame`,
  damage + stun, `ActionState = Splat` → `Actionable`), `Bounce` (reflected velocity × `Restitution`, stays `Tumble`), `BreakSurface`
  (request `ImpactSurface` → `IGSTN_ImpactSurface::Break()` applied outside the simulation at `FinalizeFrame`, victim keeps
  `ResidualSpeedScale`); event `Combat.Event.Impact.{Splat,Bounce,Break}`; `bNoTech` flag on hit data reserved for M6. Trampoline:
  `Surface.Type.Trampoline` row forces `Bounce` for tumbling victims (no special case); the non-tumbling launch (`AGSTN_Trampoline`,
  `FGSTN_LayeredMove_Launch` through the GE path, direction = trampoline normal) lives in a small optional plugin `GSTNLevelObjects`
  (depends on `GSTNCombatNP`): **H11b, only if the owner wants it in M4** (§6; otherwise the row + launch move type only).
- Tests `GSTNCombat.NP.Hits.Impact.*`: `ContactRecordWritten`, `ResolvedOneFrameAfterContact`, `SplatEndsActionable`,
  `BounceKeepsTumble`, `BreakRequestOutsideSim`, `ForcedAndForbiddenByHitData`, `MaterialMappingBaked`.
- Scenarios: `scenario_impact_wall` (splat, bounce, forced and forbidden by hit data; glass breaks), `scenario_hit_on_wall` second
  half (launch into the wall from the wall mode), `scenario_trampoline` (H11b only).
- Acceptance (S §12 4b second half): outcomes follow the precedence of §7b; nothing reads live components in the decision.
- Refs: S §7b, M6 §6 contact record, DD §4, §2d, OQ 20/21/31.

### H12 Base dodges, ricochet response, dodge gating (data + `GSTNCombatNP`) — size S/M

- Base PvE dodges are C14 kit data: ground dodge `HurtboxModifier` window `Intangible`, `AffectedTags = Combat.Hurtbox.Feet`, frames
  = take-off to 40 % of the airborne frames (asset default, not code); air dodge same rule (A-22). Blocked tags `Combat.State.Hitstun`,
  `Combat.State.Hitstop` on the abilities' data. No rolling mode, no spot dodge, no generic roll (DD §6, §8).
- Ricochet: `GA_BaseCombat` consumes the H8 flag: ends the timeline, plays `FGSTN_RicochetResponse` (per attack, weapon, kit fallback
  from C2/C5): `RecoilMontage` synced to the combat clock, `RecoilFrames` locked, cancellable tail, optional push-back movement
  track, cue; no hitstop, no confirm, no meter on the attacker. Confirmed by the host from the same rewound view (H10).
- Tests: `GSTNCombat.NP.Hits.Dodge.FeetWindowFromData`, `...RicochetEndsTimelineIntoRecoil`, `...RecoilTailCancellable`.
- Scenarios: `scenario_dodge_feet` (feet missed early, feet hit late, torso hit throughout), `scenario_air_dodge_feet`,
  `scenario_ricochet` (fake attacker-outcome extension in test content forces `Ricochet`: recoil on the attacker, victim gets cue only,
  no hitstop on either side; host and client equal), `scenario_kit_escape_in_hitstun` (a test kit without the blocked tags dodges in
  hitstun).
- Acceptance (S §12 4b): only the feet, only the initial part, window is kit data; a designer reshapes it without a build.
- Refs: S §1b, §3b 3c, DD §6, §2c, §2e, §2g, OQ 10/22/23/33/34.

### H13 Content, tools, harness, validators, docs — size M

- `Tools/GAS/make_hurtbox_set.py` (Physics Asset bodies → `UGSTN_HurtboxSetDefinition`, tags per body name map),
  `Tools/GAS/bake_sockets.py` (samples the montage at the NPP tick rate → `BakedSockets`; replaced by M5), test move with a root
  hitbox, a baked-socket hitbox, a `WorldAtMoveStart` hitbox and an `Ally` variant (half shape, no hitstun); three test hurtbox sets
  (light / medium / heavy weight, one high-threshold hurtbox); example Blueprints for each extension class (`BP_Step_Example`,
  `BP_KnockbackModel_Example`, `BP_OutcomeApplier_Example`, `BP_AttackerOutcome_Example`, `BP_RelationshipResolver_Example`) under
  `/Game/GASNPPTest/Combat/`; test map with a wall, a glass panel, a KO zone (and a trampoline if H11b). Validators' matrix printers
  wired to `IsDataValid` output. Full harness (both topologies, every scenario of H7–H12, with and without loss) + all `GSTNCombat.*`.
  Docs commit: CM §12 (hits), §17 (H0 results), S corrections found while implementing, READMEs ("Extending in Blueprint", settings,
  fork requirements), plugin tag ini documented.
- Acceptance (S §12 1, 5): everything green; Blueprint-only authoring of a new step/model/applier verified; CM updated.

## 4. Spikes first (H0) and stop-and-ask conditions

| Spike | Gates | Stop and ask when |
|---|---|---|
| H0a cross-entity cost | H9 acceptance 2, H10 | victim client > 1 rollback per hit in the clean case; host reconciles at all |
| H0b freeze + Mover tag | H7, wall lock design | neither layered move nor modifier can expose a tag (freeze flag moves to combat state; wall lock reads the tracker) |
| H0c hitstop with montage | H7 | montage position diverges after a forced reconcile (fork fix first) |
| H0d interpolation buffer | H10 ini value | no value below 100 ms is smooth at 2 % loss (keep 100, note in CM §17) |
| H0e lag-compensation API | H10 | fork component missing or rewinds the wrong thing (build own `FGSTN_PoseHistory` ring, +1 size) |

Other stop-and-ask points: C3b's step base class name (`UGSTN_CombatStep`) vs S `UGSTN_HitResolutionStep` (H4 derives; rename only
if the coordinator prefers one class); `Input.BufferEvent.HitReceived` lives in the input plugin (H5); the wall-movement extension
point (H7); any new Mover mode (never: the plan adds none).

## 5. Assumptions for unanswered owner questions

| OQ id | Assumption planned on | Checkpoint | Cost to change |
|---|---|---|---|
| M4.9 | `YawSource` default `AttackerFacing`, `SpecialAngle = None`; per hitbox data | H1 | data default, free |
| M4.12 | friendly fire **off** in the standard rules; `Ally` variant example in test content | H4, H13 | rules asset, free |
| M4.13 | heals/buffs on allies are not hits in M4 (no pipeline path; `HitType` ≠ Normal is data only, validator warns) | H1, H4 | a `Support` step later, S |
| M4.14 | `HittableRelationships = {Enemy, Neutral}` by default; AI vs AI ordinary hits allowed by the same rule (teams decide); `NoTeam` = Neutral | H4 | rules asset, free |
| M4.15 | `bUseLagCompensation = true`, `MaxRewindMs = 250` (M8's ring size) | H4, H10 | rules asset, free |
| M4.16 | no forward prediction; hits on a local client victim are corrections; counted in H10 | H10 | later milestone |
| M4.17 | presentation effects stop early on correction; the attacker's already-applied predicted hitstop is corrected by the normal reconcile (no extra smoothing) | H10 | S (cosmetic blend) |
| M4.18 | trade = both hits resolve; no clank/priority step in M4 (`Priority` stored, unused) | H9 | a `Clank` step, M |
| M4.19 | minimum presentation: material flash, local cue, mesh pause; no camera shake, damage numbers or controller rumble | H10 | content, free |
| M4.20/21 | three-layer precedence hit → victim → surface; "destruction" = surface breaks; victim destruction only via `ImpactDamage` reaching the cap | H3, H11 | M (new outcome) |
| M4.22/23 | air dodge feet only, `Intangible` state; both are asset defaults | H12 | data, free |
| M4.24 | host advantage accepted; `MaxRewindMs` is the fairness knob; no `bHostHitFeelParity` | H10 | S (presentation delay) |
| M4.25 | players use the same pipeline and tiers; player hurtbox set threshold 0 and `MaxReaction = Launch` | H4, H13 | data, free |
| M4.26 | push back: speed = min(K × `PushBackSpeedScale`, `MaxPushBackSpeed`), fixed `DecayPerFrame`, stops at walls and ledges, promoted to `Launch` when airborne/on a wall | H2, H9 | S (move tuning) |
| M4.27 (open half) | a sub-threshold hit during hitstun changes nothing (no extension, no decay); no hitstun decay in M4 | H9 | S (a decay rule in the model) |
| M4.30 | sharp `UpwardThresholdDot = −0.1`, tunable; no transition zone (M6 curve) | H3 | data, free |
| M4.31 | trampoline bounce fixed or speed-scaled along the normal; no jump-hold or fast-fall interaction; `GSTNLevelObjects` only if asked (H11b) | H11 | M (kit interaction) |
| M4.32 | every "up" uses the victim's gravity of the current frame; DI uses the gravity at `HitstopEndFrame` | H3, H9 | S |
| M4.33 | ground dodge not jump-/air-cancellable before its tail | H12 | data, free |
| M4.34 | feet window = take-off to 40 % of the airborne frames | H12 | data, free |
| M4.36 | yes: `WeightDriven` standard, percent models need a flat curve (validator) | H2, H4 | data |
| Gap: targets per hit | `MaxTargetsPerGroup` per hitbox (default 8 = ring size); ring overflow replaces the oldest, logged once | H8 | S |
| Gap: constants | starting values in the standard rules asset (`TumbleThreshold` 1800 cm/s, `HitstunFactor` 0.4 frames per cm/s... all in data, listed in H13's doc delta for tuning) | H4 | data, free |
| Gap: attacker hit or cancelled on the same frame it hits | the hit is already in the victim's inbox (written at F); the attacker's own reaction resolves at F+1: both land (true trade) | H9 | S |
| Gap: victims in special states | hitstun/tumble: recompute (OQ 27); hitstop: entry drained normally, new window replaces; wall mode: `PushBack` → `Launch`; dodge: intangible skip or invincible register | H8, H9 | S |
| OQ 1/4 open halves | cap values per entity type are hurtbox-set / rules data; after-KO behaviour (respawn, spectator) is out of M4: the event only | H5, H9 | game code |

## 6. Blueprint / shared-asset edits that need the owner's OK before they happen

- `BP_GASNPP_TestCharacter`: hurtbox set reference, `UGSTN_HitPresentationComponent`, `UNetworkPredictionLagCompensation`
  (fork component), test move assets (H8, H10, H13). Via `Tools/GAS/setup_gas_assets.py` with backup, as in M2/M3.
- Wall movement core: `UGSTN_WallMovementModeBase::IsMovementLocked` extension point in the project module (H7; no behaviour change
  when unlocked). Touches the wall-movement core, per wall-movement.md.
- `Plugins/GASNPP/NetworkPrediction/Config/DefaultNetworkPrediction.ini`: `FixedTickInterpolationBufferedMS` to the H0d value (H10).
- `GSTNInputCore` tags: `Input.BufferEvent.HitReceived` (one native line, H5), per coordinator decision 1.
- Game mode / GameState Blueprint: `UGSTN_CombatRulesComponent` (H5), next to the C7 loadout wiring.
- Test map under `Content/GASNPPTest/Combat/` (new file): walls, glass panel with `IGSTN_ImpactSurface`, KO zone; physical material
  assets for `Surface.Type.*` (H11, H13). `Content/Levels/DefaultLevel.umap` untouched.
- Optional plugin `GSTNLevelObjects` with `AGSTN_Trampoline` (H11b): only if the owner wants trampolines in M4.
- Standard rules asset and plugin tag ini (`Plugins/GSTNCombat/Config/Tags/GSTNCombat.ini`) are new plugin files, not project edits.

## 7. Risks and parallelisation

Risks (S §13 plus plan-specific):

- Seams slipping (S §13): H4/H6/H9 are acceptance items; the parts plan depends on **H9 = SC**, and may start Core-only work after H4.
- `RestoreFrame` not called for every instance on rollback → stale snapshot: development assert in H6; H0a watches it.
- Attacker hitstop vs victim hitstop drift when the victim's state changes inside the window: accepted by S §3 (logged in H10).
- Host budget (S §9b): per-hit cost measured on the host configuration in H8–H10; swept overlaps opt-in per hitbox.
- Reaction constants explosion: identity defaults, thresholds on the hurtbox set and the rules, matrix validator (H4, H13).
- C3b base class / C8 request array shape may differ from this plan's names once C7–C10 land: H6/H8 adapt names, not design.
- The wall lock extension point is project code: if refused, hitstop on walls degrades to "wall exits possible during freeze" (scenario
  `scenario_hit_on_wall` would be marked known-failing, not hidden).

Parallel threads (independent commits, no shared files):

| Thread | Checkpoints | Notes |
|---|---|---|
| A (Core, headless) | H1 → H2 → H4a | starts now on C6 |
| B (Core math, headless) | H3 | parallel with A; merges before H4b |
| C (GAS, headless) | H5 | after C4; parallel with A/B |
| D (NP) | H6 → H7 → H8 → H9 | after C8/C10/C12; H7 needs H0b/H0c; H6 can start on C8 while A finishes |
| E (editor) | H0 | needs the editor + harness; H0a/H0d need only M2 state, H0c needs C10 |
| F (after SC) | H10 ∥ H11 ∥ H12 | three threads after H9; H13 last |
| Parts plan (other writer) | Core steps after H4, NP after H9 (SC) | `PartResolve` step, `FGSTN_PartsSnapshotExt`, part fragment ids |
