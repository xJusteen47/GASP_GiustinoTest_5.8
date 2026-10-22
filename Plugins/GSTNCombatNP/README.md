# GSTNCombatNP

The Network Prediction adapter of [GSTNCombat](../GSTNCombat/README.md). It was the `GSTNCombatNP` module of the
`GSTNCombat` plugin and is now its own plugin so that `GSTNCombat` (`GSTNCombatCore`) works in a project without the GAS-NPP
fork. The module keeps its name, so class paths (`/Script/GSTNCombatNP.*`) and include paths are unchanged and no class
redirect is needed. Design: [docs/architecture/combat.md](../../docs/architecture/combat.md).

Status: **experimental (0.1)**.

## What it contains

The combat NP model: `UGSTN_CombatSimComponent` (`Simulation/GSTN_CombatSimComponent.h`, model `GSTN_Combat`, sort priority
`PreKinematicMovers - 3`) and its sync/aux/input types (`Simulation/GSTN_CombatSimTypes.h`): the sync state is `FGSTN_CombatSyncCore` (the embedded input state
and the hitstop window) plus a list of sync fragments (below). It embeds the input command and
the input sim state of [GSTNInputNP](../GSTNInputNP/README.md) and is the host of its simulator (`IGSTN_InputSimHost`), so the
component only adds the debug hitstop. It also implements `IGSTN_CommandSourceInterface` (the activation context and
`IsCommandButtonHeld` for abilities), honours the command locks (`Input.Lock.Command[.<root>]`) and publishes
`Input.Event.CommandExecuted` through `UGSTN_SimEventSubsystem` on the authority only (the listen-server host's own pawn
included). Everything that touches the ability system goes through here. `UGSTN_CombatSimComponent` is `Blueprintable` so a
project can subclass it for its own defaults; its `InputConfig` and `SlotBindings` are soft references resolved once in
`BeginPlay`, and its simulation hooks stay in C++.

## Sync fragments and tick extensions

**Sync state.** `FGSTN_CombatSyncState` is `Core` (`FGSTN_CombatSyncCore`: the input state and the hitstop window, serialized exactly as before) followed by
`Fragments`, a list of `FGSTN_SyncFragment` from `GSTNCombatCore` that features add per entity. The core's bytes are the same with
or without fragments, and an entity with none pays one bit for the list (the format and the reason it is length-prefixed are in the
[GSTNCombat README](../GSTNCombat/README.md)). A fragment mismatch against the authority asks for a reconcile like the hitstop window
does; the input state's mismatches are still reported to the trace only. Interpolated proxies take the newer core, and each fragment
decides how it blends (default: the newer one). No fragment type exists yet.

**Tick extensions.** `IGSTN_CombatTickExtension` (`Simulation/GSTN_CombatTickExtension.h`) is the C++ per-frame hook an optional plugin registers in
`FGSTN_CombatTickExtensionRegistry::Get()`. The combat tick calls the registry at four fixed points (`EGSTN_CombatHookPoint`:
`AfterClocks`, `BeforeCandidates`, `AfterExecute`, `BeforeTagProjection`, the steps of combat-m3 7.2); with nothing registered each call is an empty loop.
Extensions run ordered by `GetPriority()` (lower first), ties by `GetTypeId()` (a `GSTNFeatureIds` value), so the order is the same on every
machine whatever the plugin load order; two extensions with one type id are refused. An extension is simulation code: deterministic, its state only in
its own sync fragment, no component, world or ability system reads, no net-mode branches. Until the timeline clock and tag projection exist
in the model the four points sit around the input simulator call (which does candidates, resolution and the request in one function); their meaning
is fixed and the call sites move with the model.

## Dependencies

- Plugins (all non-optional, see `GSTNCombatNP.uplugin`): `GSTNCombat`, `GSTNInputNP`, `GSTNInput`, GameplayAbilities,
  AbilitySystemSimulation, NetworkPrediction (the GAS-NPP fork in `Plugins/GASNPP`), EnhancedInput, Mover.
- Module `GSTNCombatNP` depends on `GSTNCombatCore`, `GSTNInputCore`, `GSTNInputNP` and the fork modules. Nothing in `GSTNCombat`
  depends on this plugin.
- Dependency rule: an `*NP` module may depend on another plugin's `*NP` module of the same layer, and on nothing above it.
  `GSTNCombatNP` depends on `GSTNInputNP` by design (one NP model per character embeds the input simulation) and may use only its
  Public headers and interfaces (for example `IGSTN_InputSimHost`), never its internals. Core modules keep no NP, GAS or Mover
  dependencies.

## Tags and settings

This plugin declares no gameplay tags and owns no settings class and no config file: the combat tags, lock tags and
`UGSTN_CombatSettings` live in `GSTNCombatCore` (`Plugins/GSTNCombat/Config/DefaultGSTNCombat.ini`), the input tags and
settings in `GSTNInputCore`.

## How to wire it

1. Enable the plugin `GSTNCombatNP` (the project's `.uproject` lists it; it pulls in `GSTNInputNP` and the fork).
2. Put `UGSTN_CombatSimComponent` on the pawn next to a `UNpAbilitySystemComponent` and a Mover component, set its
   `InputConfig` and `SlotBindings` (a `UGSTN_CommandBindingSet`), and use `UGSTN_StandardSlots` from `GSTNCombatCore` for the
   default command set. A pawn uses either this component or `UGSTN_InputSimComponent` of `GSTNInputNP`, never both.
3. A project module that uses the component lists `GSTNCombatNP` (and the Core modules it includes from) in its `Build.cs`.

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

## Tests

Automation tests `GSTNCombat.NP.*` in `Source/GSTNCombatNP/Private/Tests/` run without a world, headless, with the rest:
`Automation RunTests GSTNInput+GSTNCombat`.

- `Fragments.{RoundTrip0, RoundTrip1, RoundTrip2, UnknownTypeId}`: the wire format of the combat sync state with zero, one and two fragments
  (the log prints the bits of each; zero fragments are exactly one bit over the core), the array order not affecting the bytes,
  `ShouldReconcile`, `Interpolate` and `ToString` of the state, a fragment of an unknown type id being skipped without moving the reader, and a
  malformed count being an archive error. The fragments used are test-only types with ids from the test range.
- `TickExtension.{OrderByPriority, EmptyHooksNoEffect}`: run order by priority then type id independent of registration order, duplicate
  ids refused, and the hook points changing nothing when no extension or only an observer is registered.

The pure logic is covered by `GSTNCombat.Core.*` (in `GSTNCombatCore`) and `GSTNInput.NP.*` (in `GSTNInputNP`). The network behaviour of the
combat model is checked with the PIE harness `Tools/GASNPPTest/gasnpp_test.py` (dedicated server + 2 clients), scenario
`combat_spike`; any change to the combat sync state needs it (see [testing.md](../../docs/architecture/testing.md)).

## Conventions

Types are prefixed `GSTN_`; export macro `GSTNCOMBATNP_API`. Public headers in `Public/<Area>/`, implementations in
`Private/<Area>/`. Simulation code must be deterministic and rollback-safe (rules in
[gas-npp.md](../../docs/architecture/gas-npp.md)).

## Further reading

[combat.md](../../docs/architecture/combat.md) · [combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) ·
[input.md](../../docs/architecture/input.md) · [GSTNInputNP README](../GSTNInputNP/README.md) ·
[gas-npp.md](../../docs/architecture/gas-npp.md) · [PATCHES.md](../GASNPP/PATCHES.md)
