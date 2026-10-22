# GSTNCombatNP

The Network Prediction adapter of [GSTNCombat](../GSTNCombat/README.md). It was the `GSTNCombatNP` module of the
`GSTNCombat` plugin and is now its own plugin so that `GSTNCombat` (`GSTNCombatCore`) works in a project without the GAS-NPP
fork. The module keeps its name, so class paths (`/Script/GSTNCombatNP.*`) and include paths are unchanged and no class
redirect is needed. Design: [docs/architecture/combat.md](../../docs/architecture/combat.md).

Status: **experimental (0.1)** (`IsExperimentalVersion` in `GSTNCombatNP.uplugin`). No content (`CanContainContent: false`), no
config file, no editor module.

## Modules

One module, `GSTNCombatNP` (`Runtime`, loading phase `Default`), export macro `GSTNCOMBATNP_API`, in
`Source/GSTNCombatNP/`:

| Path | Contents |
|---|---|
| `Public/Simulation/GSTN_CombatSimComponent.h` | `UGSTN_CombatSimComponent`, the combat NP model's driver and simulation (model def `FGSTN_CombatModelDef` is private, in the `.cpp`). |
| `Public/Simulation/GSTN_CombatSimTypes.h` | `FGSTN_CombatSyncCore`, `FGSTN_CombatSyncState`, `FGSTN_CombatAuxState` (empty), `FGSTN_CombatStateTypes` (input `FGSTN_InputCmd` from `GSTNInputNP`). |
| `Public/Simulation/GSTN_CombatTickExtension.h` | `EGSTN_CombatHookPoint`, `FGSTN_CombatTickContext`, `IGSTN_CombatTickExtension`, `FGSTN_CombatTickExtensionRegistry`. |
| `Private/GSTNCombatNPModule.cpp` | `FDefaultModuleImpl`: the module registers nothing at startup. |
| `Private/Tests/` | The `GSTNCombat.NP.*` automation tests and their test-only fragment types (`GSTN_CombatNPTestTypes.h`). |

Console variable: `gstn.Combat.Debug.Input` (draws the input history above the locally controlled pawns, see `GetDebugString`).

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
- Module `GSTNCombatNP` (`GSTNCombatNP.Build.cs`, all public): Core, CoreUObject, Engine, GameplayTags, NetCore, `GSTNInputCore`,
  `GSTNInputNP`, `GSTNCombatCore`, GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover. It does not list
  `GSTNCombatGAS` yet (it will when it first needs a type from there). Nothing in `GSTNCombat`, `GSTNCombatGAS`, `GSTNInput` or
  `GSTNInputNP` depends on this plugin.
- What depends on it: the project module `GASP_Giustino2` (`Source/GASP_Giustino2/GASP_Giustino2.Build.cs`) and the project's
  `.uproject`, which enables the plugin. `Plugins/GSTNInputNP/Config/DefaultGSTNInputNP.ini` carries one class redirect from
  `/Script/GSTNCombatNP.GSTN_MoverSimStateTracker` to `GSTNInputNP`: the tracker used to live here before the input split.
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

