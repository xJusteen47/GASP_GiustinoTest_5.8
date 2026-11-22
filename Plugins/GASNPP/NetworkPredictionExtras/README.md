# NetworkPredictionExtras (GAS-NPP fork)

Epic's sample and support plugin for [NetworkPrediction](../NetworkPrediction/README.md): working example simulations
(a mock accumulator, flying and character movement, mock abilities, root motion, parametric movers), the pawns and game
mode that drive them, and a module that shows a model registered by a late-loaded module. Its own description says it:
"Not intended to be used directly in a shipping product." In this project nothing runs on it; it is here because
`AbilitySystemSimulation.uplugin` lists it as a dependency and because its sources are the closest thing to a tutorial
for writing a Network Prediction model (the project models in `GSTNInputNP` and `GSTNCombatNP` are the real ones).

Status: **beta** (`NetworkPredictionExtras.uplugin` version `0.1`, `IsBetaVersion: true`, by Epic Games). Copied with
the fork and ported to UE 5.8 (`53c8716`), one project fix since.

## Modules

| Module | Type | Loading phase | Contents |
|---|---|---|---|
| `NetworkPredictionExtras` | Runtime | Default | `Source/NetworkPredictionExtras/Public/`: `MockNetworkSimulation.h` (the minimal model: `FMockNetworkSimulation` + `UMockNetworkSimulationComponent`, console commands `mns.*`), `BaseMovementComponent.h` / `BaseMovementSimulation.h` (abstract movement driver and simulation with collision helpers), `FlyingMovementComponent.h` / `FlyingMovementSimulation.h`, `CharacterMotionComponent.h` / `CharacterMotionSimulation.h` (input/sync/aux structs and a floor-walking simulation), `MockAbilitySimulation.h` and `MockCharacterAbilitySimulation.h` (sprint/dash/blink style "abilities" layered on the movement sims, with cues), `MockRootMotionComponent.h` / `MockRootMotionSimulation.h` / `MockRootMotionSourceObject.h` / `MockRootMotionSourceStore.h` (predicted root motion sources, `UMockRootMotionSourceClassMap` built at engine init and before PIE in `NetworkPredictionExtrasModule.cpp`), `ParametricMovement.h` (`UParametricMovementComponent`, a server-driven mover), `Springs.h`, and the sample actors `ANetworkPredictionExtrasFlyingPawn`, `ANetworkPredictionExtrasCharacter` (+ `_MockAbility` variants), `ANetworkPredictionExtrasGameMode`, `ANetworkPredictionExtrasGameState`. |
| `NetworkPredictionExtrasLatentLoad` | Runtime | None | `Source/NetworkPredictionExtrasLatentLoad/Private/NetworkPredictionExtrasLatentLoadModule.cpp`: a complete, do-nothing model (`FLatentNetworkModelDef`, input/sync/aux structs, simulation, driver, `NP_MODEL_REGISTER`) and two cues with a handler (`NETSIMCUE_REGISTER`, `NETSIMCUESET_REGISTER`). Loading phase `None` means it is never loaded automatically; it exists to prove registration works from a module loaded after startup. |

`CanContainContent` is true, but this copy has no `Content/` folder: the test maps and Blueprints Epic ships with the
plugin were not brought over.

## Dependencies

- Plugin dependencies (`NetworkPredictionExtras.uplugin`): `NetworkPrediction` (the fork), not optional.
- Module dependencies (`NetworkPredictionExtras.Build.cs`): public `NetworkPrediction`, `Core`, `CoreUObject`,
  `Engine`, `RenderCore`, `InputCore`, `PhysicsCore`, `Chaos`; private `NetCore`, `TraceLog`, and `UnrealEd` in editor
  targets. `NetworkPredictionExtrasLatentLoad.Build.cs` is the same list without `NetCore`.
- Depends on it: only `AbilitySystemSimulation.uplugin`, as a plugin dependency; the `AbilitySystemSimulation` module
  does not link either module and no project source includes an Extras header. The plugin is not listed in the project
  `.uproject`; it is enabled through that dependency.

## Gameplay tags

Declares no gameplay tags.

## Configuration

