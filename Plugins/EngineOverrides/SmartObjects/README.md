# SmartObjects (engine override)

Copy of Epic's **SmartObjects** engine plugin (0.1, "Support for ambient life populating the game world"), kept in `Plugins/EngineOverrides/` so the project builds against this copy instead of the stock one. It is an **unmodified copy as far as git shows**: the only commit touching it is `53c8716` (initial import), and the sources contain no project markers (`GSTN`, "GAS-NPP fork", ...). No project code in `Source/` references it.

## Purpose

SmartObjects lets world actors advertise "slots" that agents (NPCs, players) can find, claim and use, driven by a behavior definition and gated by world conditions. This project enables it in `GASP_Giustino2.uproject` only because the sample content and `GameplayInteractions` need it. It is not used by the wall-movement / GAS-NPP work. Not enabled by default (`EnabledByDefault: false`); the .uplugin is not flagged beta or experimental.

## Modules

| Module | Type / phase | Contents |
|---|---|---|
| `SmartObjectsModule` | Runtime, PreDefault | `USmartObjectSubsystem`, `USmartObjectDefinition` (data asset), `USmartObjectComponent`, `USmartObjectUserComponent`, `USmartObjectBehaviorDefinition` (base for behaviors), `ASmartObjectPersistentCollection`, `USmartObjectHashGrid` / octree space partitions, annotations (`Annotations/`), world conditions (`WorldConditions/`), EQS generator and Blackboard key, gameplay debugger category. Headers in `Source/SmartObjectsModule/Public/`. |
| `SmartObjectsTestSuite` | UncookedOnly, PreDefault | Automation tests (`Source/SmartObjectsTestSuite/`). |
| `SmartObjectsEditorModule` | Editor, PostEngineInit | Definition editor, component visualizers, asset definition. |

## Dependencies

- Plugins (all required): `GameplayAbilities`, `TargetingSystem`, `WorldConditions`, `PropertyAccessEditor`, `PropertyBindingUtils`.
- Modules (`SmartObjectsModule.Build.cs`, public): `AIModule`, `GameplayTags`, `GameplayAbilities`, `WorldConditions`, `NavigationSystem`, `TargetingSystem`, `PropertyBindingUtils`, `MassCore`, `MassEntity`, `DeveloperSettings`, `RHI`.
- Used by: `GameplayInteractions` (this folder) and the project, which enables it in `GASP_Giustino2.uproject`.
- Note: it depends on the project's `GameplayAbilities` and `TargetingSystem` copies, so those must resolve (the first is the GAS-NPP fork in `Plugins/GASNPP/`, see [gas-npp.md](../../../docs/architecture/gas-npp.md)).

## Gameplay tags

Declares no native gameplay tags. It uses tag queries in its world conditions (`SmartObjectWorldConditionSlotTagQuery.h`, `WorldCondition_SmartObjectActorTagQuery.h`) and tags set on definitions and slots.

## Config

`Config/DefaultSmartObjects.ini` sets `SpacePartitionClassName=/Script/SmartObjectsModule.SmartObjectHashGrid` and holds `[CoreRedirects]` for renamed functions, enums and structs.

## How to extend

- C++: subclass `USmartObjectBehaviorDefinition` (see `UGameplayInteractionSmartObjectBehaviorDefinition` in `GameplayInteractions`), add annotations (`SmartObjectAnnotation.h`), add world conditions (`SmartObjectWorldConditionBase.h`), or a custom space partition.
- Blueprint: `USmartObjectDefinition` is a data asset configured in the editor; `USmartObjectComponent` and `USmartObjectUserComponent` are placeable/usable from Blueprint, and `SmartObjectBlueprintFunctionLibrary.h` exposes queries. Subsystem logic and behavior definitions stay C++ unless a Blueprintable class is marked as such upstream.

## Using it standalone

Copy it together with `TargetingSystem` (`GameplayTargetingSystem/`), and make sure `GameplayAbilities`, `WorldConditions`, `PropertyAccessEditor`, `PropertyBindingUtils`, and the Mass modules (`MassCore`, `MassEntity`) are available. Enable it in the .uproject. No blocker from this project; it works with the stock `GameplayAbilities` too.

## Where to read more

No project architecture doc covers it. Epic's engine documentation applies as it is an unmodified copy. Index of overrides: [../README.md](../README.md).
