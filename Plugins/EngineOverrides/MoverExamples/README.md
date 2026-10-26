# Mover Examples

Mover Examples (`MoverExamples.uplugin` 0.1, experimental, by Epic Games) holds non-essential sample code for Mover: a sample character, game mode and game state, movement modes that follow a path or spline, and ability-style inputs including ziplining. The upstream plugin also ships maps and Blueprint pawns (see Mover's README, "Examples"). This folder is a copy of Epic's 5.8 plugin that overrides the engine version. It is not meant for shipping.

## What this project changed vs. the stock plugin

Nothing, as far as git shows: it was imported in `53c8716`, has no "GAS-NPP fork" marker, and has no later commits. It is kept because it is part of the Mover override set (it is enabled in `GASP_Giustino2.uproject`) and is available as reference. The folder has no `Content/`: no maps or pawn Blueprints are in the repository, so the maps named in Mover's README are not available here. The project gameplay does not depend on it (`GASP_Giustino2.Build.cs` does not reference `MoverExamples`).

## Module

| Module | Type | Loading phase | Contents |
|---|---|---|---|
| `MoverExamples` | Runtime | Default | `Source/MoverExamples/Public/`: `MoverExamplesCharacter.h`, `MoverExamplesGameMode.h`, `MoverExamplesGameState.h`, `CharacterVariants/` (`AbilityInputs.h`, `Ziplining/`), `MovementBases/` (`FollowPathMode.h`, `FollowSplineMode.h`). |

## Dependencies

- Plugins (all enabled): `Mover`, `ChaosMover`, `EnhancedInput`, `CableComponent`.
- Modules (`MoverExamples.Build.cs`): `Core`, `CoreUObject`, `Engine`, `InputCore`, `Mover`, `EnhancedInput`, `CableComponent`.
- Used by: nothing in the project code.

## Gameplay tags

Declares no gameplay tags.

## How to extend

It is example code: copy the classes you need into your own module rather than editing them. `AMoverExamplesCharacter` shows a C++ input producer (`OnProduceInput`), `FollowPathMode` and `FollowSplineMode` show custom `UBaseMovementMode` subclasses. Blueprint extension works through the sample pawns, which are not in this repository.

## Using it standalone in another project

Enable `Mover` and `MoverExamples` (and `ChaosMover`, which it depends on) from the plugin folder; `EnhancedInput` and `CableComponent` are engine plugins. For the upstream maps, project settings and walkthrough see [Mover README](../Mover/README.md), "Getting Started". The maps and Blueprints themselves come from Epic's engine copy of the plugin, not from this repository.

## Where to read more

- [Mover README](../Mover/README.md), "Examples" and "Project notes".
- [ChaosMover README](../ChaosMover/README.md).
