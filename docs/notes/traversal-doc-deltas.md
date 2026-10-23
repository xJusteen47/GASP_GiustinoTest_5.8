# Traversal surfaces: doc deltas found while implementing (to fold into the specs at T9)

Collected per the plan's rule (one docs commit at T9). Each line: where, what changed against the spec, why.

## T1 `GSTNSurfaceCore` (2026-10-23, desktop branch `claude/project-thread-v1plhd`)

- modularity-and-tags §4.1 rule 3 ("*Unsure*: verify that the plugin folder scan is on by default in 5.8"): **it is not.**
  `UGameplayTagsManager::ConstructGameplayTagTree` adds only `ProjectConfigDir()/Tags`; Game Feature plugins register theirs in
  `GameFeaturePluginStateMachine`. An ordinary plugin must call `UGameplayTagsManager::AddTagIniSearchPath(<Plugin>/Config/Tags)`
  in `StartupModule` (done in `GSTNSurfaceCoreModule.cpp`, test `GSTNSurface.Core.TypeLeavesFromIni`). Every plugin that ships a
  tag ini needs the same three lines; the 7.1 checklist should say so. Packaged builds: check that `Config/Tags` of plugins is
  staged (not verified yet).
- traversal-surfaces §2.2 `IGSTN_SurfaceProvider`: `Break()` is named `BreakSurface()` (a bare `Break` reads like a struct
  break node in Blueprint), and the interface gains `GetSurfaceVelocity(const FGSTN_SurfaceQuery&)` (default zero). T3/T4 need the
  surface velocity "reached through `IGSTN_SurfaceProvider` so combat never links `GSTNPlatform`"; declaring it now avoids an
  interface change later.
- §2.2: `FGSTN_SurfaceView` gains `EGSTN_SurfaceSource Source` (Component, Provider, PhysicalMaterial, Default). The wall filter's
  `bUnknownSurfacesPass` (§5.2) can read `Source == Default` instead of guessing.
- §2.2: the velocity comes from the provider whenever one exists, even when a behaviour component supplied the behaviour (velocity
  is a pose fact). Without a provider it is zero: the resolution never reads `GetComponentVelocity` of a live component.
- Plan T1 `ResolveComponent(const UPrimitiveComponent*, FName)`: the `FName` is the bone/body used to find the physical material.
- Plan T1: the physical-material table and the `Surface.Type` index table live in a plain C++ `FGSTN_SurfaceTable` owned by
  `UGSTN_SurfaceSettings`, baked after engine init (`GetOnPostEngineInit`) and again lazily if anything invalidated it. Indices:
  0 = none, then the `Surface.Type.*` tags in lexical order of their full names (deterministic for equal tag sets).
- H11b launch math (`LaunchDirectionModes`, `SpeedModes`) lives as `BlueprintPure` functions in `UGSTN_SurfaceUtils`
  (`ComputeLaunchDirection`, `ComputeLaunchSpeed`), next to the params, so H11b, T4 and Blueprint hooks share one implementation.
  Tilt is spherical around the surface's local up: `TiltPitch` away from it, `TiltYaw` from local +X towards local +Y.
- `FGSTN_SurfaceLaunchParams` defaults (proposal, tunable data): `Speed` 1500, `Restitution` 1, `MinSpeed` 800, `MaxSpeed` 3000,
  `bKeepTangentialVelocity` true, `bResetAirJumps` true, `bResetCoyote` false, `bAddSurfaceVelocity` true.
- Test rig note: `AActor::ProcessEvent` ignores calls until `UWorld::InitializeActorsForPlay`, so a test world that calls a
  `BlueprintNativeEvent` on an actor provider must initialise its actors (the GAS rig already does).
- Not done in T1: the canary build (`Tools/Canary/make_canary.py` does not exist yet) and the `check_modules.py` layers entry (the
  script does not exist yet either).

## T5 wall-surface filter (2026-10-23, desktop branch `claude/project-thread-v1plhd`; owner OK TS.2, re-confirmed in chat 2026-10-23 10:10)

- traversal-surfaces §5.2 / plan T5: the filter is stored on the **modes and the entry transition**, not only in
  `FGSTN_WallCheckSettings`. `UGSTN_WallMovementModeBase::SurfaceFilter` ("Wall|Surface Detection") is copied into
  `FGSTN_WallCheckSettings::SurfaceFilter` by `GetWallCheckSettings`; `UGSTN_WallEntryTransition::SurfaceFilter`
  ("Wall Entry|Detection") is its own property, so the Falling transition for a slide entry and the one for a climb entry each
  carry their query. Both default to empty = today.
