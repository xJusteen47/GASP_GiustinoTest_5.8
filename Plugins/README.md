# Plugins

Every plugin in this folder has a `README.md` next to its `.uplugin`. Each README covers the same points: purpose and status,
modules, dependencies, the gameplay tags the plugin owns, how to extend it in C++ and Blueprint, how to use it in another
project, and where to read more in [docs/architecture](../docs/architecture/).

## Project plugins (GSTN)

The game's own systems. Each feature is split into a pure **Core** plugin (no GAS, Network Prediction or Mover) and adapter
plugins that add GAS or Network Prediction, so the Core can be reused in a project without the GAS-NPP fork.

| Plugin | Layer | What it is | Needs the GAS-NPP fork |
|---|---|---|---|
| [GSTNInput](GSTNInput/README.md) | Core | Input commands, buffer, priority, locks, contexts and native input tags. | No |
| [GSTNInputNP](GSTNInputNP/README.md) | NP adapter | The input simulation embeddable in an NP model, ability bindings, the Mover sim-state tracker. | Yes |
| [GSTNCombat](GSTNCombat/README.md) | Core | Default combat command set, combat tags and settings, timelines, loadout data and rules, sync fragment format. | No |
| [GSTNCombatGAS](GSTNCombatGAS/README.md) | GAS | Combat ability tags, attribute sets, kit stats, meter and weapon effects, kit/weapon/super assets, the loadout component, loadout presets (save game) and the skin component. | No (stock GAS works) |
| [GSTNCombatNP](GSTNCombatNP/README.md) | NP adapter | The combat NP model (`UGSTN_CombatSimComponent`), sync fragments and tick extensions. | Yes |

Dependency rule: Core modules depend on no GAS, NP or Mover module; an adapter depends on its own Core and may depend on
another plugin's adapter of the same layer through public interfaces only.

## GAS-NPP fork

[GASNPP](GASNPP/README.md): Epic's NetworkPrediction (+ Extras, Insights), the GameplayAbilities fork and
AbilitySystemSimulation, ported to UE 5.8. Every project change is listed in [GASNPP/PATCHES.md](GASNPP/PATCHES.md).

## Engine overrides

[EngineOverrides](EngineOverrides/README.md): copies of engine plugins loaded instead of the engine's (Mover, ChaosMover,
MoverExamples, SmartObjects, GameplayInteractions, TargetingSystem, editor toolsets). Mover carries project edits marked
"GAS-NPP fork"; the others are unmodified as far as git shows.

## Editor tooling

[UE_MCP_Bridge](UE_MCP_Bridge/README.md): third-party editor-only WebSocket bridge for the UE-MCP server. Not game code.