Optional properties on the component: `ResetRules` (character-level buffer reset rules, applied before the binding set's own),
`InputSource` (instanced object implementing `IGSTN_InputSource`; default `UGSTN_PlayerInputSource`, use `UGSTN_AIInputSource` for
server-driven pawns), `ContextProvider` (instanced `UGSTN_InputContextProvider`; default `UGSTN_MoverInputContextProvider`, which
needs a Mover component on the owner) and the two `Combat|Spike` fields of the debug hitstop (`HitstopTestButton`, -1 = off,
`HitstopTestFrames`). Missing `InputConfig`, `SlotBindings`, context provider or `UNpAbilitySystemComponent` are logged at
`BeginPlay` (`LogGSTNCombatSim`) and degrade the component rather than fail it: no buttons, no ability starts, facing yaw 0, or
requests ignored.

## How to extend it

**In C++**

- *A per-frame feature with its own state*: a sync fragment (`FGSTN_SyncFragment`, registered in `FGSTN_SyncFragmentRegistry`,
  both in `GSTNCombatCore`) plus an `IGSTN_CombatTickExtension` registered in `FGSTN_CombatTickExtensionRegistry::Get()` from the
  feature module's `StartupModule`, unregistered in `ShutdownModule`. Both share one id from `GSTNFeatureIds`
  (`GSTNCombatCore/Public/Sync/GSTN_SyncFragment.h`). The hook gets `FGSTN_CombatTickContext` (`Frame`, `In`, `Out`) and may write
  only its own fragment in `Out` (`FGSTN_SyncFragmentList::FindOrAdd<T>`). The feature plugin depends on `GSTNCombatNP`; nothing
  here names it.
- *Project defaults for the component*: subclass `UGSTN_CombatSimComponent` (`Blueprintable`) and set the properties in the
  constructor or on the Blueprint child. `SimulationTick`, `ProduceInput`, `FinalizeFrame` and the `IGSTN_InputSimHost` /
  `IGSTN_CommandSourceInterface` overrides are not meant to be overridden by a project: the model's behaviour is shared by every
  machine in a match.
- *A different input producer or movement context*: implement `IGSTN_InputSource` or subclass `UGSTN_InputContextProvider`
  (both in `GSTNInputNP`) and assign the instance to `InputSource` / `ContextProvider`.
- *The command set, abilities, reset rules and input conditions*: data and classes of `GSTNInput`, `GSTNCombat` and
  `GSTNCombatGAS`; see their READMEs. Abilities read `GetActivationContext()` and `IsCommandButtonHeld()` through
  `IGSTN_CommandSourceInterface` (`GSTNInputNP`), so they do not depend on this plugin.

**In Blueprint**

- A Blueprint child of `GSTN_CombatSimComponent` to set its defaults; the component is `BlueprintSpawnableComponent`.
- The `BlueprintPure` reads `GetActivationContext` and `IsCommandButtonHeld` (from an ability's activation or tick only: they read
  the sync state the simulation is on), the `BlueprintCallable` test helpers `SetScriptedButtons`, `ClearScriptedButtons`,
  `TapScriptedButtons`, and `GetDebugString`.
- A movement context written in Blueprint: assign a `UGSTN_BlueprintInputContextProvider` child (`GSTNInputNP`) to
  `ContextProvider`; its rules (rolled-back reads only) are in its header.
- There is no `BlueprintNativeEvent` or `BlueprintImplementableEvent` in this plugin, on purpose: a tick extension runs for every
  entity on every simulated and re-simulated frame. The Blueprint extension point of the combat rules is the pipeline step
  `UGSTN_CombatStep` of `GSTNCombatCore` (see the [GSTNCombat README](../GSTNCombat/README.md)).

What must stay C++: the model definition and sort priority, the sync state and its fragments, `IGSTN_CombatTickExtension`,
and everything called from `SimulationTick` or `HandleAbilitySimulationPreTick` (rules in
[gas-npp.md](../../docs/architecture/gas-npp.md)).

## Using it in another project

Copy `Plugins/GSTNCombatNP` together with `Plugins/GSTNInput`, `Plugins/GSTNInputNP`, `Plugins/GSTNCombat` and the GAS-NPP fork
(`Plugins/GASNPP`: `GameplayAbilities`, `AbilitySystemSimulation`, `NetworkPrediction`), and enable them plus `EnhancedInput` and
`Mover` in the `.uproject`. Blockers and assumptions:

- It compiles only against the fork: `UNpAbilitySystemComponent::OnSimulationPreTick`, `GetPlayingSyncedMontage`,
  `PauseCurrentSyncedMontage` / `ResumeCurrentSyncedMontage` do not exist in stock GameplayAbilities, and Network Prediction
  is the fork's version. Stock GAS gets no combat simulation from this plugin (a `GSTNCombatStockGAS` is only a note in
  combat.md §3).
- The default `ContextProvider` reads Mover through `UGSTN_MoverSimStateTracker` (`GSTNInputNP`); this project's
  `Plugins/EngineOverrides/Mover` is the Mover it is tested with. Another movement system needs its own
  `UGSTN_InputContextProvider`.
- The pawn needs `UNpAbilitySystemComponent`, a Mover component and the input config, binding set and input actions of
  `GSTNInput` / `GSTNCombat`; the ini of those plugins (`Config/DefaultGSTNInput*.ini`, `DefaultGSTNCombat.ini`) comes with them,
  and the designer command tags are project content (here `Config/Tags/GSTNCommands.ini`). This plugin itself needs no ini entry.
- Three models of this family share the `PreKinematicMovers - 2/-3/-4` priorities; a project model of its own takes the
  next free one below (see the [GSTNInputNP README](../GSTNInputNP/README.md)).
- Nothing here refers to `/Game/` content, to the project module or to a project tag.

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

- [combat.md](../../docs/architecture/combat.md): §3 modules, §5 simulation architecture (tick order, communication rule,
  authoritative events), §16 project integration.
- [combat-m3-moves-kits.md](../../docs/architecture/combat-m3-moves-kits.md): §3 what this module gains in M3, §7 combat model
  changes (7.2 is the per-frame order the hook points follow, 7.3 the pre-tick execution).
- [combat-m2-sim-input.md](../../docs/architecture/combat-m2-sim-input.md) and [input.md](../../docs/architecture/input.md):
  the embedded input simulation.
- [gas-npp.md](../../docs/architecture/gas-npp.md): the fork, tick order and the hard rules for simulation code;
  [PATCHES.md](../GASNPP/PATCHES.md): the fork patches this plugin relies on (`OnSimulationPreTick`).
- Sibling READMEs: [GSTNCombat](../GSTNCombat/README.md), [GSTNCombatGAS](../GSTNCombatGAS/README.md),
  [GSTNInputNP](../GSTNInputNP/README.md), [GSTNInput](../GSTNInput/README.md).
