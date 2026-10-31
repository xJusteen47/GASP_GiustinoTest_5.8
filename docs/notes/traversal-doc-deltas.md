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

## T3 platform shapes with behaviours (2026-10-25, desktop branch `claude/project-thread-v1plhd`; owner approved T3 2026-10-25)

`GSTNPlatform` did not exist: T3 created it with the fighter minimum (first commit) and then the shapes. Deviations from
many-enemies-replication.md §4 (PL), traversal-surfaces.md §4 (TS), modularity-and-tags.md and the plan:

- **Modules.** `GSTNPlatformCore` (pure) and `GSTNPlatformRuntime` (components, colliders, pose point), not the plan's `GSTNPlatform`
  module nor modularity's `GSTNPlatformNP`: the runtime has no Network Prediction code (it reads Mover's time step), so "NP" would be
  wrong, and `*Runtime` is the pattern of `GSTNInteract` and `GSTNDepthMarker`. Dependencies are `GSTNSurfaceCore` and Mover only (plan
  T3 acceptance); modularity §3.5/§3.6's `GSTNCombatCore` (lag-compensation time) and the fork are not needed before T7 and are not
  linked. Modularity §3.5 table and §3.6 row to update at T9.
- **Pose point driver (PL §4.3, TS §4.2).** There is no NP post-fixed-tick hook yet (combat-m8-minions.md spike 2) and the plugin may
  not depend on NP, so the pose point runs at the start of every Mover simulation tick of the world (`OnPreSimulationTick`), once per
  frame (deduplicated on frame and time), posing the end of frame F-1 with `Frame = ServerFrame - 1`, `TimeMs = BaseSimTimeMs`
  (server timespace, the same number on the server, a predicting client and a re-simulation). Consequences: simulation that ticks
  before Mover in the same frame (combat 47, ASC 48) sees the platforms one frame older, and on the first frame of a re-simulation at
  the newest pose; none of it sweeps against platforms today. When the hook exists it calls `UGSTN_PlatformSubsystem::ApplyPosePoint`
  and nothing else changes. On clients the spec's "interpolation sample once per game tick before physics" becomes "the same pose
  point, reading the owner's (interpolated) transform at that moment" for approach B; approach C (scripted) is exact.
- **Approach C as a component.** `UGSTN_ScriptedPlatformComponent` (pose = `FGSTN_PlatformScriptedPath::Evaluate(origin, frame
  time)`, origin replicated initial-only, owner root moved with the colliders) is how the scripted test fighter meets wall-slide-v2.md
  §6.14's requirement (base pose of a frame identical on every machine). It is the `AGSTN_WallTestMover` idea moved into the pose point.
- **Collision channel (PL §4.3, plan §6).** In the plugin's `Config/Engine.ini` (merged by the engine's plugin modification layer
  `{PLUGIN}/Config/{TYPE}.ini`; a plugin `DefaultEngine.ini` is **not** merged, checked: the channel test failed with it), slot
  `GameTraceChannel4`, found by name. The project's `DefaultEngine.ini` is untouched.
- **Collision from behaviour (TS §4.1).** A table of `FGSTN_PlatformCollisionRule { FGameplayTagQuery When; BlockChannels }` in
  `UGSTN_PlatformSettings` (shipped rules built from `MoverChannels`, `WallProbeChannels`, `StandAndLaunchExtraChannels`), so neither
  channels nor behaviour leaves are hard-coded (modularity §3.7 rule 4). This project's wall modes probe with the capsule's object type
  (Pawn), the same channel as the capsule: collision cannot keep a Stand-only top from being a wall; the surface the wall entry
  resolves does (no `WallSlide`). Collision is query-only (no physics body interacts with a collider that teleports every frame).
- **Colliders (plan `UGSTN_PlatformShapeComponent`).** Three classes, `UGSTN_Platform{Box,Sphere,Capsule}Collider`, because the
  engine's primitives are separate classes and GSTNSurface looks for the provider on the hit component's class; a C++ interface
  `IGSTN_PlatformCollider` finds the platform and shape id. Convex is not supported; Stand requires a Box.
- **`SetShapeMask(uint8, EGSTN_PlatformShapeState)`**: the second argument is how disabled shapes go (`Breaking` = with the grace
  telegraph, `Inactive` = at once); shapes turned on are Active at once. A second input `SetActionState(EGSTN_PlatformActionState)`
  carries PL §4.2's `ActiveActionStates` (spec default, per-shape override); T7 maps combat state onto it. States are **timelines by
  frame** (`FGSTN_PlatformShapeStates`): a request is stamped with the next frame the pose point simulates, and a re-simulated frame
  gets the collision it had the first time (a stepped state would stay off during the replay). `Braced` is reserved (no producer).
