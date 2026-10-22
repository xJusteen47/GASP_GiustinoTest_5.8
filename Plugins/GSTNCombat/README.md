# GSTNCombat

Fighting-game style combat for Unreal Engine 5.8: input classification, a command buffer with priorities, moves as
gameplay abilities and a hit system, all simulated by Network Prediction (the GAS-NPP fork in `Plugins/GASNPP`) so it
rolls back and re-simulates together with the ability system and Mover.

Status: **experimental (0.1)**. Input and the combat simulation component exist; moves, kits and hits are specified but
not built yet. The design lives in [docs/architecture/combat.md](../../docs/architecture/combat.md) (with one file per
milestone, `combat-m1` to `combat-m5-m9`); the generic input layer is described in
[docs/architecture/input.md](../../docs/architecture/input.md). This README is the map; those documents hold the
decisions and the reasons.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatCore` | exists | Combat content built on `GSTNInputCore`, pure and deterministic (no GAS, no Network Prediction): the standard button tags (`Input.Button.Attack`...), the standard command tags and the default command set (`UGSTN_StandardSlots`). Covered by Automation tests (`GSTNCombat.Core.*`). |
| `GSTNCombatNP` | exists | The adapter to the Network Prediction fork: the combat NP model (`UGSTN_CombatSimComponent`, sync/aux state), which embeds the input command and input sim state of `GSTNInputNP`. Everything that touches the ability system goes through here. |
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

**Requirements.** `GSTNCombatCore` needs `GSTNInputCore` and Developer Settings. `GSTNCombatNP` needs the GAS-NPP fork plugins and
Mover (optional plugin dependencies in the descriptor, but the module does not build without them).
The input command, the input sim state and the Mover tracker live in `GSTNInputNP` (step S2a of input.md); the simulator, sources,
binding set and context provider (S2b) are there too: the combat component hosts the simulator
(`IGSTN_InputSimHost`) and only adds the debug hitstop. It also implements `IGSTN_CommandSourceInterface` (the activation
context and `IsCommandButtonHeld` for abilities), honours the command locks (`Input.Lock.Command[.<root>]`) and publishes
`Input.Event.CommandExecuted` through `UGSTN_SimEventSubsystem` on the authority only (the listen-server host's own pawn
included). The native lock tags of the combat command roots (`Input.Lock.Command.Attack`, `.Strong`, `.Special`, `.Combo`, `.Super`)
are declared here (`GSTN_CombatLockTags.h`, namespace `GSTNCombatLockTags`), next to the roots' command tags.

**Dependency rule.** An `*NP` module may depend on another plugin's `*NP` module of the same layer, and on nothing above it.
`GSTNCombatNP` depends on `GSTNInputNP` by design: the combat NP model embeds the input simulation, one NP model per
character (see input.md). Core modules keep no NP, GAS or Mover dependencies, so shared NP types are not moved into
`GSTNInputCore`. `GSTNCombatNP` may use only the Public headers and interfaces of `GSTNInputNP` (for example
`IGSTN_InputSimHost`), never its internals.

## How a frame flows

On a pawn that has the combat component, an ability system component and a Mover component, every fixed Network
Prediction tick (62.5 Hz) runs the three models in this order, on the server, on the predicting client and again on
every re-simulation after a rollback:

```
combat model (priority 47)  ->  ability system (48)  ->  Mover (50)
```

1. **Combat model.** Reads the frame's input command (buttons, raw stick, camera yaw), classifies the stick against the
   facing the Mover tracker knows for the end of the previous frame, and appends the result to the input history kept in
   its sync state. It then decides what it wants from the ability system (activate a move, pause the montage for
   hitstop) and writes that as a *frame-stamped request* in its own sync state. It never calls the ability system.
2. **Ability system.** At the start of its tick it broadcasts `OnSimulationPreTick` (a hook added to the fork, see
   [PATCHES.md](../GASNPP/PATCHES.md)). The combat component executes the pending requests there, so activations run
   with the correct frame, time and rollback bookkeeping.
3. **Mover.** Moves the character. Abilities reach Mover only through queued moves, never by writing character state.

Consequences worth remembering: tags and effects granted in a frame are visible to Mover in the same frame, but
Mover's result is visible to the abilities one frame later; all code in this chain must be deterministic and keep its
state in the sync/aux state (rules in [gas-npp.md](../../docs/architecture/gas-npp.md)).

## How to extend it

The generic extension points (a new input condition, slot, binding or buffer reset rule) are described in the
[GSTNInput README](../GSTNInput/README.md). What is specific to combat:

- **A new standard command**: add its tag to `GSTN_CombatCommandTags` (and give its root a tier in
  the combat settings, `Config/DefaultGSTNCombat.ini`, if it is a new root), add the slot in `UGSTN_StandardSlots::Build`, then update the
  `GSTNCombat.Core.StandardSlots.*` tests (they check the shapes, the chords and that the validator finds no conflicts).
- **A new standard button**: add its tag to `GSTN_CombatButtonTags` and to the project's input config; add it to
  the combat settings' `ExclusiveButtons` if its presses compete with attacks for one move.
- **The combat component in Blueprint**: `UGSTN_CombatSimComponent` is `Blueprintable` so a project can subclass it for its own
  defaults. The input source, the context provider and the rule families are the Blueprint extension points (see the GSTNInput
  README, "Writing a rule in Blueprint"); the component's own simulation hooks stay in C++. Its `InputConfig` and `SlotBindings`
  are soft references resolved once in `BeginPlay`.

Tests are in `Source/GSTNCombatCore/Private/Tests/` with the helpers of `GSTN_CombatTestUtils.h`.

## Conventions

- Types are prefixed `GSTN_` (`UGSTN_`, `FGSTN_`, `EGSTN_`); export macros are `GSTNCOMBATCORE_API` / `GSTNCOMBATNP_API` (`GSTNINPUTCORE_API` for the input plugin).
- Public headers in `Public/<Area>/`, implementations in `Private/<Area>/`.
- Comments explain *why* and the simulation constraints. Headers document the intent of a class: read the header before
  the `.cpp`.
- Tuning values are `UPROPERTY` with `Category = "Area|Section"` and `meta` limits, grouped in small structs.

## Testing

- Pure logic: Automation tests `GSTNCombat.Core.*` (Session Frontend > Automation, or
  `UnrealEditor-Cmd <your project>.uproject -ExecCmds="Automation RunTests GSTNInput+GSTNCombat; Quit"`, with your project's `.uproject`).
- Network behaviour: the PIE harness `Tools/GASNPPTest/gasnpp_test.py` (dedicated server + 2 clients), scenario
  `combat_spike`. See [testing.md](../../docs/architecture/testing.md).

## Further reading

[combat.md](../../docs/architecture/combat.md) (overview and decisions) ·
[combat-m1-core-input.md](../../docs/architecture/combat-m1-core-input.md) ·
[combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) ·
[combat-m3-moves-kits.md](../../docs/architecture/combat-m3-moves-kits.md) ·
[combat-m4-hits.md](../../docs/architecture/combat-m4-hits.md) ·
[input.md](../../docs/architecture/input.md) · [GSTNInput README](../GSTNInput/README.md) · [gas-npp.md](../../docs/architecture/gas-npp.md) ·
[PATCHES.md](../GASNPP/PATCHES.md)
