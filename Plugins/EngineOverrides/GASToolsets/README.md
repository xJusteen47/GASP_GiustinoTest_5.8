# GASToolsets (engine override)

Copy of Epic's **GASToolsets** engine plugin (1.0, "Toolsets for the Gameplay Ability System"). Editor-only, `IsExperimentalVersion: true`, `NoRedist: true`. It is an **unmodified copy as far as git shows** (only commit `53c8716`, no project markers). It is not enabled directly in `GASP_Giustino2.uproject`; it is pulled in by the `AllToolsets` aggregator in this folder.

## Purpose

Exposes GAS to AI agents through the `ToolsetRegistry` (the mechanism behind the `unreal-mcp` server used in this project): functions marked `meta = (AICallable)` that inspect and edit ability systems, attribute sets and gameplay cues.

## Modules

| Module | Type / phase | Contents |
|---|---|---|
| `GASToolsets` | Editor, PostEngineInit (target allow-list: Editor) | `Source/GASToolsets/Private/`: `UAbilitySystemInspectorToolset` (category `AbilitySystem`), `UAttributeSetToolset` (`AttributeSets`), `UGameplayCueToolset` (`GameplayCues`), all `UToolsetDefinition` subclasses (`BlueprintType, Hidden`); `Tests/` holds automation tests for the first two. Public header only `GASToolsets.h`. |

## Dependencies

- Plugins: `GameplayAbilities`, `ToolsetRegistry` (both required).
- Modules: `Core` (public); private `AssetRegistry`, `CoreUObject`, `Engine`, `GameplayAbilities`, `GameplayTags`, `GameplayTagsEditor`, `Kismet`, `KismetCompiler`, `ToolsetRegistry`, `UnrealEd`.
- Used by: `AllToolsets`.

## Gameplay tags

Declares no gameplay tags.

## How to extend

C++ only: add a `UToolsetDefinition` subclass with `UFUNCTION(meta = (AICallable))` static functions. The `unreal-engine-skills-for-claude-code:create-toolset` skill describes the conventions. Not Blueprint-extensible.

## Using it standalone

Needs the `ToolsetRegistry` engine plugin (experimental in 5.8) and `GameplayAbilities`; editor builds only. With the GAS-NPP fork of `GameplayAbilities` the toolsets only see the fork's API; no project-specific changes were found.

## Where to read more

[../AllToolsets/README.md](../AllToolsets/README.md), [../README.md](../README.md).
