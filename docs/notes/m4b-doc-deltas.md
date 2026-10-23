# M4b doc deltas (GSTNParts P1-P3)

One line each: where, what differs, why. Collected per checkpoint; folded into `docs/` by plan P14 (the only docs commit).

## P1 (skeleton, tags, settings)

- m4b-parts-plan.md P1 (settings): the plan lists "default penetration table soft ref" in `UGSTN_PartsSettings`; the table type arrives
  in P2, so the field is added there, not in P1. Why: the type does not exist yet and a placeholder would change type later.
- m4b-parts-plan.md P1 (settings): "default weight curve" is `DefaultWeightScaleAtCap` (linear 1.0 down to that value at the cap)
  plus `MinWeightScale`, and a part set can carry its own curve. Why: a settings-level curve asset would need a soft load inside
  the bake at asset load; a number gives the spec's default (1.0 to 0.6) with nothing to load.
- m4b-parts-plan.md P1 (settings): `FGSTN_PartsPresentationBudget` (48 chunks, 4 s, 600 particles, 2 GC, 1.5 s sleep) is a struct in its
  own header so the rules extension (P2) and the presentation module (P10) share it. Why: CP §6.4 numbers as data.
- combat-parts.md §7 / plan P1: the ini tags (`Part.Type.*`, `GameplayCue.Combat.Part.*`) need the module to register the plugin's
  `Config/Tags` folder (the engine reads it only for the project and Game Feature plugins), as `GSTNSurface` does. Why: otherwise the
  leaves do not exist and the picker filters are empty.
- m4b-parts-plan.md P1 (`.uproject`): the plan says the `.uproject` stays untouched until the owner enables the plugin; the lead
  asked for the entry, so `GSTNParts` is appended after `GSTNCombatDebug`. Why: the tests only run with the plugin enabled.

## P2 (part data, penetration table, break effects, validators)

- m4b-parts-plan.md P2 / combat-parts.md §4.3: `FGSTN_BakedCurve` is named `FGSTN_PartsBakedCurve`. Why: the M4 plan (H2) declares a
  `FGSTN_BakedCurve` in `GSTNCombatCore` for the `HealthWeight` step; two types of one name cannot be included together (P4 needs
  both). When H2 lands, replace ours by the combat one (the table, the integer read and the tests move over unchanged).
- m4b-parts-plan.md P2 / combat-parts.md §3.1: `MaxReaction` is `EGSTN_PartReactionTier` {StaggerInPlace, PushBack, Launch}, not the M4
  `EGSTN_HitReaction`, which does not exist yet. Why: it keeps the part data complete and the same names and order, so assets load
  unchanged when P4 maps it onto (or swaps it for) the M4 enum.
- m4b-parts-plan.md P2: `FGSTN_PartHitData` and `FGSTN_PartsRulesExtension` do not derive from `FGSTN_HitExtension` / `FGSTN_RulesExtension`
  (neither exists; H1 and C3b/H4 add them). Why: no stub base with the same name may be created here (it would collide); adding a base to
  a struct without virtuals is a source-level change and assets keep loading. `FGSTN_PartsKitExtension` does derive from the existing
  `FGSTN_KitExtension`.
- combat-parts.md §3.1: `EGSTN_BrokenHurtboxPolicy::Replace` has no shape payload (`FGSTN_HitShape` is H1). The enum value exists, damage
  is computed as for `Keep`, validation warns. Why: the payload type does not exist yet.
- combat-parts.md §3.1: the set-level `Presentation` soft reference is not in `UGSTN_PartSetDefinition`. Why: `UGSTN_PartPresentationSet` is
  in the Presentation module (P10), which Core cannot name; P10 adds the reference (a soft path with an allowed class) to the set.
- combat-parts.md §3.1: the set carries its weight curve as an inline `FRuntimeFloatCurve` (`bOverrideWeightCurve`), baked in `PostLoad` and
  on edit; the default shape (1.0 to `DefaultWeightScaleAtCap`) needs no asset. Why: a soft curve asset cannot be loaded inside `PostLoad`.
- combat-parts.md §3.1: `BrokenMainTransfer` uses a negative value for "default = `MainTransfer` x 0.5" (`GetBrokenMainTransfer()`).
- combat-parts.md §3.3: `FGSTN_PartBreakEffect_Launch` fields (speed, up fraction) and `FGSTN_PartBreakEffect_Stats` entries (SetByCaller key and
  magnitude) are my choice; the spec names the effects without fields.
- combat-parts.md §3.4: the default band cue tags are empty in code (`GameplayCue.Combat.Part.Ricochet` is ini vocabulary and code never names a
  leaf, modularity §4.2b); the shipped table asset (P14) sets them. The lowest default band starts at `MinDelta` -15 (hardness 15 against
  penetration 0), the lowest possible delta; a table that stops higher falls back on its last band and validation warns.
- m4b-parts-plan.md P2 (tests): `Penetration.IgnoreHardness` needs the lookup with the flag, which is `UGSTN_PenetrationTable::LookupBand`
  (`UGSTN_PartMath::LookupBand` of P3 wraps it for Blueprint).
- m4b-parts-plan.md P2: added `Data.NumbersValidated`, `Data.PartSetAsset`, `Data.ExtensionsAndHitData`, `Data.StructsAreBlueprintTypes`,
  `Data.NoHardContentReferences` (the R-B and Blueprint gates as tests).

## P3 (pure part math)

