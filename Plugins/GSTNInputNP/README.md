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
the standalone `UGSTN_InputSimComponent` (model `GSTN_Input`). How to embed the pieces in a host model: the "Embedding the input
simulation in a model" section of the GSTNInput README.

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

## Tests

Automation tests `GSTNInput.NP.*` (`Source/GSTNInputNP/Private/Tests/`), run with the usual `Automation RunTests GSTNInput+GSTNCombat`
(filter by name prefix, so the plugin move did not change it). See [testing.md](../../docs/architecture/testing.md).
