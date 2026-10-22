# GSTNInputNP

The Network Prediction adapter of [GSTNInput](../GSTNInput/README.md). It was the `GSTNInputNP` module of the `GSTNInput`
plugin and is now its own plugin so that `GSTNInput` (`GSTNInputCore`) works in a project without the GAS-NPP fork. The
module keeps its name, so class paths (`/Script/GSTNInputNP.*`) and include paths are unchanged. Design:
[docs/architecture/input.md](../../docs/architecture/input.md) (section 5).

Status: **experimental (0.1)** (`IsExperimentalVersion` in the `.uplugin`). It is a project plugin, not a fork of an engine
plugin, and contains no content (`CanContainContent: false`).

## Modules

| Module | Type / phase | Contents |
|---|---|---|
| `GSTNInputNP` | Runtime, `Default` | The whole plugin (one module). Public headers in `Source/GSTNInputNP/Public/{Simulation,Bindings,Sources,Context,Mover}/`, implementations in `Private/`, tests in `Private/Tests/`. Export macro `GSTNINPUTNP_API`, log category `LogGSTNInput` (shared with `GSTNInputCore`). |

## What it contains

`FGSTN_InputCmd` and `FGSTN_InputSimState` (`Simulation/GSTN_InputSimTypes.h`), `FGSTN_InputSimulator` and `IGSTN_InputSimHost`
(`Simulation/GSTN_InputSimulator.h`), `UGSTN_CommandBindingSet` and the runtime `FGSTN_CommandTable` (`Bindings/`), the player and
AI input sources (`Sources/`), the context provider base, the Blueprint provider and the Mover provider (`Context/`),
`UGSTN_MoverSimStateTracker` (`Mover/`), the activation context and `IGSTN_CommandSourceInterface`
(`Simulation/GSTN_CommandSourceInterface.h`), the host helpers (`Simulation/GSTN_InputHostUtils.h`) and
the standalone `UGSTN_InputSimComponent` (`Simulation/GSTN_InputSimComponent.h`, model `GSTN_Input`). How to embed the pieces in a
host model: "How to wire it" below.

| Piece | Header | Role |
|---|---|---|
| `FGSTN_InputCmd` | `Simulation/GSTN_InputSimTypes.h` | The Network Prediction input type: held/pressed button masks, raw stick, camera yaw, forced command. |
| `FGSTN_InputSimState` | `Simulation/GSTN_InputSimTypes.h` | The embeddable sync state: input history, classifier, watched tags, the frame-stamped request (`RequestedBinding`, `RequestButton`, `RequestFrame`), `PrevContext`. |
| `FGSTN_InputSimulator`, `IGSTN_InputSimHost`, `FGSTN_InputTickArgs/Result` | `Simulation/GSTN_InputSimulator.h` | The stateless per-frame simulation and the plain C++ interface the host implements. |
| `UGSTN_CommandBindingSet`, `FGSTN_CommandBinding`, `FGSTN_CommandTable` | `Bindings/GSTN_CommandBindingSet.h` | The data asset (slot sets, slots, bindings command -> requirements -> ability, reset rules) and the table `BuildTable` compiles once at setup. |
| `IGSTN_InputSource`, `UGSTN_PlayerInputSource`, `UGSTN_AIInputSource` | `Sources/GSTN_InputSource.h` | Who fills the command each tick: Enhanced Input for the local player, code (`SetHeld`, `Tap`, `SetStick`, `RequestCommand`) for AI, scripts and tests. |
| `UGSTN_InputContextProvider`, `UGSTN_BlueprintInputContextProvider`, `UGSTN_MoverInputContextProvider` | `Context/GSTN_InputContextProvider.h` | Facing, `Input.Context.*` tags, context fingerprint and `Input.BufferEvent.Landed` from the movement system (end of the previous frame). |
| `UGSTN_MoverSimStateTracker` | `Mover/GSTN_MoverSimStateTracker.h` | Mover's sync state of the frame being (re)simulated, for everything that runs in the fixed tick before Mover. |
| `FGSTN_ActivationContext`, `EGSTN_ActivationSource`, `IGSTN_CommandSourceInterface` | `Simulation/GSTN_CommandSourceInterface.h` | What abilities ask the host: why they started, whether their button is still held. |
| `FGSTN_InputHostUtils` | `Simulation/GSTN_InputHostUtils.h` | `CanExecuteBinding`, `ExecuteRequest`, `IsButtonHeld`, `PublishOutbox`: the host code every model shares. |
| `UGSTN_InputSimComponent` | `Simulation/GSTN_InputSimComponent.h` | The standalone Network Prediction model (`FGSTN_InputModelDef`, `GSTN_Input`) that hosts the simulation with nothing added. |

