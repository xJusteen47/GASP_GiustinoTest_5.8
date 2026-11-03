# GameplayInteractions (engine override)

Copy of Epic's **GameplayInteractions** engine plugin (1.0, "Player and NPC interactions", flagged `IsExperimentalVersion`). It is an **unmodified copy as far as git shows**: the only commit touching it is `53c8716` (initial import) and there are no project markers in the sources. No project code references it. It is enabled in `GASP_Giustino2.uproject`.

## Purpose

Runs interactions on top of SmartObjects using StateTree: a behavior definition for a smart object slot starts a StateTree that moves and animates the user. Likely kept here together with `SmartObjects` so both build from the same engine-version copy.

## Modules

| Module | Type / phase | Contents |
|---|---|---|
| `GameplayInteractionsModule` | Runtime, PreDefault | `Source/GameplayInteractionsModule/Public/`: `UGameplayInteractionSmartObjectBehaviorDefinition` (a `USmartObjectBehaviorDefinition`, BlueprintType), `GameplayInteractionContext.h`, `GameplayInteractionStateTreeSchema.h`, `GameplayInteractionsTypes.h`, `StateTree/StateTreeTask_FindSlotEntranceLocation.h`. |

## Dependencies

- Plugins (all required): `StateTree`, `SmartObjects`, `ContextualAnimation`, `NavCorridor`, `GameplayStateTree`.
- Modules (public): `AIModule`, `ContextualAnimation`, `GameplayStateTreeModule`, `GameplayTags`, `GameplayTasks`, `NavCorridor`, `NavigationSystem`, `SmartObjectsModule`, `StateTreeModule` (plus Core/CoreUObject/Engine).
- Depends on the `SmartObjects` override in this folder.

## Gameplay tags

Declares no gameplay tags.

## Config

`Config/DefaultGameplayInteractions.ini` (engine defaults).

## How to extend

- C++: add StateTree tasks/conditions for the `GameplayInteractionStateTreeSchema`, following `StateTreeTask_FindSlotEntranceLocation.h`.
- Blueprint/data: author a StateTree asset with that schema and reference it from a `UGameplayInteractionSmartObjectBehaviorDefinition` inside a `USmartObjectDefinition`. New task types are C++.

## Using it standalone

Copy it with `SmartObjects`, and enable the plugins listed above (StateTree, ContextualAnimation, NavCorridor, GameplayStateTree are engine plugins). No project assumptions.

## Where to read more

Not covered by project docs; Epic's docs apply. See [../README.md](../README.md).
