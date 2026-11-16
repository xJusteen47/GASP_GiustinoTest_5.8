# Chaos Mover

Chaos Mover (`ChaosMover.uplugin` 1.0, experimental, by Epic Games) lets Mover drive a physics-simulated character: a Chaos character ground constraint performs the movement that Mover's modes propose, and Chaos networked physics handles prediction and rollback instead of Network Prediction. This folder is a copy of Epic's 5.8 plugin that overrides the engine version.

## What this project changed vs. the stock plugin

Nothing, as far as git shows. `Plugins/EngineOverrides/ChaosMover` was imported in `53c8716` and has had no commit since, and it has no "GAS-NPP fork" marker. It is kept in the project because `Mover` and `MoverExamples` (`ChaosMannyPawnExtended`, map `L_PhysicallyBasedCharacter`) depend on it and all three are overridden together; the project's own gameplay does not use physics-driven movement (the project movement runs on Network Prediction, see [gas-npp.md](../../../docs/architecture/gas-npp.md)). The import commit has no stock copy to diff against, so an unmarked edit cannot be ruled out.

## Module

| Module | Type | Loading phase | Contents |
|---|---|---|---|
| `ChaosMover` | Runtime | Default | `Source/ChaosMover/Public/ChaosMover/`: `ChaosMoverSimulation.h`, `ChaosMoverStateMachine.h`, `ChaosMovementMode.h`, `Backends/` (backend liaison, async callback, subsystem, network data), `Character/` (`ChaosCharacterMoverComponent.h`, `ChaosCharacterInputs.h`, `Modes/` walking, falling, flying, swimming, smooth walking; `Executors/`, `Effects/`, `Settings/`, `Transitions/`, `Modifiers/`), `PathedMovement/`, `ChaosMoverBlueprintLibrary.h`, `ChaosMoverConsoleVariables.h`. |

## Dependencies

- Plugin: `Mover` (required). Needs the engine's Chaos physics.
- Modules (`ChaosMover.Build.cs`): public `Core`, `GameplayTags`, `InputCore`, `NetCore`, `Mover`, `Water`; private `CoreUObject`, `DeveloperSettings`, `Engine`, `MotionWarping`.
- Used by: `MoverExamples` (plugin and module). The project module does not reference it. It is enabled in `GASP_Giustino2.uproject`.

## Gameplay tags

Declares no gameplay tags (no `UE_DEFINE_GAMEPLAY_TAG*`, no `Config/Tags`).

## Config

`Config/DefaultChaosMover.ini` has a core redirect (`ChaosCharacterMoverComponent.OnLanded` to `OnLandedDelegate`) and enables `PhysicsPrediction` (`bEnablePhysicsPrediction=True`) in `/Script/Engine.PhysicsSettings`.

## How to extend

Same pattern as Mover, with Chaos-side classes: subclass `UChaosMovementMode` or the character modes under `Character/Modes/`, add executors (`Character/Executors/`), transitions (`UChaosMovementModeTransition`), layered move sources (`ChaosLayeredMoveSource.h`) or modifiers. Modes and executors are C++ classes; the Blueprint surface is `ChaosMoverBlueprintLibrary.h` and the Blueprint-exposed component and settings properties. See Mover's README for the shared concepts (modes, layered moves, input and sync state).

## Using it standalone in another project

Copy `ChaosMover` next to `Mover` and enable both; set the physics settings from `Config/DefaultChaosMover.ini` and follow the notes in Mover's README ("Physics-driven Character Example"). It does not use Network Prediction and does not depend on the GAS-NPP fork itself, but it builds on this project's patched Mover (see Mover's README for what that changes).

## Where to read more

- [Mover README](../Mover/README.md), section "Physics-driven Character Example" and "Project notes".
- [PATCHES.md](../../GASNPP/PATCHES.md) section 5: why the engine overrides are in the project.
