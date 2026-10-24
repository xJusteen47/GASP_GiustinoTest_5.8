# M4 doc deltas (for the final docs commit)

Same use as `m3-doc-deltas.md`: what the code did differently from the M4 plan (`m4-hits-plan.md`) and the spec
(`combat-m4-hits.md`, both on the docs branch), to be folded into the docs in the closing commit (H13). The plan says
`Saved/Notes/`; `docs/notes/` is the current rule.

## H1 hit geometry and data (GSTNCombatCore, `Hits/`)

- Plan H1 `FGSTN_HitboxTrack` / `FGSTN_HurtboxModifierTrack`: the C2 tracks are called `FGSTN_TimelineTrack_Hitbox` and
  `FGSTN_TimelineTrack_HurtboxModifier` and already carry `FGSTN_PartSelector`; they stay (names, the M3 validation, the default
  dodge timelines and tests use them) and are completed in `Timeline/GSTN_TimelineTypes.h`, not in `GSTN_HitboxTypes.h`
  (a track derives from the timeline base, which `GSTN_HitboxTypes.h` is included by). Hitbox track: `Hitboxes`
  (`TArray<FGSTN_HitboxSpec>`) beside `Part`. Modifier track: `Part` + `Kind` kept; `State` of the spec is `Kind`, now
  `Invincible / Intangible / Armor` (the resolved per-hurtbox state is the new `EGSTN_HurtState`, with `Normal`);
  added `ArmorThreshold`, `AffectedHurtboxes`, `AffectedTags`, `ShapeOverrides`.
- Spec 3 "`AffectedHurtboxes` (empty = all)": not followed. M3's owner rule stays (a selector with nothing set addresses
  nothing, and the validator reports an error): `Addresses()` is the union of `Part.PartName` / `AffectedHurtboxes` by name and
  `Part.PartTag` / `AffectedTags` by tag (hierarchical); `Part.GroupName` addresses no M4 hurtbox (a hurtbox has no group; the
  parts layer uses it). "All hurtboxes" is spelled by the root tag `Combat.Hurtbox` if every hurtbox carries a leaf, or by
  listing the names. Fail-safe: an empty modifier never makes the whole body intangible.
- `ShapeOverrides` read as "the portion of the addressed hurtboxes the state covers" (spec 1b: "which portion of them"), not as a
  replacement shape. `FGSTN_TimelinePlayer::ResolveHurtState` leaves a modifier that has portions out; the H4 `HurtState` step
  decides with the contact point.
- Plan "`FGSTN_TimelineFrameResult`, M8 §3 shape": delivered as a separate `FGSTN_TimelinePlayer::EvaluateHits(Data, Frame,
  Result)` (Frame = timeline frame minus paused frames, as `FGSTN_TimelineOutput::Frame`) with `FGSTN_ActiveHitbox { Spec, Track,
  TrackFrame }` and the open modifier tracks; `Evaluate` and `FGSTN_TimelineOutput` are unchanged. The per-hurtbox state is
  `ResolveHurtState(Result, Hurtbox)` returning `FGSTN_HurtboxFrameState { State, ArmorThreshold }` (Intangible > Invincible >
  Armor, armor keeps the larger threshold, infinite beats a number).
- Key frames of a hitbox (`FGSTN_HitboxKey::Frame`) count from the start of the track; `BakedSockets` are indexed by timeline
  frame. Keys must be sorted (validator error otherwise). Rotation interpolates by slerp, offset and scale linearly; outside the
  keyed range the nearest key holds.
- `FGSTN_CombatTimelineData::BakedSockets` is `TMap<FName, FGSTN_BakedSocketFrames>` (a struct with `TArray<FTransform> Frames`),
  not `TMap<FName, TArray<FTransform>>`: a nested container is not reflectable. `FindBakedSocket(Socket, Frame, Out)` clamps the
  frame (past the end the last transform holds).