`Config/DefaultNetworkPredictionExtras.ini` adds a "Debug" DevHUD page to `UNetworkPredictionSettingsObject`
(`np2.debug`, `np2.Debug.Tolerance`, `np2.dump`, spawn/destroy dummy physics clients) and a class redirect from
`NetworkPredictionExtras.NetworkPhysicsComponent` to `NetworkPrediction.NetworkPhysicsComponent`. Both are stock
leftovers: the dummy-client entries reference `/NetworkPredictionExtras/Physics/...` content that is not in this copy,
and the redirect target does not exist in the forked `NetworkPrediction` (physics NPP was removed upstream). Neither
does harm.

## What this project changed vs. the stock engine plugin

One line, from the 5.8 port audit (`951c495`, PATCHES.md section 1): `Private/MockRootMotionComponent.cpp` creates the
root motion source CDO with `Source.GetDefaultObject()` the way the 5.7 fork did, because the 5.8 code path
(`GetDefaultObject(false)`) dropped parameterless sources. Nothing else differs from the port as far as git shows
(`git log --oneline -- Plugins/GASNPP/NetworkPredictionExtras`: `53c8716` import, `951c495` the fix). The fix is listed
in [PATCHES.md](../PATCHES.md), which is the place to add any further change.

## How to extend

This plugin is reference material. Read it, do not build on it:

- To write a model, follow the step list in the [NetworkPrediction README](../NetworkPrediction/README.md#how-to-extend-writing-a-new-simulation-model)
  and keep `MockNetworkSimulation.h` open next to `GSTN_CombatSimComponent.cpp`: the mock shows the bare hookups
  (state structs with `NetSerialize` / `ShouldReconcile` / `Interpolate` / `ToString`, a `SimulationTick`, a
  `UNetworkPredictionComponent` driver, the model def and `NP_MODEL_REGISTER`), the combat component shows the same
  shape with a real input command and the project's rules applied.
- `FBaseMovementSimulation` / `UBaseMovementComponent` are the pattern for a movement model that owns a primitive
  component and does its own sweeps; in this project movement is Mover's job (`Plugins/EngineOverrides/Mover`), so a
  new movement feature is a Mover mode, not an Extras subclass.
- `MockAbilitySimulation.h` and `MockCharacterAbilitySimulation.h` show cues invoked from a tick (`Output.CueDispatch`)
  and handled on the driver; `NetworkPredictionExtrasLatentLoadModule.cpp` is the shortest complete cue example.
- `UMockRootMotionSourceClassMap` (`MockRootMotionSourceObject.h`) is an example of mapping `UClass`es to small ids so
  a sync state can reference an asset-driven object deterministically.

In Blueprint: the sample components are `BlueprintType` and `BlueprintSpawnableComponent` (`UMockNetworkSimulationComponent`,
`UFlyingMovementComponent`, `UCharacterMotionComponent`, `UMockFlyingAbilityComponent`, `UMockCharacterAbilityComponent`,
`UMockRootMotionComponent`, `UParametricMovementComponent`) and the root motion sources are `Blueprintable`, so a test
pawn can be assembled in a Blueprint. The simulations themselves are C++ and must stay so.

## Using it standalone in another project

Copy `NetworkPredictionExtras` together with the forked `NetworkPrediction` (its only dependency) into `Plugins/` and
enable both; a project that also takes `AbilitySystemSimulation` needs this plugin enabled, because that `.uplugin`
requires it. No ini is needed beyond `NetworkPrediction`'s own settings. Without `Content/` you have the code only: the
`mns.Spawn` console command adds `UMockNetworkSimulationComponent` to every pawn on the server for a quick check (the
header documents the other `mns.*` commands), and the sample pawns can be placed from C++ or a Blueprint child. If you
do not need `AbilitySystemSimulation`, you do not need this plugin.

## Where to read more

- [NetworkPrediction README](../NetworkPrediction/README.md) and Epic's [readme.txt](../NetworkPrediction/readme.txt)
  (its "Getting Started" refers to this plugin under its old name, NetworkPredictionExamples).
- [PATCHES.md](../PATCHES.md): the one project change.
- [gas-npp.md](../../../docs/architecture/gas-npp.md): the rules a real model in this project follows.
- [GSTNInputNP README](../../GSTNInputNP/README.md), [GSTNCombatNP README](../../GSTNCombatNP/README.md): the project's
  models.