- New `UGSTN_WallQueryUtils::PassesSurfaceFilter(Hit, Filter)` is the single place that resolves: empty query returns true
  **before resolving** (zero cost for unconfigured modes); `Source == Default` returns `bUnknownSurfacesPass`; otherwise
  `MatchesBehaviours`. Consequence: `bUnknownSurfacesPass = false` has no effect while the query is empty.
- Plan T5 "cached per component in `FGSTN_WallTickState`": **no cache.** `FGSTN_WallTickState` is not visible to
  `IsHitOnValidWall` (a stateless util called from probes, corner checks and the wrap probe) and a cache would be a state outside
  the sync/aux state (CLAUDE.md rule 1). Cost: one `Resolve` per probe hit that passes the angle test, i.e. up to 3 along-wall
  probes + lookahead + corner/wrap checks per substep, and only for modes with a non-empty query. `Resolve` is a
  `FindComponentByClass` on the owner, an interface check and a table lookup; measure in PIE with the Slate profiler if a mode
  query is turned on for many probes/substeps, and add a per-call local memo inside `FindWall` only then.
- The filter runs right after the angle test of the hit's own normals, **before** the edge-contact line trace (the costly part);
  an edge hit is judged by the component the sweep touched, not by the neighbouring face the trace finds.
- Entry transition: the filter is applied to the swept face and also to the capsule-height confirmation trace (`CenterHit`),
  because the mode's probes start from the capsule centre: a forbidden surface found there would make the mode exit at once and
  the transition grab again every tick.
- Corner cases covered by the same check with no extra code: `IsConcaveCornerHit` (corner onto a forbidden wall), the lookahead
  sample and `FindWallAroundConvexEdge` (wrap around an edge onto a forbidden wall) all go through `IsHitOnValidWall`.
- Module: `GASP_Giustino2.Build.cs` adds `GSTNSurfaceCore` as a **private** dependency; no surface header is included from a
  public header of the project (the public types file only uses `FGameplayTagQuery`).
- Tests are in the project module with the prefix `GSTNMovement.Wall.Filter.*` (the existing project wall tests use `GSTN.Wall.*`:
  run the filter `GSTN.Wall` as well as `GSTNMovement`). The entry transition's `SweepForWall` is protected and needs a
  `FSimulationTickParams`, so it is covered only through the shared `PassesSurfaceFilter` and by `scenario_surface_nowallslide`
  (PIE, not run here).
- Blueprint step left for later (shared asset `SandboxCharacter_Mover`, announced before editing): `SurfaceFilter.RequiredBehaviours`
  = `Surface.Behaviour.WallSlide` on the slide mode and on the Falling to slide transition, `Surface.Behaviour.WallClimb` on the
  climb mode and its transition. Until then nothing changes in play.

## Review fixes (2026-10-23, review of T1 and T5 on `claude/project-thread-v1plhd`)

