# GSTNCombatGAS

The ability-system layer of [GSTNCombat](../GSTNCombat/README.md). It is its own plugin so that `GSTNCombat`
(`GSTNCombatCore`) stays free of GameplayAbilities: a project that only wants the input-facing combat content never loads
the ability system. Design: [docs/architecture/combat.md](../../docs/architecture/combat.md) and
[combat-m3-moves-kits.md](../../docs/architecture/combat-m3-moves-kits.md).

Status: **experimental (0.1)**, skeleton. Today it holds only the native tags; attribute sets, effects and the
kit/weapon/loadout data follow in M3.

## Dependency rule

```
GSTNCombatCore  <  GSTNCombatGAS  <  GSTNCombatNP
 (no GAS/NP)       (GAS, no NP/Mover)   (fork, NP, Mover)
```

- Module `GSTNCombatGAS` depends on Core, CoreUObject, Engine, GameplayTags, GameplayAbilities, GameplayTasks and
  `GSTNCombatCore`. The descriptor lists the plugins `GSTNCombat` and `GameplayAbilities`.
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

## How to wire it

1. Enable the plugin `GSTNCombatGAS` (the project's `.uproject` lists it; it pulls in `GSTNCombat` and GameplayAbilities).
2. A module that uses the tags lists `GSTNCombatGAS` in its `Build.cs` and includes `GSTN_CombatTags.h`.
3. Designers add leaves under the roots in a tag ini (`Config/Tags/` of a plugin or of the project).

## Testing

Automation tests `GSTNCombat.GAS.*` (for example `GSTNCombat.GAS.Tags.AllRegisteredAndDocumented`), run with the same command
as the other combat tests: `-ExecCmds="Automation RunTests GSTNInput+GSTNCombat; Quit"`.
