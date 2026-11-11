# GSTNCore

Shared helpers of the GSTN plugins: code that two or more plugins need, kept in one place instead of duplicated
([modularity-and-tags.md](../../docs/architecture/modularity-and-tags.md) §3.6, layer L2). Pure: no GAS, no Network
Prediction, no Mover, and no edge to any other GSTN plugin, so every plugin may depend on it without a cycle.

Status: **experimental (0.1)**, `EnabledByDefault: true` (a plugin that needs it also lists it in its `.uplugin`). Created by M8 G2
([m8-minion.md](../../docs/architecture/plans/m8-minion.md), decision Q7 of 2026-11-03).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCore` | exists (Runtime, Default) | `Net/GSTN_PoseHistory.h`: `FGSTN_PoseSample`, `FGSTN_PoseHistory` (ring of the last 24 end-of-frame poses by simulation frame, re-simulation overwrites, velocity at a point from two consecutive frames). Covered by `GSTNCore.Net.*`. `GameRules/GSTN_GameRules.h` (GR-1): `UGSTN_GameRules` (data asset, one per mode or level, a piece per plugin), `UGSTN_GameRulesPiece` (base of a plugin's piece; its class defaults are that plugin's rules when no asset is active), `FGSTN_ActiveGameRules::GetPiece<T>()`, `UGSTN_GameRulesSubsystem` (server picks: URL `GameRules=<path>`, else an asset listing the level, else one listing the game mode, else defaults) and `AGSTN_GameRulesInfo` (replicates the choice, always relevant). First piece: `UGSTN_HeldRules` (GSTNHeld). |

Planned in `Net/` (M8 G2 and later, only when a second user exists): the quantizer and net item codec helpers, frame-stamp
reconstruction, the tier rate scheduler (many-enemies-replication.md §3.5).

## Dependencies

- Plugins: none.
- `GSTNCore` (`Build.cs`): Core, CoreUObject, Engine, AssetRegistry (GR-1, for the game rules). `Tools/Modules/check_modules.py` fails if it lists a GSTN module, the project module,
  GameplayAbilities, AbilitySystemSimulation, NetworkPrediction, Mover or UnrealEd.
- Used by: `GSTNPlatform` (`GSTNPlatformCore`, which keeps a redirect header `GSTN_PoseHistory.h`), `GSTNMinion` (from M8 G2), `GSTNHeld` (game rules piece, GR-1).

## Gameplay tags

None.

## Extending and reuse

A plugin adds its gameplay factors to the game rules by deriving a `UGSTN_GameRulesPiece` (`config = <Plugin>` so the defaults stay in its own ini, not in Project Settings) with a static `Get()` that returns `FGSTN_ActiveGameRules::GetPiece<T>()`. Per-kit or per-object exceptions stay in the kit or object.

Add a helper here only when a second plugin needs it; until then it stays in the plugin that owns it. Include as
`"Net/GSTN_PoseHistory.h"`. To reuse in another project, copy the plugin folder: it needs nothing else.

## Tests

`GSTNHeld.Core.GameRules`: no asset = defaults, an asset's piece wins, an asset without the piece = defaults.

`GSTNCore.Net.PoseHistory.ResimOverwritesAndReset` (no world). The original `GSTNPlatform.Core.PoseHistory.*` tests stay in the
platform plugin and must keep passing: they prove the move changed nothing.