- m4b-parts-plan.md P3: `FGSTN_PartsState` is a plain C++ POD, not a USTRUCT (fixed `uint8[16]` is not Blueprint-visible and the layout is the
  wire contract); `UGSTN_PartMath::ApplyHit` is C++ only. The BlueprintPure subset is `GetPenetrationBandIndex`, `QuantisePartDamage`,
  `DequantisePartDamage`, `IsPartBroken`, `CountBrokenParts`, `GetEffectiveWeight`. The names differ from the C++ statics because a UFUNCTION
  cannot share a name with an overload.
- combat-parts.md §4.2 / plan P3: "p breaks when it reaches `PartCap`" is implemented as "the stored byte reaches 255". With the ceil rule a
  part at more than 254/255 of its cap breaks, so the break is a pure function of the byte the fragment carries.
- combat-parts.md §4.2: a cascade breaks every descendant of a broken part, including unbreakable ones (an armour plate goes with the limb),
  and sets their byte to 255. `AllBroken` still counts breakable parts only. Spec silent; flagged as an open point.
- combat-parts.md §3.1: `Disable` on a broken part makes the hit the body's (full damage, part index `INDEX_NONE`, `bPassedToBody`); `Keep` and
  `Replace` damage the stump with `BrokenMainTransfer` and the band of the stump's hardness. A hit on a broken part never changes the byte.
- plan P3 `FGSTN_PartsProjection`: the "hurtbox mask" is a list of hurtbox **names** (`DisabledHurtboxes`), because hurtbox indices belong to the
  hurtbox set of M4; the P7 snapshot code maps names to indices. The hit-group mask is a `uint8` as in the spec (`GroupMask`).
- plan P3 `RestoreParts(State, Mask, Fraction)`: Fraction is the damage left as a fraction of the cap (0 = pristine), capped at 254/255 so a restored
  part is not broken; it never raises a part's damage; the serial advances once if any part unbroke. **Changed after review (B.1):** the
  signature is `RestoreParts(Parts, State, Mask, Fraction)`; a child cannot stay intact under a broken parent (combat-parts §3.1), so the
  broken ancestors of the masked parts are restored too (and returned in the result). `ApplyHit` also starts the cascade from the part that
  just broke instead of from the whole broken mask, so a state that does hold an intact child under a broken parent does not re-break the
  child (no second bonus, no serial event) when something else breaks. Mask 0 still means every part; a caller that computes a mask must
  test it for emptiness.
- plan P3 result type: `FGSTN_PartHitResult` also carries the band's `StaggerScale` and `HitstopScale`, `HitMainDelta` / `BonusMainDamage` / `MainDelta`,
  `bKill`, `bAllBrokenReached` so the P4 step needs nothing else from the math.
- plan P3 tests: the grab gate evaluation is in P3 (`GrabGate.Evaluation`, one test covering empty, min count, required, any-of); P9 adds the NP
  projection test. `Determinism.1000RandomSequencesTwoConfigs` runs 2 part sets x 1000 sequences x 40 hits twice each and compares the run
  fingerprint with a pinned value (the "second compile config" check is that value on another build configuration).
- plan P3 allocation test: a pass-through allocator installed with a compare and swap on the global allocator pointer counts the allocations of
  the test thread (with a control that must see a growing array allocate). No engine facility counts per thread.
- plan P3 acceptance: the golden hand values and the pinned fingerprint (0x827A1F0F6B36481A) pass in Development and DebugGame (`UnrealEditor-Win64-DebugGame-Cmd.exe`), 48 of 48 `GSTNParts.*`.

## Review fixes (2026-10-23)

- Validators (review B.3, B.4): the grab gate is checked against the parts that can ever break (breakable parts and the descendants of
  breakable parts, by the same cascade `ApplyHit` uses): a required part outside that set, an any-of gate with none inside it, or a
  `MinBrokenParts` above its size is an error. The `Launch` (finite non-negative speed, up fraction in 0..1), `Stagger` (frames not
  negative), `Stats` (a valid stat tag, finite magnitudes; a stat outside `Combat.SetByCaller.Stat` is a warning, compared as text because
  the root belongs to `GSTNCombatGAS`) and `DisableHurtbox` (no empty name) effects are checked, and a part type outside `Part.Type` warns.
  Band tables warn when the top band ricochets, when no band deals damage, and when a ricochet band keeps stagger or hitstop scales.
- Baked weight curve (review B.6): `UGSTN_PartSetDefinition` rebakes in `PostDuplicate` and `PostEditImport` as well as on load and edit. A set
  built with `NewObject` and filled by code still has to call `RebakeWeightCurve()` (documented on the function).
- `UGSTN_PenetrationTable::PostEditChangeProperty` no longer sorts on an interactive change (rows jumped while typing `MinDelta`).
- Tag namespace `GSTN_PartsTags` is now `GSTNPartsTags` (the files keep their names), to match `GSTNCombatTags` and `GSTNSurfaceTags`.
- Wording: the README and headers no longer say stagger is "the existing `Combat.State.Hitstun`" (no such tag is declared anywhere; the M4
  pipeline decides how stagger and hitstop reach the victim), no longer claim a set mismatch "is refused at connect" (nothing does that yet),
  and name the minion bridge as planned for M8, not built.
- Left as they are (design choices or theory, review B.11, B.12, minor list): the per-hit `ceil` rounding (documented in the README as a bias
  of up to half a byte per hit), `FRichCurve::Eval` in float at bake time (only linear or constant curves are bit-identical by construction; a
  fingerprint check at connect would cover it), `bWeakPointHit` on an already broken part, `Penetration` clamping in `Make`, FString vs FText
  messages in the validators, the `Mask == 0` meaning of "all" in `RestoreParts`, and the test hygiene items (`UCLASS(Hidden)` test types
  outside a test guard, the counting allocator, tests that read the project's ini defaults).
