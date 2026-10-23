# GSTNSurface

The traversal surface data contract for Unreal Engine 5.8: which mechanics a collider offers (wall slide, wall climb,
launch, stand), its impact class (`Surface.Type.*`) and its launch parameters, and one stateless way to resolve all of it
from a hit. Level objects (walls, trampolines) and enemy body parts carry the same data, so the wall movement, the combat
impacts and the platforms read surfaces without knowing what they hit.

Status: **experimental (0.1)**, `EnabledByDefault: false`. This is checkpoint T1 of
[traversal-surfaces-plan.md](../../docs/architecture/plans/traversal-surfaces-plan.md); the design is
[traversal-surfaces.md](../../docs/architecture/traversal-surfaces.md) (§2 data model, §3 plugin placement). The consumers
(wall-surface filter T5, platform shapes T3, launch T4, H11 impacts) are not built yet. Enabling the plugin changes nothing
until a designer marks a surface: unmarked geometry resolves to the settings' default, which offers everything the wall and
walking modes did before.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNSurfaceCore` | exists (Runtime, Default) | Types, tags, provider interface, behaviour component, settings, the baked lookup table and the resolution utils. Pure data and stateless functions: no GAS, Network Prediction or Mover, so it needs no fork. Covered by `GSTNSurface.Core.*`. |

The module class (`Private/GSTNSurfaceCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager (the
engine reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins), bakes the surface table
after engine init so no lookup inside the simulation loads assets, and rebuilds the type indices when the tag tree changes.

## Dependencies

- Plugins: none.
- `GSTNSurfaceCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, PhysicsCore, DeveloperSettings; private
  Projects (to find the plugin's own folder).
- Planned dependents (traversal-surfaces.md §3, each edge Core -> Core): `GSTNCombatCore` (H11/H11b impacts and launch params),
  the wall movement (T5 surface filter, private), `GSTNPlatformCore` (T3 shapes), `GSTNLevelObjects` (H11b trampoline).

## Gameplay tags

| Tags | Source | Declared in |
|---|---|---|
| `Surface`, `Surface.Type`, `Surface.Behaviour` (roots) | native | `Public/GSTN_SurfaceTags.h`, namespace `GSTNSurfaceTags` |
| `Surface.Behaviour.WallSlide`, `.WallClimb`, `.Launch`, `.Stand` | native | same |
| `Surface.Behaviour.Slippery`, `.OneWay` | native, reserved (no reader yet) | same |
| `Surface.Event.Launched` | native (local presentation only) | same |
| `Surface.Type.Wall`, `.Glass`, `.Crate`, `.Trampoline` | ini, designer content | `Config/Tags/GSTNSurface.ini` |

No surface tag is ever stored in a sync state: the simulation keeps the `uint8` index of `Surface.Type.*` from the baked table
(`UGSTN_SurfaceUtils::GetSurfaceTypeIndex`). Indices follow the tag names in lexical order, so machines with the same tag set
(already required by Network Prediction) agree. "No wall slide" is not a tag: a surface without `WallSlide` forbids it.

## How a surface is resolved

`UGSTN_SurfaceUtils::Resolve(const FHitResult&)` returns an `FGSTN_SurfaceView`; first match wins:

1. `UGSTN_SurfaceBehaviourComponent` on the hit collider's owner: its `PerComponent` entry for that collider, else `Default`;
2. `IGSTN_SurfaceProvider` on the hit collider, then on its owner (platform shapes, level objects);
3. the physical material table of `UGSTN_SurfaceSettings`;
4. the settings' `Default` (`Source = Default`, which a consumer may treat as "unknown surface").

An empty `SurfaceType` falls back to the material table's type, then the default's. `SurfaceVelocity` comes from the provider
whenever there is one (it is a fact of the pose, not configuration); without a provider it is zero, because the resolution never
reads a live component's velocity. `bMoving` is the collider's mobility.

Rollback: everything read is static configuration or what a provider answers from replicated state, so a resimulated frame
resolves the same surface. Providers must answer from static data or confirmed state only, and write nothing.

## Settings

Project Settings > Plugins > GSTN Surface (`UGSTN_SurfaceSettings`, `GSTNSurface.ini`): `Default` and `PhysicalMaterials`.
Defaults ship in `Config/DefaultGSTNSurface.ini`; a project overrides them in its own `Config/DefaultGSTNSurface.ini`. Every
machine of a match must run the same values.

## Extending in Blueprint

- Mark an actor: add the **GSTN Surface Behaviour** component, fill `Default`, name the colliders that differ in `PerComponent`.
- Answer per face or per state: implement `IGSTN_SurfaceProvider` on a component or actor (`GetSurfaceBehaviour`,
  `GetSurfaceVelocity`, `BreakSurface`). The first two run inside the simulation: no world reads, timers, delays or randoms.
- Read a surface: `Resolve`, `ResolveComponent`, `MatchesBehaviours`, `ComputeLaunchDirection`, `ComputeLaunchSpeed` are
  `BlueprintPure`. All surface fields are `BlueprintReadOnly` (checked by `GSTNSurface.Core.NoRuntimeWriteInBlueprint`).
- A new behaviour: declare a `Surface.Behaviour.<Name>` leaf and give the consumer a `FGameplayTagQuery` field; never branch on a
  tag name in core code.

## Replacing content

The plugin ships no content. Surface classes come from the physical material table and the `Surface.Type.*` leaves; a game adds
its own leaves in its own `Config/Tags` ini and its own table entries in its `DefaultGSTNSurface.ini`.

## Tests

`Automation RunTests GSTNSurface`: `Smoke`, `TagsDeclaredOnce`, `TypeLeavesFromIni`, `TypeIndicesStable`,
`ResolveOrderComponentBeforeProviderBeforeMaterial`, `DefaultPassesEverything`, `PerComponentOverride`,
`TypeFallbackAndProviderVelocity`, `LaunchParamsValidated`, `LaunchDirectionAndSpeedModes`, `MaterialTableBaked`,
`NoRuntimeWriteInBlueprint`. The canary build (plugin alone in a blank project, modularity §3.7 rule 7) is not run yet: the
canary script does not exist.

## Changelog

- 0.1 (2026-10-23): data contract, tags, resolution, settings and tests (plan T1).