- Capsule extent convention: `Extent.X` = radius, `Extent.Z` = half height including the hemispheres (Unreal's), axis = local Z,
  `Extent.Y` ignored. Sphere uses `X`. Scale: sphere by the largest component, capsule radius by the larger of X and Y, half
  height by Z, box per axis.
- Sweep: N = ceil(travel of the shape's centre / smallest world extent), clamped to [1, 4], samples at i / N for i = 0..N
  (so up to five tests, the endpoints included). Only the centre's translation sets N: a hitbox that only rotates in place takes
  one step (its rotation is still interpolated when it also translates). Documented in the header; revisit with the M5 baked
  swings if tunnelling shows up.
- Capsule vs box is exact (no sampling): the squared distance from the segment to the box is convex and piecewise quadratic,
  split at the face-plane crossings, and minimised in closed form per piece.
- `FGSTN_HitData::ExtraEffects` is `TArray<TSoftClassPtr<UObject>>`, not `TSubclassOf<UGameplayEffect>`: Core has no
  GameplayAbilities dependency. H5 resolves and validates them in `GSTNCombatGAS`.
- `FGSTN_HitDataOverride` is "which fields (bit mask `EGSTN_HitDataField`) take the value from `Values`" + multipliers +
  `ExtensionOverrides`, not a struct of optionals. Order in `ApplyVariant`: replace, then multiply (a negative
  `FixedKnockback` stays unused), then extensions replaced by struct type (appended when the hit has none).
  `FGSTN_HitExtensionOverride` of plan H4 is not needed: the override list holds `FInstancedStruct`s of `FGSTN_HitExtension`
  (`GSTN_HitExtensions.h` carries only the base struct in H1; H4 adds appliers there).
- Types the plan puts in H2/H3 but that H1's structs need are declared early in their final files, minimal: `GSTN_ReactionTypes.h`
  (`EGSTN_HitReaction`, `FGSTN_StaggerThreshold`, `FGSTN_HitReactionModifiers`, `FGSTN_BakedCurve`) and `GSTN_ImpactTypes.h`
  (`EGSTN_ImpactOutcome`, `FGSTN_ImpactProfile`). Differences H2/H3 must keep or change on purpose:
  `FGSTN_HitReactionModifiers::StaggerThreshold` is an `FGSTN_StaggerThreshold` (`Value` = -1 unset, `bInfinite`) instead of a
  float, so super armor works on a hurtbox; `FGSTN_BakedCurve` is `USTRUCT()` with `TArray<uint16> Samples` (Blueprint has no
  16 bit integer, so the property is not exposed; 64 samples = authored, empty = unset) and has no evaluation yet;
  `FGSTN_ImpactProfile::Forbid` is `TArray<EGSTN_ImpactOutcome>` (outcomes are an enum, not tags).
- `UGSTN_HurtboxSetDefinition` is a plain `UDataAsset` (no primary asset type to register); `Hurtboxes` is the array the
  `HurtboxIndex` points into.
- Tags: `Hits/GSTN_CombatHitTags.h` (not in `GSTN_CombatCoreTags.h`): `Combat.Hurtbox` (root), `Combat.HitType.*`,
  `Combat.Relationship.*`. The hurtbox leaves are in `Plugins/GSTNCombat/Config/Tags/GSTNCombat.ini`. The engine does not scan a
  plugin's `Config/Tags`: `GSTNCombatCore` registers the folder in `StartupModule` (`AddTagIniSearchPath`, new private dependency
  `Projects`). A loose tag ini reads a plain `GameplayTagList=` line per tag; the `+GameplayTagList=` form loads nothing.
- Validator (`FGSTN_TimelineValidation`): hitbox shape without volume (error), a `BakedSocket` hitbox with no socket name (error),
  a socket with no bake or a bake shorter than the timeline (warnings), unsorted keys (error), `HitType` other than Normal on the
  hit or on a variant that overrides it (warning, plan assumption M4.13), an Armor window with no threshold (warning). A
  modifier addressed only through the new lists is a valid selector.
- Not done in H1 (on purpose): the reach logic (`ReachAxis` / `bScaleExtentWithReach` are data, M11), the same-group priority
  rule for sweet/sour spots (H8), the hurt state as a pipeline step (H4), `FGSTN_HitVictimView` completion (H2).

## H2 reaction model (GSTNCombatCore, `Hits/`)

- Files as the plan lists them, plus `GSTN_ReactionValidation.h/.cpp` (`FGSTN_ReactionValidation::ValidateTuning`, `ValidateModelWithCurve`; its own `FGSTN_ReactionMessage`, not the timeline's) and `GSTN_ReactionTypes.cpp`. The pre-declared types were completed, not redefined: `FGSTN_StaggerThreshold` (value + `bInfinite`) is used by `FGSTN_HitReactionModifiers` and by the stagger test; `FGSTN_BakedCurve` kept `TArray<uint16> Samples` and gained the evaluation / baking.
- `FGSTN_BakedCurve` encoding (the plan only said "64 x uint16, integer interpolation"): Q2.14 (16384 = 1.0, so up to about 4), the input is quantised to 1/4096 and the rest is integer arithmetic; NaN reads as 0; an unauthored curve evaluates to 1.0 (`One`) and is not `IsFlat()`.
- `FGSTN_VictimReactionState` is a plain trivially copyable struct (not a USTRUCT: it holds `uint16`, which Blueprint cannot expose) with `Make()` doing the quantising: percent in tenths, weight scale in 1/64, `EffectiveThresholdQ` an `FGSTN_StaggerThreshold` rounded to whole units. Flags are the constants of `GSTNVictimReactionFlags`. `FGSTN_HitVictimView` got `HealthFraction`, `Weight`, `ReactionState` and a non reflected `const void* SnapshotExtension` (the "pointer to the extension POD"); the M8 fields it already had are untouched.
- `FGSTN_KnockbackInput` is the plan's "(hit, input)" folded into one struct: it holds `Hit` itself. Added fields the formulas need and the plan did not name: `LaunchDirection` (unit, H3 builds it), `Up` (victim's local up, for the push back), `Weight`, `DamagePercent`, `HealthFraction`. `FGSTN_KnockbackResult` added `Knockback` (K after `KnockbackScale`).
- Model structure: `UGSTN_KnockbackModel::Compute` (BlueprintNativeEvent) = `ResolveRules(Input, ComputeKnockback(Input))`; subclasses override only the protected pure virtual `ComputeKnockback` (K before `KnockbackScale`). `Tuning` (`FGSTN_ReactionTuning`) is a property of the base. Rules in `ResolveRules` follow the plan's order exactly.
- Hitstop Auto uses the **real K** (after `KnockbackScale`) in `HitstopPerKnockback`, per the plan ("Auto from real K and real damage"), not the `NominalKnockback` of the older spec text. Both sides use the same `Base`.
- Push back: horizontal = launch direction projected on the plane normal to `Input.Up`; when the direction lies along `Up` there is no horizontal part and the velocity is 0. Promotion to `Launch` when airborne or on a wall stays in H9 (needs the movement state).
- Formulas the spec left open (documented in the header, tested with hand values): `PercentScaling` is the platform fighter formula with every constant a property (`P = DamagePercent + Damage`, `K = ((P/10 + P*Damage/20) * 200/(EW+100) * 1.4 + 18) * Growth/100 + Base) * UnitScale`, `UnitScale` 10 cm/s); a `FixedKnockback >= 0` is the whole K there (no percent, no weight), while in `WeightDriven` it replaces only the bracket and the weight still applies; `CappedPercent` is `PercentScaling` with the percent clamped to `PercentCap` (default 100); `Fixed` is `FixedKnockback` else `BaseKnockback` (no growth, percent or weight).
- C++ defaults of `FGSTN_ReactionTuning` are neutral starting points, not the plan's rules asset numbers: `StaggerPerDamage` 1, `StaggerPerKnockback` 0, `HitstopPerDamage` 0.3, `HitstopBase` 3, `MaxHitstop` 20, `TumbleThreshold` 1800, `HitstunFactor` **0.02** frames per cm/s (the plan's "0.4 frames per cm/s" would be 720 frames at the tumble threshold: to settle with the owner when the standard rules asset is written in H4), `MinHitstunFrames` 6, `PushBackSpeedScale` 1, `MaxPushBackSpeed` 600, `WeightReference` 100, `MinWeight` 1. Tier thresholds 0 as specified.
- Validator: `PushBackThreshold <= LaunchThreshold <= TumbleThreshold` (errors), non positive `WeightReference` / `MinWeight` (error), `MaxHitstop == 0` (warning), a curve whose sample count is neither 0 nor 64 (error), a model with `UsesPercentTerm()` next to an authored non flat curve (warning). `UsesPercentTerm()` is a virtual on the model.
- Test additions beyond the plan list: `PushBackOnVictimUp` (tilted up, never world Z), `BitwiseIdentical` (300 LCG inputs through the four models on two runs, compared field by field). Test tunings use binary exact fractions (0.25, 0.0625, 0.0078125) because `0.01f * 500` floors one frame short.
- Not done (on purpose): `KnockbackResistance` (H9, velocity only), the stagger lock floor (H9), `AirborneJuggleScale` (rules, H4), the `HealthWeight` step that writes `WeightScale` (H4).
