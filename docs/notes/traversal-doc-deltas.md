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