## Dependencies

- Plugins (all non-optional, see `GSTNInputNP.uplugin`): `GSTNInput`, GameplayAbilities, AbilitySystemSimulation,
  NetworkPrediction (the GAS-NPP fork in `Plugins/GASNPP`), Mover, EnhancedInput.
- Module `GSTNInputNP` (`Source/GSTNInputNP/GSTNInputNP.Build.cs`): public `Core`, `CoreUObject`, `Engine`, `GameplayTags`,
  `GSTNInputCore` (plugin `GSTNInput`), `GameplayAbilities`, `AbilitySystemSimulation`, `NetworkPrediction`, `NetCore`, `Mover`;
  private `EnhancedInput` (only the player input source uses it, no public header exposes it). Nothing in `GSTNInput` depends on
  this plugin.
- Who depends on it: the plugin `GSTNCombatNP` (its `.uplugin` and `GSTNCombatNP.Build.cs`), and the project module
  `GASP_Giustino2` (`Source/GASP_Giustino2/GASP_Giustino2.Build.cs`). The project's `.uproject` enables it.
- Dependency rule: an `*NP` module may depend on another plugin's `*NP` module of the same layer, never on anything above it.
  `GSTNCombatNP` (its own plugin, `Plugins/GSTNCombatNP/`) depends on `GSTNInputNP` and may use only its Public headers and
  interfaces.

## Tags and settings

This plugin declares no gameplay tags and owns no settings class: tags and `UGSTN_InputSettings` live in `GSTNInputCore`
(`Plugins/GSTNInput/Config/DefaultGSTNInput.ini`). Its only config is `Config/DefaultGSTNInputNP.ini`, the `[CoreRedirects]`
of the Mover tracker's old class names (`/Script/GASP_Giustino2.*` and `/Script/GSTNCombatNP.*`). The tags its code reads or
writes (`Input.Context.*`, `Input.BufferEvent.Landed`, `Input.Lock.Command*`, `Input.Event.CommandExecuted`) are the Core
module's native tags; the Mover provider also copies Mover's own mode tags (`MoverTags`). The only `UE_DEFINE_GAMEPLAY_TAG*`
in this plugin are the test-only `Input.Button.NPTest.*`, `Command.NPTest.*` and `Input.Lock.Command.NPTest.Attack` tags
inside `Private/Tests/GSTN_InputNPTests_Simulator.cpp`.

## How to wire it

### Network Prediction sort priorities

 Network Prediction orders models by a sort priority and warns when two registered
models share one. The ability system is `PreKinematicMovers - 2`, the combat model (`GSTN_Combat`) `- 3` and the standalone
input model (`GSTN_Input`, `UGSTN_InputSimComponent`) `- 4`. The input-only and combat models are never on the same pawn,
but the check is global, hence three different numbers. A new model of this family takes the next free one below.

### Embedding the input simulation in a model

`GSTNInputNP` does not own a Network Prediction model; a host model embeds its pieces (the combat plugin's
`UGSTN_CombatSimComponent` is the first host). Use `FGSTN_InputCmd` as the model's input type and put an
`FGSTN_InputSimState` member in its sync state, forwarding the three calls:

```cpp
struct FGame_SyncState
{
	FGSTN_InputSimState Input;

	void NetSerialize(const FNetSerializeParams& P) { Input.NetSerialize(P.Ar); }
	bool ShouldReconcile(const FGame_SyncState& Auth) const { Input.ShouldReconcile(Auth.Input); return false; }
	void ToString(FAnsiStringBuilderBase& Out) const { Input.ToString(Out); Out.Append("\n"); }
};
```

Add your own fields after the embedded member and serialize them there. Everything that touches this state must be
deterministic and rollback-safe (gas-npp.md). Module dependencies: `GSTNInputNP` in the host's `Build.cs`.

