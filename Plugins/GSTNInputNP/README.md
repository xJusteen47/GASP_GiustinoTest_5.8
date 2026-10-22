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
- Dependency rule: an `*NP` module may depend on another plugin's `*NP` module of the same layer, never on anything above it.
  `GSTNCombatNP` (plugin `GSTNCombat`) depends on `GSTNInputNP` and may use only its Public headers and interfaces.

## Tags and settings

This plugin declares no gameplay tags and owns no settings class: tags and `UGSTN_InputSettings` live in `GSTNInputCore`
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

## Tests

Automation tests `GSTNInput.NP.*` (`Source/GSTNInputNP/Private/Tests/`), run with the usual `Automation RunTests GSTNInput+GSTNCombat`
(filter by name prefix, so the plugin move did not change it). See [testing.md](../../docs/architecture/testing.md).
