# GSTNCombat

Fighting-game style combat content for Unreal Engine 5.8, built on [GSTNInput](../GSTNInput/README.md): the standard
button and command tags, the combat lock tags, the default command set and the combat settings. This plugin is
`GSTNCombatCore` only: pure and deterministic, it works without the GAS-NPP fork. Simulating combat with Network
Prediction (the combat NP model, the ability system driven from it) is the separate plugin
[GSTNCombatNP](../GSTNCombatNP/README.md).

Status: **experimental (0.1)**. Moves, kits and hits are specified but not built yet. The design lives in
[docs/architecture/combat.md](../../docs/architecture/combat.md) (with one file per milestone, `combat-m1` to
`combat-m5-m9`); the generic input layer is described in [docs/architecture/input.md](../../docs/architecture/input.md).
This README is the map; those documents hold the decisions and the reasons.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatCore` | exists | Combat content built on `GSTNInputCore`, pure and deterministic (no GAS, no Network Prediction): the standard button tags (`Input.Button.Attack`...), the standard command tags and the default command set (`UGSTN_StandardSlots`). Covered by Automation tests (`GSTNCombat.Core.*`). |
| `GSTNCombatNP` | separate plugin | Moved to the plugin [GSTNCombatNP](../GSTNCombatNP/README.md) (same module name): the combat NP model (`UGSTN_CombatSimComponent`), which needs the fork. |
| `GSTNCombatGAS` | planned | Native tags, attribute set, damage effect and execution, weapons and loadouts. |
| `GSTNCombatEditor` | planned | Timeline editor, slot conflict validator UI, debug drawing. |

The generic input code (history, classifier, pattern grammar, matcher, buffer, slots, resolver, bindings, settings) lives in
the [GSTNInput](../GSTNInput/README.md) plugin, which this one depends on (`GSTNCombatCore` -> `GSTNInputCore`). The rule
that decides what stays here: anything about *fighting* (standard buttons, command tags, default slots, hits) is combat,
anything about *input in general* is input. The combat priority tiers and exclusive buttons are the plugin's own settings
(`UGSTN_CombatSettings`, Project Settings > Plugins > GSTN Combat, `GSTNCombat.ini`): defaults in
`Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini`, overridable in the project's `Config/DefaultGSTNCombat.ini`. The module
contributes them to the input plugin's settings when it starts (`UGSTN_InputSettings::SetContribution`), which ships both lists
empty; nothing about combat lives in the project's `DefaultGame.ini`.

**Tags.** The combat plugin declares the standard command roots (`Command.Attack`...) and lock tags natively. The standard
button leaves (`Input.Button.Attack`, `.Strong`, `.Special`, `.Crouch`, `.Shield`, `.Jump`) are native too and sit under the
`Input.Button` root that the input plugin owns: an accepted exception for now (the input plugin declares only the root, buttons
are the vocabulary of whoever has them, and `Input.Button` is the filter every tag picker uses), see input.md. Designer buttons
and commands of a game are tags in the game's own tag ini.

**Requirements.** `GSTNCombatCore` needs `GSTNInputCore` and Developer Settings; the descriptor lists only the plugin
`GSTNInput`, so enabling `GSTNCombat` pulls in no fork plugin (GameplayAbilities, AbilitySystemSimulation, NetworkPrediction)
and no Mover. The native lock tags of the combat command roots (`Input.Lock.Command.Attack`, `.Strong`, `.Special`, `.Combo`,
`.Super`) are declared here (`GSTN_CombatLockTags.h`, namespace `GSTNCombatLockTags`), next to the roots' command tags. How
the command locks, the activation context and the events work in the simulation is described in the
[GSTNCombatNP README](../GSTNCombatNP/README.md) and the [GSTNInputNP README](../GSTNInputNP/README.md).

**Dependency rule.** Core modules keep no NP, GAS or Mover dependencies: `GSTNCombatCore` depends only on `GSTNInputCore`
and engine modules, and nothing in this plugin depends on `GSTNCombatNP`. An `*NP` module may depend on another plugin's
`*NP` module of the same layer, and on nothing above it; `GSTNCombatNP` depends on this plugin's Core and on `GSTNInputNP` by
design (see the [GSTNCombatNP README](../GSTNCombatNP/README.md)). Shared NP types are not moved into the Core modules.

## How to extend it

The generic extension points (a new input condition, slot, binding or buffer reset rule) are described in the
[GSTNInput README](../GSTNInput/README.md). What is specific to combat:

- **A new standard command**: add its tag to `GSTN_CombatCommandTags` (and give its root a tier in
  the combat settings, `Config/DefaultGSTNCombat.ini`, if it is a new root), add the slot in `UGSTN_StandardSlots::Build`, then update the
  `GSTNCombat.Core.StandardSlots.*` tests (they check the shapes, the chords and that the validator finds no conflicts).
- **A new standard button**: add its tag to `GSTN_CombatButtonTags` and to the project's input config; add it to
  the combat settings' `ExclusiveButtons` if its presses compete with attacks for one move.
- **The combat component**: in the [GSTNCombatNP README](../GSTNCombatNP/README.md).

Tests are in `Source/GSTNCombatCore/Private/Tests/` with the helpers of `GSTN_CombatTestUtils.h`.

## Conventions

- Types are prefixed `GSTN_` (`UGSTN_`, `FGSTN_`, `EGSTN_`); export macro is `GSTNCOMBATCORE_API` (`GSTNINPUTCORE_API` for the input plugin).
- Public headers in `Public/<Area>/`, implementations in `Private/<Area>/`.
- Comments explain *why* and the simulation constraints. Headers document the intent of a class: read the header before
  the `.cpp`.
- Tuning values are `UPROPERTY` with `Category = "Area|Section"` and `meta` limits, grouped in small structs.

## Testing

- Pure logic: Automation tests `GSTNCombat.Core.*` (Session Frontend > Automation, or
  `UnrealEditor-Cmd <your project>.uproject -ExecCmds="Automation RunTests GSTNInput+GSTNCombat; Quit"`, with your project's `.uproject`).
- Network behaviour (combat NP model): see the [GSTNCombatNP README](../GSTNCombatNP/README.md) and
  [testing.md](../../docs/architecture/testing.md).

## Further reading

[combat.md](../../docs/architecture/combat.md) (overview and decisions) ·
[combat-m1-core-input.md](../../docs/architecture/combat-m1-core-input.md) ·
[combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) ·
[combat-m3-moves-kits.md](../../docs/architecture/combat-m3-moves-kits.md) ·
[combat-m4-hits.md](../../docs/architecture/combat-m4-hits.md) ·
[input.md](../../docs/architecture/input.md) · [GSTNInput README](../GSTNInput/README.md) · [GSTNCombatNP README](../GSTNCombatNP/README.md) · [gas-npp.md](../../docs/architecture/gas-npp.md) ·
[PATCHES.md](../GASNPP/PATCHES.md)
