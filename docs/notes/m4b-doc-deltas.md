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