- **Spec fields (PL §4.2).** `bIgnoreBaseRotation` and `bImpartVelocityOnLeave` are not in the spec (they are the pawn modes' settings,
  nothing in the platform would read them); `MaxSpeedWhileOccupied` and `BraceTimelineOnMount` moved into `FGSTN_OccupantResponse`
  (TS.4), data only; `OnOccupantChanged` needs occupancy and is T7. Shape ids 0..7, names `Surface0..7` (TS §4.1).
- **"Never a bone or the capsule" (TS §4.1, test `ValidatorRejectsBoneAndCapsule`).** Shapes are always new primitives, so the enemy's
  capsule or mesh cannot be one. The validator rejects a shape whose `Bone` is set (a field that must stay None, kept so hurtbox-style
  data pasted into a shape list fails loudly) and Stand on a sphere or capsule (its top fails the walkable test at the edges). The wall
  angle check is a warning; with the default 45..120 range a box always has a face in range, so it only fires for narrow ranges.
- **Part names (TS §4.1).** A warning in `GSTNPlatform` alone (the shape behaves as body); the bridge (T6) makes unknown names errors.
- **Surface velocity (TS §4.3).** `GetSurfaceVelocityAt(ShapeId, Point)` = rigid-frame velocity of the owner between the end of the
  frame before and the end of the frame the colliders hold, from `FGSTN_PoseHistory` (24 frames, in `GSTNPlatformCore`, not in the
  `GSTNCore` of modularity §3.6, which does not exist). Zero for an inactive shape. It reaches GSTNSurface through the provider's
  `GetSurfaceVelocity`. **Gap:** the colliders are teleported, so their physics velocity is zero and the wall modes'
  `bAddBaseVelocityOnExit` and Mover's leave-base velocity add nothing on leaving a platform shape; reading the provider's velocity
  there is a mode subclass (T7) or T4 for launches, never a core edit.
- **A shape's surface can be shadowed.** A `UGSTN_SurfaceBehaviourComponent` on the enemy wins over every shape (resolution step 1):
  the platform's validation and its registration log warn about it.
- **Scenario finding (TS §4.3, T2).** Re-grabbing the same moving shape after sliding off it reconciled on the client (ASC
  `GA_WallSlide`/`GA_Jump` plus Mover) until the harness reset the wall memory: the entry's same-wall `HeightCap` policy is judged
  differently on the two machines when the wall moves. This is exactly what T2's `UGSTN_SurfaceWallEntryTransition` (moving base ->
  `ReentryCooldown` only) is for; T2 is not built on this branch. The scenario resets the memory (`gstn.Wall.ResetMemory`) before each
  grab, as `wall_ab.py` does.
- **`.uproject`.** `GSTNPlatform` is enabled (plan §6 lists it as an owner-OK edit; taken as covered by the T3 approval, as for the other
  GSTN plugins of this branch). Remove that one entry to keep the plugin off.
- Not done in T3: the canary build and the `check_modules.py` layers entry (neither script exists), the dedicated-server run of the
  scenario (listen only), the presentation component (TS §8).

## T2 wall movement on moving surfaces (2026-10-25, desktop branch `claude/project-thread-v1plhd`; owner approved T2 2026-10-25, "ignore the legacy wall slide")

- **Root cause (plan T2 and TS §4.3 assumed the world-height `HeightCap`; it was not that).** The T3 fighter moves sideways only, so world and wall-relative
  heights are the same. The reconcile came from the memory **reset**: "the character went through `MemoryResetModes` after the wall" reads Mover's
  `CommonBlackboard::LastModeChangeRecord`, a rollback blackboard entry (`InvalidatedOnRollback`, `MoverComponent.cpp`). The harness's teleport (applied to both copies
  of the pawn at different frames) makes a correction right after the take-off; when the client rolls back to a frame after the server's take-off and before its own, its
  take-off record is erased and the re-simulation (already in the air) never writes it again. Server: record `Walking -> Falling` after the wall, memory reset, grab.
  Client: the older record (`Falling -> Walking`, the landing), no reset, `HeightCap` refuses (log: client `ENTRY rejected: height cap (486.2 > 126.7)` every tick,
  server `ENTRY -> WallSlidingV2 ... memReset=1`), then the mode, GA_WallSlide and the position reconcile. Static walls have the same hazard; moving shapes only made it
  visible because the scenario re-grabs the same face.