- §2.2 / modularity-and-tags §4.7 `Surface.Type` indices (review B.2): the indices are what a sync state carries, so they cannot
  move during a match. They are still computed from the tag tree (lexical order), once, at engine init. In the **running game**
  that first bake freezes the table (`FGSTN_SurfaceTable::Freeze`); a later `OnGameplayTagTreeChanged` that would give a different
  list is an error in the log and the table is left alone (the new type reads as 0). The **editor never freezes** (a designer who
  adds a leaf needs its index at once). `UGSTN_SurfaceUtils::GetSurfaceTypeTableChecksum()` and `FGSTN_SurfaceTable::GetTypesChecksum()`
  give a 32-bit CRC of the ordered names (UTF-8, so the value does not depend on the platform's `TCHAR`); the bake logs it. A join
  check now compares it (second review fixes, below). Rule for content: every `Surface.Type.*` leaf lives in a `Config/Tags` ini of the project or of a
  plugin that loads at startup, never in a late Game Feature plugin.
- §2.2 lazy bake (review B.7): `GetTable()` no longer loads assets. A lookup before the engine-init bake gets the types and only the
  physical materials already in memory (`TSoftObjectPtr::Get`, not `LoadSynchronous`), with one warning, and only on the game
  thread (from another thread: an ensure and an empty table). `Rebake()`, `RebakeTypes()` and `InvalidateTable()` are non-const;
  `Rebake()` asserts the game thread. A settings edit or config reload forgets only the material entries (the types stay).
  Reading a baked table from any thread is safe.
- Spec §3 vs §9 (review B.5, B.10): §3 says the wall movement depends on `GSTNSurfaceCore` (private) and the project module does
  (`GASP_Giustino2.Build.cs`, private). §9's acceptance says "the project with `GSTNSurface`... removed, everything builds". **The two
  contradict each other and the code follows §3**: the project does not compile without `GSTNSurfaceCore` (the plugin is
  `EnabledByDefault: false` but the `.uproject` enables it and the module links it). This is a deviation from the canary rule, not an
  oversight: the wall movement (future `GSTNMovement`) is a declared dependent. The "project builds without GSTNSurface" criterion
  cannot be met until the filter sits behind an interface or in an optional module; decide at T9 whether to relax §9 or to split the
  filter out. `check_modules.py` does not exist yet, so nothing catches it today.
- Plan T1 names the tag namespace `GSTN_SurfaceTags`; the code uses `GSTNSurfaceTags` (like `GSTNCombatTags`). The Parts plugin used
  `GSTN_PartsTags` and is renamed `GSTNPartsTags` for the same convention.
- §2.4 warning "Stand only on a component with a walkable face" is not implemented in `IsDataValid` (nothing reads `Stand` yet).
- Known limits left as they are (design choices, review B.8, B.9, B.10, B.13): the wall filter is empty by default on the modes and
  the transition, so it must be set in both places; `bUnknownSurfacesPass` bypasses a customised `UGSTN_SurfaceSettings::Default`;
  `Resolve` also computes velocity and launch for a yes/no filter question (measure before adding a `ResolveBehaviours`); two
  `UGSTN_SurfaceBehaviourComponent` on one actor: superseded, see "Second review fixes" (lowest name wins, validator error).

## Second review fixes (2026-10-23, B.2 rest and B.13)

- §2.2 / modularity-and-tags §4.7 join check (B.2, rest). Design chosen: the server hooks `FGameModeEvents::OnGameModePostLoginEvent` in the
  module (`FGSTN_SurfaceJoinCheck::CheckLogin`) and compares the URL option `GSTNSurfaceTypes=<8 hex>` of the connection's `RequestURL` with
  its own `GetTypesChecksum()`; a mismatch (or, with `UGSTN_SurfaceSettings::bRequireJoinChecksum`, a missing option) is kicked through
  `GameSession->KickPlayer` and logged as an error. **PostLogin, not PreLogin**, because `GameModePreLoginEvent` carries no options and the
  plugin must not override the project's game mode or game session. **The client side is the game's job**: the engine has no global hook
  on the connect URL, so the game returns `FGSTN_SurfaceJoinCheck::GetJoinOption()` from `ULocalPlayer::GetGameLoginOptions` or calls
  `AppendJoinOption` on its travel URL. Until it does, a client without the option is only a warning (the default), so the existing
  PIE harness and projects keep working; turn `bRequireJoinChecksum` on after wiring the game. Not wired in this project (`Source/` was out
  of scope): **open item for the project module**. The kick path needs a real connection and is not covered by an automation test (the
  decision, `Evaluate`, and the option round trip are); verify with the dedicated-server PIE harness by joining with a different tag set.
- B.2 empty bake: `FGSTN_SurfaceTable::Freeze()` returns false and does nothing for a table with no types. The bake after engine init warns
  and leaves the table open; `RebakeTypes` (tag tree change) freezes it on the first non-empty set once the engine has started. Before, a
  first bake that ran with an incomplete tag tree froze "no types" for the session.
- B.13: `UGSTN_SurfaceBehaviourComponent::FindFor(Actor)` picks the component with the lowest name instead of the first one the actor's
  unordered component set returns, so every machine resolves the same surface. A second component is now an **error** (was a warning) in
  `CollectFindings` / `IsDataValid`. `ResolveQuery` uses `FindFor`.
- Test nit: `GetTableNeverLoadsAssets` adds a warning (`VACUOUS: ...`) when no unloaded engine asset is found under `/Engine/EngineMaterials`,
  instead of an info line; it still checks the table contents.
