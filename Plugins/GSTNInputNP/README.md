# GSTNInputNP

The Network Prediction adapter of [GSTNInput](../GSTNInput/README.md). It was the `GSTNInputNP` module of the `GSTNInput`
plugin and is now its own plugin so that `GSTNInput` (`GSTNInputCore`) works in a project without the GAS-NPP fork. The
module keeps its name, so class paths (`/Script/GSTNInputNP.*`) and include paths are unchanged. Design:
[docs/architecture/input.md](../../docs/architecture/input.md) (section 5).

Status: **experimental (0.1)**.

## What it contains

`FGSTN_InputCmd` and `FGSTN_InputSimState` (`Simulation/GSTN_InputSimTypes.h`), `FGSTN_InputSimulator` and `IGSTN_InputSimHost`
(`Simulation/GSTN_InputSimulator.h`), `UGSTN_CommandBindingSet` and the runtime `FGSTN_CommandTable` (`Bindings/`), the player and
AI input sources (`Sources/`), the context provider base and the Mover provider (`Context/`), `UGSTN_MoverSimStateTracker`
(`Mover/`), the activation context and `IGSTN_CommandSourceInterface`, the host helpers (`Simulation/GSTN_InputHostUtils.h`) and
the standalone `UGSTN_InputSimComponent` (model `GSTN_Input`). How to embed the pieces in a host model: "How to wire it" below.

## Dependencies

- Plugins (all non-optional, see `GSTNInputNP.uplugin`): `GSTNInput`, GameplayAbilities, AbilitySystemSimulation,
  NetworkPrediction (the GAS-NPP fork in `Plugins/GASNPP`), Mover, EnhancedInput.
- Module `GSTNInputNP` depends on `GSTNInputCore` (plugin `GSTNInput`) and the fork modules. Nothing in `GSTNInput` depends on
  this plugin.
- The hard dependency on Mover is a deliberate bridge, not a leak: the module is the adapter between the pure input logic and
  the simulation. Mover appears in two places only: `UGSTN_MoverSimStateTracker` (`Mover/`, the rollback-safe copy of the
  character's simulated mode and tags) and `UGSTN_MoverInputContextProvider` (`Context/`, which turns that into the
  grounded/airborne/on-wall context and the landing event). The simulator, the binding set and the input sync state never
  touch Mover. A game with another movement system subclasses `UGSTN_InputContextProvider` (C++ or Blueprint) and does not
  use those two classes; if a Mover-free build is ever needed, they are the pieces to move into their own bridge module.
- Dependency rule: an `*NP` module may depend on another plugin's `*NP` module of the same layer, never on anything above it.
  `GSTNCombatNP` (plugin `GSTNCombat`) depends on `GSTNInputNP` and may use only its Public headers and interfaces.

## Tags and settings

This plugin declares no runtime gameplay tags (only the Core automation tests declare test tags) and owns no settings class: tags and `UGSTN_InputSettings` live in `GSTNInputCore`
(`Plugins/GSTNInput/Config/DefaultGSTNInput.ini`). Its only config is `Config/DefaultGSTNInputNP.ini`, the `[CoreRedirects]`
of the Mover tracker's old class names (`/Script/GASP_Giustino2.*` and `/Script/GSTNCombatNP.*`).

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
	bool ShouldReconcile(const FGame_SyncState& Auth) const { return Input.ShouldReconcile(Auth.Input); }
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

### Camera aim

`FGSTN_InputCmd` can carry the aim of the controller (`bHasAim`, `AimYaw`, `AimPitch`; 65536 steps per turn, so the
resolution is 0.0055 degrees, against the 1.4 degrees of the `CameraYaw` byte the direction classifier uses). The player source
fills it when `UGSTN_InputConfig::bSendCameraAim` is on, the AI source after `SetAim`. Off, the command costs one bit more; on, it
costs two words more, every command (the NP input buffer gives no previous command to delta against). The simulation host
reads it for an ability (`UGSTN_CombatSimComponent::GetAimRotation`, combat-m3 7.10).

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
It also exposes the local gravity frame and the launch signal, all from the same frame as the other fields: `GetUpDirection()` / `GetGravityDirection()` (Mover's up direction, quantised to 1/4096, never world Z), `GetVelocity()`, `GetUpSpeed()` (velocity along up) and `WasExternallyLaunchedThisFrame()` (the last step started a `FLayeredMove_Launch` or child, derived from the restored sync state so a re-simulation agrees; the jump impulse is an instant effect and never sets it).

## Types the host and the sources use

- `IGSTN_InputSource` (`UGSTN_InputSource` is its `UInterface`): `ProduceInput(const FGSTN_InputSourceContext&, DeltaTimeMS, FGSTN_InputCmd&)` fills the
  command of one fixed tick on the machine that controls the pawn. `FGSTN_InputSourceContext` is what it may read (the owner, the input config and the
  command table). `UGSTN_PlayerInputSource` and `UGSTN_AIInputSource` are the two shipped sources.
- `FGSTN_InputModelSyncState` / `FGSTN_InputModelAuxState` (`GSTN_InputSimComponent.h`): the standalone component's model state, the input sim state
  plus nothing else; the combat model embeds `FGSTN_InputSimState` instead.
- `FGSTN_InputTickResult` (`GSTN_InputSimulator.h`): what one `FGSTN_InputSimulator::Tick` returns to its host (the pressed buttons, the request, the binding index).
- `FGSTN_CommandBinding` and `FGSTN_CompiledBinding` (`Bindings/GSTN_CommandBindingSet.h`): an authored binding (command, ability, requirements) and its
  compiled form in the command table the simulation reads.
- `UGSTN_BlueprintInputContextProvider` (`Context/`): the context provider written in Blueprint, the slower path (see its header for the rollback rules).

## How to extend

- **A new input source** (replay, network bot, test script): a `UObject` that implements `IGSTN_InputSource` and fills the command in `ProduceInput`
  from nothing but its own state and the context; set it as the host's `InputSource`. It runs only on the controlling machine, and the command it
  makes is what the server and every re-simulation replay, so it must not read anything the simulation also writes.
- **A context provider for a non-Mover movement system**: derive from `UGSTN_InputContextProvider`, implement `Initialize` (bind to the owner),
  `GetFacingYaw`, `AppendContextTags` (the `Input.Context.*` tags), `GetContextFingerprint` (a small summary, 0 = unknown) and `AppendFrameEvents`
  (the buffer events a change of fingerprint produced). Read only rolled-back state, as the Mover tracker does; assign it to the host's `ContextProvider`.
- **A new host**: implement `IGSTN_InputSimHost` (candidate check, activation context, the held buttons...) on a model that embeds `FGSTN_InputSimState`
  and call `FGSTN_InputSimulator::Tick` each frame; use `FGSTN_InputHostUtils` for the binding check, `ExecuteRequest` from the ability system's
  pre-tick and `PublishOutbox` from `FinalizeFrame`. `UGSTN_InputSimComponent` is the minimal example, `UGSTN_CombatSimComponent` the full one.

## Tests

Automation tests `GSTNInput.NP.*` (`Source/GSTNInputNP/Private/Tests/`), run with the usual `Automation RunTests GSTNInput+GSTNCombat`
(filter by name prefix, so the plugin move did not change it). See [testing.md](../../docs/architecture/testing.md).