- **Design.** No `UGSTN_SurfaceWallEntryTransition` (the plan's class): the transitions in use are the surface-driven twins, and the rules are needed by both. The
  re-entry rules moved from `FindEntryWall` into the pure `UGSTN_WallReentryUtils::Judge` (same order and answers; test `LegacyRulesUnchanged` compares it with a copy of
  the old code on 13 824 cases) with two switches, off in `UGSTN_WallEntryTransition` and on in `UGSTN_WallSurfaceModeTransition` and
  `UGSTN_AbilityWallSurfaceModeTransition` (constructor defaults; the Blueprint needs no edit, `setup_wall_slide_modes.py --check` confirmed):
  - `bInferModeChangeLostToRollback`: a record whose mode is not the start sync state's mode (the authority's) is stale; the lost change left the record's mode
    after its time, so a reset mode entered after `LastWallTimeMs` still resets. Not recovered: two or more lost changes in one rollback window (a landing and a take-off
    within ~0.5 s of a correction: the client keeps the memory, one reconcile); a false reset needs three lost changes through a non-reset mode.
  - `bBaseRelativeHeightCap` (the plan's intent kept, not cooldown-only): on a Movable hit the cap compares the capsule's height above the wall's origin with the same
    quantity at the exit. The v2 modes (`bRecordWallBaseInMemory`, default off in the base mode) store the wall origin's height in the synced memory
    (`FGSTN_WallMovementSyncState::bLastWallOnMovingBase`, `LastWallBaseHeight`: one more flag bit, a float only when set, compared with the 2 cm height tolerance), from
    the probe's component or, on a tick that leaves before probing (a wall jump), from the base ridden at the start of the tick (`FGSTN_WallTickState::RiddenBase`,
    tick-local). A memory without a wall frame and a moving wall: cooldown only (the plan's proposal, now the fallback). Static walls keep world heights. Rejected:
    cooldown-only on every moving wall (it lets a player climb a moving wall by wall-jumping and re-grabbing) and storing the base component itself in the sync state (an
    object reference on the wire for an identity the "same wall" rule never had: walls are "the same" by facing).
- **Plan T2 settings verified, not changed** (`setup_wall_slide_modes.py --check`, read-only, on `SandboxCharacter_Mover`): `WallSlidingV2` and `WallClimbingV2` have
  `bUseMovementBases = true`, `BaseRotation = FollowExceptRoll`, `bAddBaseVelocityOnExit = true`, `bRideFromSyncedLocation = true`, `bRecordWallBaseInMemory = true`; the
  Falling transition (`GSTN_AbilityWallSurfaceModeTransition`) has both switches on, `HeightCap`, 30 deg, 10 cm, 0.25 s, reset mode `Walking`. `--fix-t2` sets them if a
  later edit loses them. The script no longer copies `ride_from_synced_location` / `record_wall_base_in_memory` from the legacy mode into a new v2 mode, nor the T2 switches
  from a plain transition into a surface-driven one (both would have switched the C++ defaults off), and the climb mode's tilt flag skip now uses the Python name
  (`allow_capsule_tilt`; it was `b_allow_capsule_tilt`, which matched nothing).
- **Optional `UGSTN_WallSlideOnBaseMode`: not needed.** TS-B (a disabled base) is handled by the existing exit path; nothing in T3's runs kept a stale base.
- **Edits outside `WallMovement/`**: one, `Private/GAS/GSTN_AbilityWallSurfaceModeTransition.cpp` (two constructor lines turning the switches on), because the ability
  twin is the transition the character uses and it lives in `GAS/`.
- **`bAddBaseVelocityOnExit` on platform shapes** still adds nothing (T3 gap: the colliders are teleported, physics velocity zero): unchanged, T4/T7.
- Harness: `surface_scenarios.py` keeps the memory by default (`--reset-memory` restores the T3 workaround, `--log N` sets `gstn.Wall.Log`). `wall_ab.py` still resets it
  per lane: each lane is an independent wall test, not a re-grab test.
- **Harness, `scenario_surface_wallslide_enemy`** (listen host + 1 client, 60/15 ms, 0 % loss, 3 reps per subject, memory kept). Before: client reps 1 and 2 failed,
  `launch_to_grab` 3 unexplained reconciles each (Mover 2, ASC 1), the client refused the grab (height cap) and was put on the wall by the correction (relative drift -48 / +62 cm).
  After: 6/6 pass, client `launch_to_grab` 1 unexplained per rep (the harness's own dual-teleport correction, the same as rep 0 before), `on` 0, host 0; 4 of 18 grabs logged
  `memReset=1 (mode (record lost to a rollback))`, i.e. the inference fired exactly where the old rule diverged.
- **Network checks completed 2026-10-26:** the scenario at 2 % loss, the moving lanes (`wall_ab.py --lanes "^h_"`) and the standard v2 wall scenarios at 0 % and 2 %; numbers in wall-slide-v2.md 6.15.
