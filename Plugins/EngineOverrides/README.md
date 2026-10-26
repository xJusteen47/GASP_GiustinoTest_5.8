# Engine overrides

This folder holds copies of Epic engine plugins that the project loads from `Plugins/` instead of from the engine install. Project plugins under `Plugins/` take precedence over engine plugins with the same name, so a copy here replaces the engine one for this project only. `CLAUDE.md` marks them as engine-level code that should not be read in bulk.

| Folder | Plugin | Notes | README |
|---|---|---|---|
| `Mover` | Mover | Basis of the wall movement work; see its README. | [Mover/README.md](Mover/README.md) |
| `ChaosMover` | ChaosMover | Chaos-physics Mover backend. | [ChaosMover/README.md](ChaosMover/README.md) |
| `MoverExamples` | MoverExamples | Epic's Mover sample content/code. | [MoverExamples/README.md](MoverExamples/README.md) |
| `SmartObjects` | SmartObjects | Unmodified as far as git shows; needs TargetingSystem. | [SmartObjects/README.md](SmartObjects/README.md) |
| `GameplayInteractions` | GameplayInteractions | Unmodified; StateTree interactions on SmartObjects. | [GameplayInteractions/README.md](GameplayInteractions/README.md) |
| `GameplayTargetingSystem` | TargetingSystem | Unmodified; folder name differs from plugin name. | [GameplayTargetingSystem/README.md](GameplayTargetingSystem/README.md) |
| `GASToolsets` | GASToolsets | Unmodified; editor-only AI toolsets for GAS. | [GASToolsets/README.md](GASToolsets/README.md) |
| `AllToolsets` | AllToolsets | Uplugin-only aggregator of toolset plugins. | [AllToolsets/README.md](AllToolsets/README.md) |

The GAS-NPP forks (GameplayAbilities, NetworkPrediction, ...) are not here; they live in `Plugins/GASNPP/` ([gas-npp.md](../../docs/architecture/gas-npp.md)).

## Why the folder exists

The project was imported (commit `53c8716`) with these engine plugins vendored so they can be changed or pinned for the 5.8 setup (Mover is the one the project builds on; see [wall-movement.md](../../docs/architecture/wall-movement.md)). Git history does not record a per-plugin reason for the others; the dependency chain suggests SmartObjects, GameplayInteractions and TargetingSystem travel together, and the toolsets belong to the `unreal-mcp` editor tooling. Where a README says "unmodified as far as git shows", the copy has no project edits and could in principle be removed in favor of the engine's (see step 5 below).

## Re-syncing with a new engine version

1. Before replacing a plugin, read its README for the project changes and list them (for Mover, diff against the stock engine copy of the same version the copy came from).
2. Copy the new engine version over the folder, re-apply the documented project changes, and keep the folder name (the uplugin name must not change).
3. Rebuild with a full build (header and reflection changes), check the `.uplugin` version and dependency lists against the README, and update the README.
4. Re-run the network harness (`Tools/GASNPPTest/gasnpp_test.py`, see [testing.md](../../docs/architecture/testing.md)) if Mover or ChaosMover changed.
5. For unmodified copies, consider removing the folder and using the engine's version instead, but only after a full build and a harness run confirm nothing depended on the copy; then drop its README and table row. This is the owner's call, not a cleanup to do in passing.