Then call `FGSTN_InputSimulator::Tick` from the model's simulation tick with a copy of the state as `Out`, and implement
`IGSTN_InputSimHost` (owner tags, facing, `CanExecuteBinding`; the context provider covers the Mover parts). Build the
`FGSTN_CommandTable` once from a `UGSTN_CommandBindingSet` at BeginPlay, and execute `RequestedBinding` from the ability
system's pre-tick when `RequestFrame` equals the frame. `UGSTN_CombatSimComponent` is a working example.

**Bindings must not depend on the net role.** Press consumption follows `CanExecuteBinding`, which asks the ability's
`CanActivateAbility`; its answer lands in the rolled back input state, so the client and the server must give the same
answer. Therefore (1) abilities with a `ServerOnly` or `ServerOnlyExecution` net security policy cannot be bound
(`UGSTN_CommandBindingSet::IsBindableAbility`; `BuildTable` drops and logs such a binding, `IsDataValid` reports it), because
the engine's `ShouldActivateAbility(GetLocalRole())` refuses them on a client, and (2) `CanActivateAbility` overrides of
bound abilities must not read the role, local control or any state outside the rolled back ones. TODO: a role free check
in the fork would lift both limits.

**Stick-only commands.** A command whose terminal step is a motion or a spin has no press to consume. The simulator
therefore records the terminal stick anchor in `FGSTN_InputHistory` (`ConsumeSticksUpTo`, through
`FGSTN_SlotResolution::ConsumeFrom`) and the matcher ignores terminal stick anchors at or before it, so such a command fires
once per motion, not on every frame of its time buffer. The frame is part of the history on the wire (see the core README).

Give the host an `FGSTN_SimEventOutbox`, pass it in `FGSTN_InputTickArgs::Outbox`, and call
`FGSTN_InputHostUtils::PublishOutbox(Outbox, Owner)` from the model's `FinalizeFrame`; use `FGSTN_InputHostUtils::CanExecuteBinding`
and `ExecuteRequest` for the ability system parts. Implement `IGSTN_CommandSourceInterface` so abilities can ask why they started.
A host that needs nothing of its own does not have to write any of this: add `UGSTN_InputSimComponent` (below).

### The standalone component

`UGSTN_InputSimComponent` is the input simulation as a Network Prediction model of its own, for a project without the combat
plugin: add it next to a `UNpAbilitySystemComponent` on a pawn, set `InputConfig` (buttons, move action) and `SlotBindings`
(a `UGSTN_CommandBindingSet`: slots, bindings to abilities, reset rules), keep the default `UGSTN_PlayerInputSource` (or use
`UGSTN_AIInputSource` for server-driven pawns) and the default `UGSTN_MoverInputContextProvider`. It registers its model
before the ability system, executes requests from the ability system's pre-tick and publishes `Input.Event.CommandExecuted`.
A pawn uses **either** this component or a host that embeds the simulation (the combat component): with both, this one logs
an error at BeginPlay and does nothing. Not done yet: projecting `Input.Context.*` onto the ability system.

### Activation context and the buttons an ability holds

An ability that must end when its trigger button is released finds the component through
`IGSTN_CommandSourceInterface::Find(Avatar)` and calls, from `ActivateAbility` or its tick, `GetActivationContext()`
(the command, the trigger button, the frame of the press and whether a command or a forced request started it; valid only on
the activation frame, so keep what you need in your own sync variables) and `IsCommandButtonHeld(Button)`.

### The Mover sim-state tracker

`UGSTN_MoverSimStateTracker::FindOrAdd(MoverComponent)` gives simulation code the Mover state of the frame being
(re)simulated; never read the Mover component's own queries from the fixed tick (gas-npp.md). Include
`"Mover/GSTN_MoverSimStateTracker.h"` and depend on `GSTNInputNP`. The tracker is a `BlueprintType` and its
`GetMovementModeName` / `HasGameplayTag` are `BlueprintPure`, so a Blueprint ability can read the same rolled back state.

## How to extend

### In C++

