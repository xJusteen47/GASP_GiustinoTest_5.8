# M10 slice 7: G7 hit ring and friendly fire by source, owner's pickup settings (desktop, 2026-11-10, 30 min window)
Branch `claude/desktop-m10-7` from `claude/desktop-m10-6` (5f67ffb5). Brief: `m10-slice6.md`, plan `combat-m10-held-objects.md` §8-§10. Doc: `Plugins/GSTNHeld/README.md` steps 9, 12.

**Done**
- G7 per-throw hit ring: a Bounce rebound shot is seeded with the targets the throw already hit (`UGSTN_ProjectileSubsystem::GetHitTargets` /
  `SeedHitTargets`, host only); they also count toward `MaxHits` across rebounds. `FGSTN_HeldObjectTuning::bOneHitPerTargetPerThrow` (default on,
  off = slice 6 behaviour). Log `HeldImpact .. Bounce .. carried=`.
- G7 friendly fire by mode: `UGSTN_CombatRules::FriendlyFireSources` (tag container, hierarchical; empty default = allies ignored, plan default) +
  `IsRelationshipHittableBySource`; GSTNProjectile uses it with the shot's `SourceTag`, so a mode's rules asset can let thrown objects hit allies while
  melee stays team-safe (`bFriendlyFire` unchanged for everything).
- Owner's decision of 2026-11-10 (both behaviours as settings, `DefaultGSTNHeld.ini`): `bPickupTakesFocusedObject` (true = framed object, false =
  nearest) and `bFocusMissFallsBackToNearest` (true = nearest other free object, false = empty hand when the framed one is contested/gone).
  Defaults keep slice 5/6 behaviour. `FGSTN_PickupRequest::bFallBackToNearest`.
- Tests: `GSTNCombat.Core.Hits.Pipeline` (friendly fire by source), `GSTNHeld.Core` pickup race (no-fallback cases).

**Gate**: RESULTS_PLACEHOLDER

**Not done / M10 left**: G7 shoulder-aim AimPolicy and `Explode`; no harness scenario for a pierce-and-rebound throw (the ring is only exercised by
code path, MaxHits 1 throws never rebound after a fighter hit); SUITE_DEFAULT/MOUNT rerun for the slice 6 fork fix (proxy attribute delta) still owed.
Then G8 (object ghost, `scenario_throw_ghost/host`), G9 thrown enemies, G12-G13 (size-binding validator, debug/editor, sweep, three-config build).
Slice 6 test values and the open owner questions unchanged.

COST_PLACEHOLDER
