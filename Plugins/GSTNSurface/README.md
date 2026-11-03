# GSTNSurface

The traversal surface data contract for Unreal Engine 5.8: which mechanics a collider offers (wall slide, wall climb,
launch, stand), its impact class (`Surface.Type.*`) and its launch parameters, and one stateless way to resolve all of it
from a hit. Level objects (walls, trampolines) and enemy body parts carry the same data, so the wall movement, the combat
impacts and the platforms read surfaces without knowing what they hit.

Status: **experimental (0.1)**, `EnabledByDefault: false`. This is checkpoint T1 of
[traversal-surfaces-plan.md](../../docs/architecture/plans/traversal-surfaces-plan.md); the design is
[traversal-surfaces.md](../../docs/architecture/traversal-surfaces.md) (§2 data model, §3 plugin placement). The wall-surface filter
(T5) is built: the project's wall movement reads this plugin. The other consumers (platform shapes T3, launch T4, H11 impacts) are not built yet. Enabling the plugin changes nothing
until a designer marks a surface: unmarked geometry resolves to the settings' default, which offers everything the wall and
walking modes did before.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNSurfaceCore` | exists (Runtime, Default) | Types, tags, provider interface, behaviour component, settings, the baked lookup table and the resolution utils. Pure data and stateless functions: no GAS, Network Prediction or Mover, so it needs no fork. Covered by `GSTNSurface.Core.*`. |

The module class (`Private/GSTNSurfaceCoreModule.cpp`) registers the plugin's `Config/Tags` folder with the tag manager (the
engine reads `Config/Tags` only for the project and for Game Feature plugins, not for ordinary plugins), bakes the surface table
after engine init so no lookup inside the simulation loads assets. In the editor it rebuilds the type indices when the tag tree
changes; in the running game the type indices are frozen by that first bake (see "Gameplay tags").

## Dependencies

- Plugins: none.
- `GSTNSurfaceCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags, PhysicsCore, DeveloperSettings; private
  Projects (to find the plugin's own folder), NetCore (the join check closes a connection).
- Planned dependents (traversal-surfaces.md §3, each edge Core -> Core): `GSTNCombatCore` (H11/H11b impacts and launch params),
  the wall movement (T5 surface filter, private; built: the project module depends on it today), `GSTNPlatformCore` (T3 shapes), `GSTNLevelObjects` (H11b trampoline).

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

The indices must not move while a match runs. In the **running game** the table is frozen by the bake after engine init: a tag
added later (a Game Feature plugin, an ini that loads late) that would shift the indices is logged as an error and ignored, so
its type reads as "no type" instead of silently renumbering the others. Put every `Surface.Type.*` leaf in a `Config/Tags` ini of the
project or of a plugin that loads at startup. The **editor** never freezes, so a designer sees a new leaf's index at once. An **empty table is never frozen**: a first bake that
runs before the tag tree is complete and finds no `Surface.Type` leaf would otherwise lock "no types" and refuse every leaf that
registers afterwards. It logs a warning and freezes with the first non-empty set the tag tree gives (`FGSTN_SurfaceTable::Freeze`
returns false for an empty table).
`UGSTN_SurfaceUtils::GetSurfaceTypeTableChecksum()` is a 32-bit checksum of the ordered type list (the bake logs it too): machines
with the same value agree on every index, and a client whose value differs from its server's reads the wrong surface types with no
other sign. The server compares it at login (below).

**Join check.** A client announces its checksum as the URL option `GSTNSurfaceTypes=<8 hex digits>`; the module hooks
`FGameModeEvents::OnGameModePostLoginEvent` on the server (`FGSTN_SurfaceJoinCheck::CheckLogin`, `Public/GSTN_SurfaceJoinCheck.h`) and
kicks a client whose value differs from the server's (`GameSession->KickPlayer`). It is PostLogin and not PreLogin because the
engine's `GameModePreLoginEvent` does not carry the URL options and the plugin cannot override the project's game mode. The engine has
no global hook on the client's connect URL, so the **game must send the option**: return `FGSTN_SurfaceJoinCheck::GetJoinOption()` from its
`ULocalPlayer::GetGameLoginOptions` (or call `AppendJoinOption` on the URL it passes to `ClientTravel` / `OpenLevel`). A client that sends
nothing is logged as a warning and accepted, unless `UGSTN_SurfaceSettings::bRequireJoinChecksum` is on (turn it on once the game sends
the option). A listen server's own player and a standalone game have no connection and are not checked. The comparison itself is
`FGSTN_SurfaceJoinCheck::Evaluate` (tested); the kick needs a connection and is checked in the PIE harness.

## How a surface is resolved

`UGSTN_SurfaceUtils::Resolve(const FHitResult&)` returns an `FGSTN_SurfaceView`; first match wins:

1. `UGSTN_SurfaceBehaviourComponent` on the hit collider's owner (one per actor; with several the one with the lowest name is read, so
   every machine picks the same, and the validator reports an error): its `PerComponent` entry for that collider, else `Default`;
2. `IGSTN_SurfaceProvider` on the hit collider, then on its owner (platform shapes, level objects);
3. the physical material table of `UGSTN_SurfaceSettings`;
4. the settings' `Default` (`Source = Default`, which a consumer may treat as "unknown surface").

An empty `SurfaceType` falls back to the material table's type, then the default's. `SurfaceVelocity` comes from the provider
whenever there is one (it is a fact of the pose, not configuration); without a provider it is zero, because the resolution never
reads a live component's velocity. `bMoving` is the collider's mobility.

Rollback: everything read is static configuration or what a provider answers from replicated state, so a resimulated frame
resolves the same surface. Providers must answer from static data or confirmed state only, and write nothing. `GetTable()` never
loads an asset: the module bakes the table (loading the physical materials) after engine init; a lookup before that bakes the types
and only the materials already in memory, on the game thread, with a warning once (from another thread it returns an empty table and
fails an ensure). Reading a baked table from any thread is safe; baking it (`Rebake`) is game thread only.

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
`NoRuntimeWriteInBlueprint`, `TypeTableChecksum`, `FrozenTypeTableKeepsIndices`, `GetTableNeverLoadsAssets` (reports a warning when it finds no unloaded engine asset to prove the point, instead of passing silently), `EmptyTypeTableIsNeverFrozen`, `JoinCheckComparesChecksums`, `SeveralSurfaceComponentsPickLowestName`. The wall filter built on
this plugin is tested in the project module (`GSTNMovement.Wall.Filter.*`). The canary build (plugin alone in a blank project, modularity §3.7 rule 7) is not run yet: the
canary script does not exist.

## Changelog

- 0.1 (2026-10-23): data contract, tags, resolution, settings and tests (plan T1).
- 0.1.2 (2026-10-23, review fixes): connect-time type table check (server kicks a mismatching client; the game sends `GetJoinOption`), an empty type table is never frozen, the surface component is picked by lowest name and a second one is an error, `GetTableNeverLoadsAssets` warns when vacuous.
- 0.1.1 (2026-10-23, review fixes): the type table freezes in the running game and exposes a checksum, `GetTable()` never loads, `Rebake` is non-const and game thread only; README aligned with T5.