- **A new host model.** Implement `IGSTN_InputSimHost` (plain C++, not a UInterface) on your `UNetworkPredictionComponent`, embed
  `FGSTN_InputSimState` in its sync state and use `FGSTN_InputCmd` as its input type, as in "Embedding the input simulation in
  a model" above. `IsCommandAllowed(CommandIndex)` is the host's own gating (a cancel window, hitstun); command locks are the
  simulator's and need no host code. Use `FGSTN_InputHostUtils` for the ability system parts and implement
  `IGSTN_CommandSourceInterface` so abilities find you with `Find(Avatar)`. Give the model its own sort priority (next free
  number below `PreKinematicMovers - 4`).
- **A new movement system.** Subclass `UGSTN_InputContextProvider` (abstract, `EditInlineNew`) and override `Initialize`,
  `GetFacingYaw`, `AppendContextTags`, `GetContextFingerprint`, `AppendFrameEvents`; set it as the host's `ContextProvider`.
  Everything it returns must come from rolled back state (a tracker like `UGSTN_MoverSimStateTracker`), never from a component
  query, because it runs inside the simulation.
- **A new input source.** Implement the UInterface `IGSTN_InputSource` (`ProduceInput(Context, DeltaTimeMS, Out)`) on a
  `UObject`; `UGSTN_PlayerInputSource` and `UGSTN_AIInputSource` are the two shipped ones. A source runs only on the machine
  that controls the pawn, never in a re-simulation.
- **New command conditions, priority criteria and reset rules** are not extension points of this plugin: they are the
  `TInstancedStruct` families of `GSTNInputCore` (`FGSTN_InputCondition`, `FGSTN_PriorityCriterion`, `FGSTN_BufferResetRule`,
  input.md section 4), consumed here through `UGSTN_CommandBindingSet::ResetRules` and the hosts' `ResetRules`.
- **A different binding asset.** `UGSTN_CommandBindingSet` is the default data; a host may build its `FGSTN_CommandTable` from
  anything else (the combat plugin plans its compiled loadout table) and call the simulator the same way.

**What must stay C++:** `FGSTN_InputSimulator`, `IGSTN_InputSimHost`, the sync/input types (`FGSTN_InputSimState`,
`FGSTN_InputCmd`: plain structs, no reflection), `IGSTN_InputSource::ProduceInput`, and the base `UGSTN_InputContextProvider`
virtuals (they run every simulated tick of every pawn). `UGSTN_InputSimComponent` is not `Blueprintable`: subclass it in C++.

### In Blueprint

- **Data.** `UGSTN_CommandBindingSet` is a `BlueprintType` data asset: slots, slot sets, `FGSTN_CommandBinding`
  (`Command`, `Requirements`, soft `Ability` class, which must be the granted class exactly), reset rules. `UGSTN_InputConfig`
  (buttons, `MoveAction`) comes from `GSTNInputCore`.
- **Context provider in Blueprint.** `UGSTN_BlueprintInputContextProvider` is `Blueprintable`: implement `Initialize`
  (`BlueprintImplementableEvent`, BeginPlay, not simulation) and the const `BlueprintNativeEvent`s `GetBlueprintFacingYaw`,
  `GetBlueprintContextTags`, `GetBlueprintContextFingerprint`, `GetBlueprintFrameEvents`. Same determinism rules as C++: read
  only `UGSTN_MoverSimStateTracker` or state rebuilt in the same call, no world queries, no time, no random. It is the slower
  path (Blueprint VM every simulated tick); a movement system every character shares belongs in a C++ provider.
- **Reading state.** `UGSTN_MoverSimStateTracker::GetMovementModeName` / `HasGameplayTag` and
  `UGSTN_InputSimComponent::GetActivationContext` / `IsCommandButtonHeld` are `BlueprintPure`; `FGSTN_ActivationContext` and
  `EGSTN_ActivationSource` are Blueprint types (`Button` is an `int32` for that reason). `GetDebugString` is `BlueprintCallable`
  for logs.
- **Driving an AI pawn.** `UGSTN_AIInputSource::SetHeld`, `Tap`, `SetStick`, `RequestCommand` are `BlueprintCallable`, so a
  behaviour tree task or a Blueprint script can feed the simulation.
- **Components.** `UGSTN_InputSimComponent` is `BlueprintSpawnableComponent` (addable in a Blueprint pawn, properties
  `EditDefaultsOnly`), `UGSTN_PlayerInputSource` / `UGSTN_AIInputSource` and the providers are `EditInlineNew` instanced objects
  set on it. There is no Blueprint hook on the component's simulation tick, by design.

