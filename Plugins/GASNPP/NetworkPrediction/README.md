# NetworkPrediction (GAS-NPP fork)

Epic's Network Prediction plugin ("NPP"): a framework for client-side prediction where gameplay code is written as a
deterministic `SimulationTick` over three state structs (input, sync, aux) and the framework does the networking: sending
input, buffering, interpolation, corrections, rollback and re-simulation. In this project it is the fixed-tick clock
(62.5 Hz) that every simulated model runs on: the ability system of
[AbilitySystemSimulation](../AbilitySystemSimulation/), Mover (the override in `Plugins/EngineOverrides/Mover`), the
input model of [GSTNInputNP](../../GSTNInputNP/README.md) and the combat model of
[GSTNCombatNP](../../GSTNCombatNP/README.md). This copy is a fork of the engine plugin, ported to UE 5.8 together with
the rest of `Plugins/GASNPP/`, with a few project patches on top (below).

Status: **beta** (`NetworkPrediction.uplugin` version `0.1`, `IsBetaVersion: true`, by Epic Games). It is used by all
network play in this project and verified with the PIE harness (see Tests).

Epic's own overview is kept next to this file in [readme.txt](readme.txt): concepts (input/sync/aux, simulation, driver),
the frame flow of `UNetworkPredictionWorldManager`, fixed versus independent ticking, prediction terminology, cues,
serialization and the list of known gaps (no smoothing of corrections, reflection-free boilerplate, no throttling of
fixed steps, sparse aux state not implemented, async/physics NPP removed). Read it once for the mental model. Two of its
pointers are stale in this copy: the "NetworkPredictionExamples" plugin and `TestMap_Empty` are not here (the samples are
[NetworkPredictionExtras](../NetworkPredictionExtras/README.md), without content), and `NetworkPredictionPhysicsComponent.h`
/ `MockPhysicsSimulation.h` no longer exist.

## Modules

One module, `NetworkPrediction` (Runtime, loading phase `Default`, `Source/NetworkPrediction/`). The headers a user of
the plugin touches, all in `Public/`:

| Header | What it holds |
|---|---|
| `NetworkPredictionStateTypes.h`, `NetworkPredictionSimulation.h`, `NetworkPredictionTickState.h` | `TNetworkPredictionStateTypes<Input, Sync, Aux>`, the `TNetSimInput` / `TNetSimOutput` views a `SimulationTick` receives, `FNetSimTimeStep` (`StepMS`, `TotalSimulationTime`, `Frame`). |
| `NetworkPredictionModelDef.h`, `NetworkPredictionModelDefRegistry.h` | `FNetworkPredictionModelDef` (the model: state types, `Simulation`, `Driver`, name, `ENetworkPredictionSortPriority`), `NP_MODEL_BODY()` and `NP_MODEL_REGISTER()`. |
| `NetworkPredictionDriver.h` | `FNetworkPredictionDriverBase<ModelDef>`: the calls the framework makes on the driver and the state types (`InitializeSimulationState`, `ProduceInput`, `FinalizeFrame`, `RestoreFrame`, `ShouldReconcile`, `Interpolate`, `NetSerialize`, `ToString`, and the fork's `PrepareRepeatedInput`). Specialize it to customize a call without touching the driver class. |
| `NetworkPredictionComponent.h`, `NetworkPredictionProxy.h`, `NetworkPredictionProxyInit.h`, `NetworkPredictionProxyWrite.h` | `UNetworkPredictionComponent`, the abstract actor component that owns an `FNetworkPredictionProxy` and the replication proxies; `Init<ModelDef>` registers an instance, `ProxyWrite` is how game code writes into the pending state outside the tick. |
| `NetworkPredictionPlayerControllerComponent.h` | Per-connection RPC handler and server-side input consumption (`AdvanceLastConsumedFrame`). |
| `NetworkPredictionWorldManager.h`, `Services/*.inl`, `Services/NetworkPredictionServiceRegistry.h` | The world subsystem that runs every fixed tick, and the services (input, ticking, rollback, interpolate, smooth, finalize, server RPC) grouped per model. `Services/NetworkPredictionInstanceData.h` holds the 64-frame ring (`NpInstanceFrameBufferSize`). |
| `NetworkPredictionCues.h`, `NetworkPredictionCueTraits.h` | Networked simulation cues: rollback-aware, non-stateful events invoked from a tick (`NETSIMCUE_BODY`, `NETSIMCUE_REGISTER`, `NETSIMCUESET_REGISTER`). |
| `NetworkPredictionSettings.h`, `NetworkPredictionCVars.h` | `UNetworkPredictionSettingsObject` (`config=NetworkPrediction`, "Network Prediction" in Project Settings) and the `np.*` cvars. |
| `NetworkPredictionTrace.h` | `NetworkPredictionChannel` and the `UE_NP_TRACE_*` macros that feed [NetworkPredictionInsights](../NetworkPredictionInsights/README.md); `UE_NP_TRACE_RECONCILE` is what a `ShouldReconcile` uses to name the field that diverged. |
| `NetworkPredictionLagCompensation.h`, `NePredictionLagCompensationData.h`, `NetworkPredictionGenericKinematicActor.h`, `NetworkPredictionReplicatedManager.h` | Lag-compensation rewind history, a generic kinematic actor driver, the server-spawned replicated manager. |

## Dependencies

- Plugin dependencies: none (`NetworkPrediction.uplugin` lists no plugins).
- Module dependencies (`NetworkPrediction.Build.cs`): public `Core`, `CoreUObject`, `NetCore`, `Engine`, `RenderCore`,
  `PhysicsCore`, `Chaos`, `TraceLog`; private `UnrealEd` in editor targets (PIE delegate only); Iris support through
  `SetupIrisSupport`.
- Depends on it: `AbilitySystemSimulation`, `NetworkPredictionExtras` (+ `NetworkPredictionExtrasLatentLoad`), the Mover
  override (`Plugins/EngineOverrides/Mover`), `GSTNInputNP`, `GSTNCombatNP` and the project module `GASP_Giustino2`.
  `NetworkPredictionInsights` reads its trace channel but does not link the module. The project `.uproject` enables the
  plugin directly.

## Gameplay tags

Declares no gameplay tags.

## Configuration

`UNetworkPredictionSettingsObject` reads `DefaultNetworkPrediction.ini`. The plugin ships one in `Config/` (60 Hz,
`SimulatedProxyNetworkLOD=ForwardPredict`); the project's `Config/DefaultNetworkPrediction.ini` overrides it and is the
one that matters: `PreferredTickingPolicy=Fixed`, `FixedTickFrameRate=62.5` (the rate the fork and the ability system
assume), `SimulatedProxyNetworkLOD=Interpolated`, `bEnableFixedTickSmoothing=True`, interpolation buffer 100 ms,
`FixedTickInputSendCount=6`, `MaximumRemoteInputFaultLimit=6`. Any project that takes this plugin needs the same keys.

Useful cvars (`NetworkPredictionCVars.h` and the services): `np.PrintReconciles`, `np.ForceReconcile`,
`np.ForceReconcileExtraFrames`, `np.SkipReconcile`, `np.ToggleDeltaSerialize`, `np.TimeDilation.*`,
`np.Interpolation.*`, `np.Smoothing.Disable`, `np2.DevMenu` (the DevHUD defined in the settings). The project's own
`np.Spike.*` cvars are listed in PATCHES.md and default to off.

## What this project changed vs. the stock engine plugin

The import (`53c8716`) is a port of the GAS-NPP fork to UE 5.8.3, audited three ways against the 5.7 fork and Epic's
5.8 plugin (`951c495`); there is no stock copy in git to diff against, so only the commits after the import are listed
as project changes. They are all described, with the reason and the file, in [PATCHES.md](../PATCHES.md); this is the
map, not a copy:

| Commit | Area | Where in this plugin |
|---|---|---|
| `d53dff2` | Rollback: every instance's state view points at the restored frame before re-simulation (PATCHES.md 1). | `Public/Services/NetworkPredictionService_Rollback.inl` |
| `951c495` | Rollbacks older than the 64-frame ring are ignored and forced rollbacks clamped; client delta-serialization bases are pruned; `NpInstanceFrameBufferSize` (PATCHES.md 1). | `NetworkPredictionService_Rollback.inl`, `Services/NetworkPredictionInstanceData.h`, `Public/NetworkPredictionSerialization.h` |
| `5efc21a` | Rollback: a server state that maps past the local pending frame (input offset still 0, before the server consumed the client's first input) is skipped instead of compared with a never-simulated slot; a simulated proxy still copies the received input to the head (PATCHES.md 1). | `Public/Services/NetworkPredictionService_Rollback.inl` |
| `5a5fbcd` | Hitch-safe input buffer: the server holds the last frame when starved and skips only past the ring; the client sends one RPC per engine frame; optional `InputType::PrepareRepeatedInput()` hook (PATCHES.md 2). | `Private/NetworkPredictionWorldManager.cpp`, `Private/NetworkPredictionPlayerControllerComponent.cpp` (+ `.h`), `Services/NetworkPredictionService_Input.inl`, `Public/NetworkPredictionDriver.h` |
| `bdfa7e8` | Spike S-FP code, inert: `np.Spike.SPPrepareRepeatedInput`, `np.Spike.LogResimCost` (PATCHES.md 4). | `Services/NetworkPredictionService_Ticking.inl`, `NetworkPredictionWorldManager.cpp/.h` |

In the source the patches are marked `GAS-NPP fork:` or `Spike S-FP:` in a comment (`grep -rn "GAS-NPP fork\|Spike S-FP"
Source/`). Rules for new patches (small, commented, one line in PATCHES.md, no restyling of upstream code) are at the top
of PATCHES.md.

## How to extend: writing a new simulation model

Nothing in this plugin is meant to be subclassed except `UNetworkPredictionComponent`; a project extends it by adding a
model. A model is five things, all C++. The two project models are the reference: the combat model in
`Plugins/GSTNCombatNP/Source/GSTNCombatNP/{Public,Private}/Simulation/GSTN_CombatSim{Types,Component}.{h,cpp}` and the
standalone input model in `Plugins/GSTNInputNP/Source/GSTNInputNP/.../Simulation/GSTN_InputSimComponent.{h,cpp}`. Epic's
minimal one is `UMockNetworkSimulationComponent` in NetworkPredictionExtras (`MockNetworkSimulation.h`).

1. **State structs.** Three plain structs (not `USTRUCT`): the input command, the sync state and the aux state, bundled
   as `using FMyStateTypes = TNetworkPredictionStateTypes<FMyInputCmd, FMySyncState, FMyAuxState>;` (combat:
   `FGSTN_CombatStateTypes` = `FGSTN_InputCmd`, `FGSTN_CombatSyncState`, `FGSTN_CombatAuxState`). Each type is a value
   type the framework copies into its ring, so everything in it is rolled back; anything the tick needs across frames
   lives here and nowhere else. Required members (the driver base forwards to them, `NetworkPredictionDriver.h`):
   - `void NetSerialize(const FNetSerializeParams& P)` on all three: the wire format. Write it by hand and keep it
     compact (`FGSTN_InputCmd` packs buttons with `SerializeIntPacked` and a one-bit "has forced command" flag).
   - `void ToString(FAnsiStringBuilderBase& Out) const` on all three, ANSI only: what Insights and the debug HUD show.
   - `bool ShouldReconcile(const T& Authority) const` on sync and aux: return true when the authority's frame differs
     enough to roll back. Use `UE_NP_TRACE_RECONCILE(condition, "Field:")` per field so the trace names the culprit.
     Compare only what the server can know (the lesson of the montage pause patch in PATCHES.md).
   - `void Interpolate(const T* From, const T* To, float PCT)` on sync and aux: how an interpolated proxy blends two
     frames. Discrete state takes `To` (`FGSTN_CombatSyncState::Interpolate` copies the newer core).
   - Optional, on the input type: `void PrepareRepeatedInput()`. Clear edge-triggered data ("just pressed" bits,
     one-shot commands) and keep held state, because the fork repeats the last input when the server is starved.
     `FGSTN_InputCmd::PrepareRepeatedInput` zeroes `PressedButtons` and `ForcedCommand`. Any input with an edge needs it
     (gas-npp.md rule 9).
   An empty aux state is fine: `FGSTN_CombatAuxState` has all four functions as no-ops.

2. **The simulation.** A class with
   `void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<FMyStateTypes>& Input, const TNetSimOutput<FMyStateTypes>& Output)`.
   `Input.Cmd / Input.Sync / Input.Aux` are the previous frame, `Output.Sync` is the frame being produced (start with
   `*Output.Sync = *Input.Sync`), `Output.Aux` is a lazy writer you only touch when aux changes, `Output.CueDispatch`
   invokes cues. `TimeStep.Frame` is the global frame number and `TimeStep.TotalSimulationTime` the agreed clock: store
   frame numbers, not wall time (the combat hitstop window is `[HitstopStartFrame, HitstopEndFrame)`). The function
   runs on the server, on the predicting client and again on every re-simulation, so it must be deterministic: no
   `HasAuthority`, no "is resimulating", no reads of actor or component state that the ring does not restore, no
   asset loading. Read-only data compiled once before the first tick is allowed (the combat component builds its
   `FGSTN_CommandTable` and `FGSTN_ButtonMap` in `BeginPlay`). In both project models the simulation and the driver are
   the same component; Epic's samples keep them separate, which is also fine.

3. **The driver.** Usually a `UNetworkPredictionComponent` subclass, with:
   - `void InitializeSimulationState(FMySyncState* Sync, FMyAuxState* Aux)`: the first frame, from the actor's state.
   - `void ProduceInput(const int32 DeltaTimeMS, FMyInputCmd* Cmd)`: called on the controlling machine before each
     frame (`PollPerSimFrame`); sample the controller here, never inside the tick. The combat component delegates to an
     `IGSTN_InputSource` (player or AI).
   - `void FinalizeFrame(const FMySyncState* Sync, const FMyAuxState* Aux)`: once per engine frame, push the result to
     the presentation layer. Side effects that must happen once per real frame go here, not in the tick (the combat
     component empties its event outbox here, on the authority only).
   - Optional `RestoreFrame`, `Interpolate`, smoothing hooks: see the banners in `NetworkPredictionDriver.h`.
   - `virtual void InitializeNetworkPredictionProxy() override` that calls
     `NetworkPredictionProxy.Init<FMyModelDef>(GetWorld(), GetReplicationProxies(), this, this);` (simulation and driver
     pointers). `UNetworkPredictionComponent::InitializeComponent` calls it; set `bWantsInitializeComponent = true` and
     `SetIsReplicatedByDefault(true)` in the constructor, as both project components do.
   Game code that needs to change state between ticks (a request from a Blueprint, a server command) writes it through
   the proxy (`NetworkPredictionProxyWrite.h`), not into the component.

4. **The model definition**, in the `.cpp`, next to the registration:

   ```cpp
   class FGSTN_CombatModelDef : public FNetworkPredictionModelDef
   {
   public:
       NP_MODEL_BODY();
       using Simulation = UGSTN_CombatSimComponent;
       using StateTypes = FGSTN_CombatStateTypes;
       using Driver = UGSTN_CombatSimComponent;
       static const TCHAR* GetName() { return TEXT("GSTN_Combat"); }
       static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers - 3; }
   };
   NP_MODEL_REGISTER(FGSTN_CombatModelDef);
   ```

   `NP_MODEL_REGISTER` registers the type at static-init time; `FNetworkPredictionModelDefRegistry::FinalizeTypes` then
   assigns ids by sort priority. The sort priority is the order models tick within one fixed step, identical on every
   machine, and it is checked globally: two registered models with the same number trigger a warning and a lexical
   fallback. The numbers in use are ability system `PreKinematicMovers - 2` (48), combat `- 3` (47), standalone input
   `- 4` (46), Mover `PreKinematicMovers` (50, `MoverNetworkPredictionLiaison.cpp`); a new model of the family takes the
   next free number below (input.md 5.7). A
   model that must see the ability system's result of the same frame sorts after 48; one that must feed it sorts before.
   `GetName()` is what the logs, Insights and the harness print.

5. **Hooking into the other models.** A model never calls another model's component from its tick. The pattern used
   here: write a frame-stamped request into your own sync state, and execute it from the other model's own tick hook
   (`UNpAbilitySystemComponent::OnSimulationPreTick`, PATCHES.md 3); read Mover's state through
   `UGSTN_MoverSimStateTracker`, not `UMoverComponent` queries (gas-npp.md hard rules).

Cues (`NetworkPredictionCues.h`) are the way to emit transient, non-simulation events (sounds, effects) from a tick:
`NETSIMCUE_BODY()`, `NETSIMCUE_REGISTER`, a handler with `HandleCue`, `NETSIMCUESET_REGISTER`;
`NetworkPredictionExtrasLatentLoadModule.cpp` is a complete small example. Cues are unreliable by design; state
transitions belong in `FinalizeFrame`.

**Blueprint.** The framework has no Blueprint surface: `UNetworkPredictionComponent` is abstract and `MinimalAPI`, and
state types, simulation, driver functions and the model definition must be C++. What a project exposes to Blueprint is
its own component (`UGSTN_CombatSimComponent` is `Blueprintable` with `BlueprintPure` queries on the current sync state
and `EditDefaultsOnly` data) and data assets read once before the first tick. A Blueprint must never write into
simulation state directly; it asks the component, which turns it into input or a request.

## Using it standalone in another project

- Copy `Plugins/GASNPP/NetworkPrediction` into the project's `Plugins/` (the project copy shadows the engine plugin of
  the same name, as this whole fork relies on) and enable `NetworkPrediction` in the `.uproject`. It has no plugin
  dependencies and no content.
- Add a `Config/DefaultNetworkPrediction.ini` with the settings above (fixed tick, frame rate, send counts). If the
  ability system fork is used too, `FixedTickFrameRate` stays 62.5 (gas-npp.md).
- The fork's changes are additive: a model without `PrepareRepeatedInput` compiles and behaves as before, so stock NP
  models (Epic's Mover included) run on it. The reverse is not true: Mover from this project
  (`Plugins/EngineOverrides/Mover`) reads `TimeStep.IsResimulating` and implements `PrepareRepeatedInput`, and
  `AbilitySystemSimulation` relies on the rollback fixes, so take those plugins together with this one.
- The plugin assumes the game thread drives everything (no async physics NPP) and that all peers run the same binary:
  `NpInstanceFrameBufferSize` and the wire format of every state type are part of the protocol.

## Tests

No automation tests in the plugin. Any change is verified with the PIE harness `Tools/GASNPPTest/gasnpp_test.py`
(dedicated server + 2 clients, `m.net(60, 15, 2)`), scenarios `abilities`, `forced_rollback`, `server_dash`,
`combat_spike`: every `CMP` line must be `OK` (PATCHES.md, "Verifying a change to the fork"; details in
[testing.md](../../../docs/architecture/testing.md)). `UE_NP_TRACE_RECONCILE` output in
[NetworkPredictionInsights](../NetworkPredictionInsights/README.md) is the tool for a reconcile you do not understand.

## Where to read more

- [readme.txt](readme.txt): Epic's overview of the framework (same folder).
- [PATCHES.md](../PATCHES.md): every project patch to the fork, with reasons.
- [gas-npp.md](../../../docs/architecture/gas-npp.md): tick order, hard rules for simulation code, the
  `PrepareRepeatedInput` rule.
- [input.md](../../../docs/architecture/input.md) section 5 (`GSTNInputNP`, 5.7 for sort priorities) and
  [combat.md](../../../docs/architecture/combat.md) section 2 (constraints from the fork), 12.7 (network model, spike
  S-FP), 17 (M0 spike results).
- [testing.md](../../../docs/architecture/testing.md): the network harness.
- Sibling plugins: [AbilitySystemSimulation](../AbilitySystemSimulation/), [GameplayAbilities](../GameplayAbilities/README.md),
  [NetworkPredictionExtras](../NetworkPredictionExtras/README.md), [NetworkPredictionInsights](../NetworkPredictionInsights/README.md).
