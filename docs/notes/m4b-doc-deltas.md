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
