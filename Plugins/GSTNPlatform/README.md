# GSTNPlatform

Enemies as solid platforms: kinematic, unattached colliders authored in an enemy's frame and posed at one defined point per
simulation step, so a player's Mover can stand on them and the wall modes can ride them like any moving base.

Status: **experimental (0.1, skeleton)**, `EnabledByDefault: false`. This is the minimum the platform plan needs for **fighters**
(many-enemies-replication.md §4.2–4.5: spec, component, collision channel, pose point, pose history), built as the first commit of
checkpoint T3 of [traversal-surfaces-plan.md](../../docs/architecture/plans/traversal-surfaces-plan.md). No platform-minion proxy,
no minion seams (combat-m8-minions.md §4d.5 stays for `GSTNPlatformMinion`). Design:
[many-enemies-replication.md](../../docs/architecture/many-enemies-replication.md) §4,
[traversal-surfaces.md](../../docs/architecture/traversal-surfaces.md) §4. Doc deltas:
[traversal-doc-deltas.md](../../docs/notes/traversal-doc-deltas.md).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNPlatformCore` | exists (Runtime) | `FGSTN_PlatformSpec`, `FGSTN_PlatformShape`, `FGSTN_PlatformPoseContext`, `FGSTN_PoseHistory`, the `Platform.*` native tags. No world, no Mover. Covered by `GSTNPlatform.Core.*`. |
| `GSTNPlatformRuntime` | exists (Runtime) | `UGSTN_PlatformSettings`, `UGSTN_PlatformDefinition`, `UGSTN_EnemyPlatformComponent`, the collider classes, `UGSTN_PlatformSubsystem` (the pose point). Covered by `GSTNPlatform.Runtime.*`. |

## Dependencies

- Plugins: `GSTNSurface`, `Mover`. No GAS, no direct Network Prediction code (Mover brings Network Prediction with it).
- `GSTNPlatformCore`: Core, CoreUObject, Engine, GameplayTags, GSTNSurfaceCore.
- `GSTNPlatformRuntime`: the above plus DeveloperSettings, PhysicsCore, Mover, GSTNPlatformCore (private NetCore).
- Never `GASP_Giustino2`, `GSTNCombat*`, `GSTNParts`, `GSTNMinion`.

## Collision channel

`Config/Engine.ini` declares the object channel `GSTN_EnemyPlatform` (slot `GameTraceChannel4`, default response Block). The engine
merges a plugin's `Config/Engine.ini` into the Engine branch when the plugin is mounted (`DefaultEngine.ini` in a plugin is **not**
merged). The code finds the channel by name, so a project that needs the slot moves the line to its own `DefaultEngine.ini`.

## Gameplay tags

| Tags | Source | Declared in |
|---|---|---|
| `Platform`, `Platform.State.{Active,Braced,Breaking,Inactive}`, `Platform.Event.{Mounted,Dropped}` | native | `GSTNPlatformCore/Public/GSTN_PlatformTags.h`, namespace `GSTNPlatformTags` |

No tag ini, so the module registers no tag search path.

## Tests

`Automation RunTests GSTNPlatform`.
