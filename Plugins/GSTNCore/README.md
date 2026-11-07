# GSTNCore

Shared helpers of the GSTN plugins: code that two or more plugins need, kept in one place instead of duplicated
([modularity-and-tags.md](../../docs/architecture/modularity-and-tags.md) §3.6, layer L2). Pure: no world, no GAS, no Network
Prediction, no Mover, and no edge to any other GSTN plugin, so every plugin may depend on it without a cycle.

Status: **experimental (0.1)**, `EnabledByDefault: true` (a plugin that needs it also lists it in its `.uplugin`). Created by M8 G2
([m8-minion.md](../../docs/architecture/plans/m8-minion.md), decision Q7 of 2026-11-03).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCore` | exists (Runtime, Default) | `Net/GSTN_PoseHistory.h`: `FGSTN_PoseSample`, `FGSTN_PoseHistory` (ring of the last 24 end-of-frame poses by simulation frame, re-simulation overwrites, velocity at a point from two consecutive frames). Covered by `GSTNCore.Net.*`. |

Planned in `Net/` (M8 G2 and later, only when a second user exists): the quantizer and net item codec helpers, frame-stamp
reconstruction, the tier rate scheduler (many-enemies-replication.md §3.5).

## Dependencies

- Plugins: none.
- `GSTNCore` (`Build.cs`): Core only. `Tools/Modules/check_modules.py` fails if it lists a GSTN module, the project module,
  GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover or UnrealEd.
- Used by: `GSTNPlatform` (`GSTNPlatformCore`, which keeps a redirect header `GSTN_PoseHistory.h`), `GSTNMinion` (from M8 G2).

## Gameplay tags

None.

## Extending and reuse

Add a helper here only when a second plugin needs it; until then it stays in the plugin that owns it. Include as
`"Net/GSTN_PoseHistory.h"`. To reuse in another project, copy the plugin folder: it needs nothing else.

## Tests

`GSTNCore.Net.PoseHistory.ResimOverwritesAndReset` (no world). The original `GSTNPlatform.Core.PoseHistory.*` tests stay in the
platform plugin and must keep passing: they prove the move changed nothing.
