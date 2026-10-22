# GSTNCombatGAS

The ability-system layer of [GSTNCombat](../GSTNCombat/README.md). It is its own plugin so that `GSTNCombat`
(`GSTNCombatCore`) stays free of GameplayAbilities: a project that only wants the input-facing combat content never loads
the ability system. Design: [docs/architecture/combat.md](../../docs/architecture/combat.md) and
[combat-m3-moves-kits.md](../../docs/architecture/combat-m3-moves-kits.md).

Status: **experimental (0.1)**. It holds the native tags, the two attribute sets, the meter-cost and weapon-stats
effects, the stat/meter data structs and the plugin settings; the kit/weapon/loadout assets follow in M3.

## Dependency rule

```
GSTNCombatCore  <  GSTNCombatGAS  <  GSTNCombatNP
 (no GAS/NP)       (GAS, no NP/Mover)   (fork, NP, Mover)
```

- Module `GSTNCombatGAS` depends on Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, GameplayTasks,
  DeveloperSettings and `GSTNCombatCore`. The descriptor lists the plugins `GSTNCombat` and `GameplayAbilities`.
- It never depends on NetworkPrediction, AbilitySystemSimulation, Mover or `GSTNCombatNP`, and nothing here refers to the
  project module, `/Game/` content or a project tag. It compiles against stock GAS and against the GAS-NPP fork in
  `Plugins/GASNPP` (same plugin and module name).
- Nothing in `GSTNCombat` depends on this plugin. `GSTNCombatNP` will list it when it first needs a type from here.

## Tags

Native, declared in `Public/GSTN_CombatTags.h` (namespace `GSTNCombatTags`), one doc line each in
`Private/GSTN_CombatTags.cpp`.

| Subtree | Tags | Notes |
|---|---|---|
| `Combat.State.*` | `Busy InTimeline SuperInput CancelWindow OverrideWindow Falling` | Projected by the combat tick, never granted by an ability or effect. |
| `Combat.Ability.*` | `Timeline AwaitingAdoption` | Ability identity. |
| `Combat.Lock.Movement` | | Read by the stats movement modifier. |
| `Combat.Weapon.Role.*` | `Primary Support` | Granted by the stats effect of the active weapon. |
| `Combat.Event.*` | `TimelineStarted TimelineFinished WeaponSwitched SuperUsed` | Authoritative outbox events. |
| `Combat.SetByCaller.MeterCost` | | Set-by-caller key. |
| Designer roots | `Weapon`, `Kit`, `Super`, `Loadout`, `Skin`, `Combat.Stat`, `Combat.Meter`, `Combat.SetByCaller.Stat` | Roots only, so tag pickers can filter; the leaves are designer content in a tag ini and code never names one. |

Not here: the input vocabulary (`Input.Button/Context/Lock.Command/BufferEvent.*`, `Command.*` families) belongs to
`GSTNInputCore` and `GSTNCombatCore`, and `Combat.Mover.*` to the NP module that sees Mover.

## Attributes, effects and data

| Type | File | Role |
|---|---|---|
| `UGSTN_CombatAttributeSet` | `Attributes/` | Movement and jump stats (`WalkSpeed`, `Acceleration`, `Braking`, `GroundFriction`, `AirControl`, `Weight`, `MaxCarryWeight`, and the jump set `JumpVelocity` ... `JumpSquatFrames`). No health. Every stat is clamped to at least 0, gravity scales to at least `MinGravityScale` (0.05), in both `PreAttributeChange` and `PreAttributeBaseChange` (the fork idiom: the aggregated and the base value agree). Frame counts are floats, read with `Get*FrameCount()` (rounded down). |
| `UGSTN_MeterAttributeSet` | `Attributes/` | Meters as value/max pairs (`ExMeter`/`ExMeterMax`, `SuperMeter`/`SuperMeterMax`). `GetMeterPairs()` is the only place that pairs them: a new meter is two attributes and one line there. Value clamped to `[0, Max]`, max to `>= 0`; an instant effect that lowers a max pulls the value down (`PostGameplayEffectExecute`). |
| `FGSTN_StatValue`, `FGSTN_KitStats` | `Attributes/GSTN_KitStats.h` | A designer number keyed by a `Combat.SetByCaller.Stat.*` leaf. `FGSTN_KitStats::ApplyTo(ASC, InitEffect)` fills the spec and applies the kit's instant init effect; `Validate` reports empty, duplicate and out-of-root tags. |
| `FGSTN_MeterSetup`, `UGSTN_MeterUtils` | `Attributes/GSTN_MeterSetup.h` | `{Meter tag (Combat.Meter), Value, Max, PointsPerBar, Bars}` and BlueprintPure readers (`FindMeterSetup`, `GetFullBars`, `GetMeterAmount`, `GetMeterMax`). |
| `UGSTN_GE_MeterCost` | `Effects/` | Instant cost: pick the meter attribute on a Blueprint child and the additive modifier on `Combat.SetByCaller.MeterCost` is built from it. `SetCost(Spec, Cost)` stores the negated cost; overspending stops at 0. |
| `UGSTN_GE_WeaponStats` | `Effects/` | Infinite base of a weapon stats effect. `Apply`/`Remove`/`IsApplied` find the effect by class (no stored handle); `Apply` returns the live effect instead of stacking. One stats class per weapon. |
| `UGSTN_CombatGASSettings` | `Settings/` | Starting values of the attributes by property name (`AttributeDefaults`), defaults in `Config/DefaultGSTNCombatGAS.ini`. A kit init effect overwrites them. |

Modifiers on a stats effect and on the kit init effect read their magnitudes from set-by-caller stat tags, and the number for
each tag lives in `FGSTN_KitStats` / the weapon modifiers, so no C++ names a stat leaf. A kit init effect must override a
meter maximum before the meter.

Blueprint notes: `UGSTN_GE_MeterCost` and `UGSTN_GE_WeaponStats` are `Blueprintable` (children are the designer content);
the structs are `BlueprintType` with `EditAnywhere, BlueprintReadOnly`, tag fields carry `Categories`; the attribute sets
are native only. Applying and removing stats effects is for the loadout, outside the simulation.

## How to wire it

1. Enable the plugin `GSTNCombatGAS` (the project's `.uproject` lists it; it pulls in `GSTNCombat` and GameplayAbilities).
2. A module that uses the tags lists `GSTNCombatGAS` in its `Build.cs` and includes `GSTN_CombatTags.h`.
3. Designers add leaves under the roots in a tag ini (`Config/Tags/` of a plugin or of the project).

## Testing

Automation tests `GSTNCombat.GAS.*`: `Tags.AllRegisteredAndDocumented`, `Attributes.Clamp`, `Attributes.KitStatsInitEffect`,
`Attributes.MeterCostSetByCaller`, `Attributes.WeaponStatsApplyRemoveByClass`. The attribute tests use a stock
`UAbilitySystemComponent` on an actor in a transient game world (effects need a world for their start time), with no Network
Prediction or Mover, so they also run against stock GAS. Run with the same command
as the other combat tests: `-ExecCmds="Automation RunTests GSTNInput+GSTNCombat; Quit"`.