## Using it standalone in another project

1. Copy `Plugins/GSTNInput` (the Core) and `Plugins/GSTNInputNP`, and enable both in the `.uproject` (the NP plugin's
   descriptor pulls in `GSTNInput`, `GameplayAbilities`, `AbilitySystemSimulation`, `NetworkPrediction`, `EnhancedInput` and
   `Mover`, all required).
2. **Blocker: it needs the GAS-NPP fork.** `GameplayAbilities`, `AbilitySystemSimulation` and `NetworkPrediction` must be the
   forked plugins from `Plugins/GASNPP/` (see `Plugins/GASNPP/PATCHES.md` and [gas-npp.md](../../docs/architecture/gas-npp.md)):
   the component binds the fork's `OnSimulationPreTick` on `UNpAbilitySystemComponent` and the bindings activate
   `UNpGameplayAbility` children. Stock engine plugins do not build it. Mover is the stock engine plugin, but this project keeps
   an override in `Plugins/EngineOverrides/Mover/`; the tracker only binds Mover's public `OnPostMovement` and
   `OnPostSimulationRollback` delegates.
3. Put `UGSTN_InputSimComponent` next to a `UNpAbilitySystemComponent` (and a `UMoverComponent` if you keep the default
   `UGSTN_MoverInputContextProvider`) on the pawn; set `InputConfig` and `SlotBindings`; grant the bound abilities to the ability
   system. Enhanced Input mapping contexts for the buttons must be active for the player (the fork's own ability input is not
   involved).
4. Config: nothing of its own beyond `Config/DefaultGSTNInputNP.ini` (class redirects you may drop in a new project). Tuning is
   `UGSTN_InputSettings` in the project's `Config/DefaultGSTNInput.ini` (defaults ship in `Plugins/GSTNInput/Config/`).
5. A project module that includes its headers lists `GSTNInputNP` (and `GSTNInputCore`) in its `Build.cs`.
6. Not needed: the combat plugins. Without them the component runs the simulation on its own; with `GSTNCombatNP`, use
   `UGSTN_CombatSimComponent` instead (never both on one pawn).

Assumptions: fixed-tick Network Prediction, one ability system component per pawn, Mover's default mode names (`Walking`,
`Falling`, `Flying`) and tags (`Mover_IsOnGround`, `Mover_IsInAir`, `Mover_IsFalling`, `Mover_IsFlying`, `Mover_IsCrouching`) for the
provider's default lists (a project's own modes, e.g. wall modes, go in `WallModes`), and that nothing reads the Mover
component's own queries from the fixed tick. Known gap: `Input.Context.*` tags are not projected onto the ability system yet,
so only binding requirements see them, not the abilities' own tag requirements.

## Tests

Automation tests `GSTNInput.NP.*` (`Source/GSTNInputNP/Private/Tests/`: `Bindings.Table`, `Simulator.*` (buffer, request,
forced, locks, unbound commands, `CommandExecutedEvent`), `Context.ActivationContext`, `Context.BlueprintProvider`,
`Component.Standalone`), run with the usual `Automation RunTests GSTNInput+GSTNCombat` (filter by name prefix, so the plugin
move did not change it). See [testing.md](../../docs/architecture/testing.md).

## Where to read more

- [input.md](../../docs/architecture/input.md): section 2 (modules and the dependency rule), 3 (tags), 5 (`GSTNInputNP`: 5.1
  simulation pieces, 5.2 context and sources, 5.3 abilities' view, 5.4 standalone component, 5.5/5.6 as implemented, 5.7 sort
  priorities), 6 (authoritative events).
- [gas-npp.md](../../docs/architecture/gas-npp.md): the fork, determinism and rollback rules, the pre-tick hook.
- [combat.md](../../docs/architecture/combat.md): the first host (`UGSTN_CombatSimComponent`) and the input behaviour
  specification (sections 6-9) this plugin implements.
- [testing.md](../../docs/architecture/testing.md): running the automation tests and the PIE network harness.
- Sibling READMEs: [GSTNInput](../GSTNInput/README.md) (Core), [GSTNCombatNP](../GSTNCombatNP/README.md) (the combat host).
