# TargetingSystem (engine override, folder `GameplayTargetingSystem`)

Copy of Epic's **Targeting System** engine plugin (`TargetingSystem.uplugin`, 1.0, `IsBetaVersion: true`, "Generic targeting system for use with gameplay abilities, aim assist, etc"). The folder is named `GameplayTargetingSystem` but the plugin name is `TargetingSystem`. It is an **unmodified copy as far as git shows** (only commit `53c8716`, no project markers). No project code references it directly; it is required by the `SmartObjects` override (`SmartObjects.uplugin` and `SmartObjectsModule.Build.cs`), which is why it is present.

## Purpose

An asynchronous, task-pipeline targeting system: a `UTargetingPreset` lists selection, filter and sort tasks that the `UTargetingSubsystem` runs to produce target results.

## Modules

| Module | Type / phase | Contents |
|---|---|---|
| `TargetingSystem` | Runtime, Default | `Source/GameplayTargetingSystem/Public/`: `TargetingSystem/TargetingSubsystem.h`, `TargetingSystem/TargetingPreset.h`, `Tasks/` (`UTargetingTask` base, `UTargetingSelectionTask_Trace`, `_AOE`, `USimpleTargetingSelectionTask`, `UTargetingFilterTask_ActorClass`, `_SortByDistance`, `USimpleTargetingFilterTask`, `USimpleTargetingSortTask`), `AbilityTasks/AbilityTask_PerformTargeting.h`, `Async/AsyncAction_PerformTargeting.h`, `Types/`. |

## Dependencies

- Plugin: `GameplayAbilities` (required), which here is the GAS-NPP fork (see [gas-npp.md](../../../docs/architecture/gas-npp.md)).
- Modules: public `Core`, `CoreUObject`, `Engine`, `GameplayTasks`, `GameplayAbilities`, `GameplayTags`; private `DeveloperSettings`.
- Used by: `SmartObjects` (this folder).

## Gameplay tags

Declares no native gameplay tags and ships no `Config/Tags`.

## How to extend

- C++: subclass `UTargetingTask` (or the `Simple*` / `*_BasicFilterTemplate` / `UTargetingSortTask_Base` helpers) to add selection, filter or sort steps.
- Blueprint: the `AbilityTask_PerformTargeting` and `AsyncAction_PerformTargeting` nodes run a preset from Blueprint; presets are data assets. `UTargetingTask` is `EditInlineNew` and Abstract.

## Using it standalone

Copy it and enable it alongside `GameplayAbilities`. Works with stock GAS.

## Where to read more

Not covered by project docs. See [../README.md](../README.md).
