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
