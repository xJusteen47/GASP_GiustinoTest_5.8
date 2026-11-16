# Traversal surfaces implementation plan: surfaces on level objects and enemy body parts (draft for owner review)

Plan only, no code (2026-10-22 evening). Checkpoints are **T0..T9**; other plans reference them by that name.

## 0. Sources and what each checkpoint implements

| Id | Source | Used for |
|---|---|---|
| TS | [traversal-surfaces.md](../traversal-surfaces.md) | the spec; section numbers below are TS §n |
| DD | [game-design-direction.md](../game-design-direction.md) | §2d (gravity, trampolines), §2f, §4, §5b |
| WM | [wall-movement.md](../wall-movement.md) | wall modes, base riding, "subclass, never edit the core" |
| H | [m4-hits-plan.md](m4-hits-plan.md) | H10 (lag comp history), H11 (contact record, `Surface.Type.*`), H11b (trampoline, `FGSTN_SurfaceLaunchParams`) |
| P | [m4b-parts-plan.md](m4b-parts-plan.md), [combat-parts.md](../combat-parts.md) | P2 (part names), P6 (fragment, `BrokenMask`), P7/P7b (confirmed mask, restore), P16 (minion parts) |
| PL | [many-enemies-replication.md](../many-enemies-replication.md) §4, [combat-m8-minions.md](../combat-m8-minions.md) §4d.5 | `GSTNPlatform` (spec, spikes P1–P4), `GSTNPlatformMinion` seams |
| MT | [modularity-and-tags.md](../modularity-and-tags.md) | §3.7–3.9, §4, §5b |
| OQ | [open-questions-m4-m10.md](../open-questions-m4-m10.md) | TS.1–TS.8 |
| T | [testing.md](../testing.md) | harness, test prefixes |

| Checkpoint | Spec sections |
|---|---|
| T0 spikes | TS §7 (TS-A/B/C), PL §3.4 P1–P4 |
| T1 `GSTNSurface` plugin, data contract, tags | TS §2, §3 |
| T2 wall movement on moving surfaces (no core edit) | TS §4.3, §5.1 |
| T3 platform shapes with behaviours | TS §4.1, §4.2, §4.4 (mask input) |
| T4 launch from moving surfaces | TS §4.3 launch, H11/H11b consumption |
| T5 wall-surface filter (core edit, owner OK) | TS §5.2 |
| T6 `GSTNPlatformParts` bridge | TS §4.4 |
| T7 lifecycle, lag comp, scenarios | TS §6, §7 |
| T8 platform minions' shapes | TS §4.5 |
| T9 content, validators, harness, docs | TS §8, §9 |

## 1. Ground rules for every checkpoint

Same as the M4 plan §1: one checkpoint = one commit; gate = full build, `Automation RunTests GSTNSurface GSTNPlatform
GSTNCombat` green, the listed PIE scenarios clean in the **listen-server** configuration (host + 2 clients) and dedicated + 2,
at 60/15 ms with and without 2 % loss once NP or Mover code is touched; earlier scenarios still green. Epic coding standard;
native tags with `UE_DEFINE_GAMEPLAY_TAG_COMMENT` in the owning module, `Surface.Type.*` leaves ini, no tag in sync state;
README of each touched plugin updated in the same checkpoint; **R-B independence** per checkpoint (plugin builds and tests
without the project module, deps only on other plugins' `*Core`, bridges excepted, no `/Game/` refs, settings in the plugin's
`UDeveloperSettings`); Blueprint extension (MT §3.8) for every behaviour class; soft refs only (MT §3.9); no `IsServer()`
branch in simulation code; wall-movement core edited **only** in T5 and only after the owner's OK (WM "How to extend").
Doc deltas go to `Saved/Notes/traversal-doc-deltas.md`; one docs commit at T9.

Clean (T): snapshots equal on all machines; no reconciles besides forced ones and the expected single correction at mount and
at surface loss on clients; host pawn zero reconciles.

## 2. Dependencies, and what can start when

| Checkpoint | Needs | Can start |
|---|---|---|
| T0 | wall modes as they are; a scratch kinematic collider; H11b not needed | **now** (scratch branch or `Saved/`) |
| T1 | nothing (pure plugin) | **now**; H11/H11b consume it, so T1 should land before H11 starts |
| T2 | T0 TS-A; project wall code (one PC, same branch as the M3 work) | after T0 |
| T3 | T1; `GSTNPlatform` skeleton (`FGSTN_PlatformSpec`, `UGSTN_EnemyPlatformComponent`, collider rule, PL §4.2–4.3). If the platform plan has not started, T3 **delivers that minimum** for fighters and the platform plan builds on it (TS §10) | after T1 |
| T4 | T3; **H11 and H11b** (contact record, launch layered move, `Surface.Type.Trampoline` row) | after H11b |
| T5 | T1 (owner OK TS.2 given 2026-10-22 22:14) | after T1 |
| T6 | T3; **P7** (confirmed `BrokenMask` readable, snapshot) and **P7b** (`Combat.Event.Part.Restored`) | after P7b |
| T7 | T2, T3, T6; **H10** (lag comp history with composed transforms); platform spikes P3/P4 | after H10 and T6 |
| T8 | T6; **M8 complete** (`GSTNPlatformMinion`, store, observer) and **P16** (minion part mask) | late |
| T9 | all; M5 E10 (validation mechanism) for the validators | last |

Parallel threads: {T0, T1} immediately; T2 on the PC with the wall code; T3 after T1 on a separate thread; T4/T6/T7 follow M4
and M4b progress; T5 waits only for the owner.

## 3. Checkpoints (in order)

### T0 Spikes (no plugin code; `Saved/Notes/traversal-spike-report.md`, to become combat.md §17 rows) — size M

- TS-A **Wall slide on a kinematic collider**: a scratch Blueprint actor with an unattached Movable box moved in a post-fixed-tick
  hook (host) / interpolated (client) at 150/300/500 cm/s with yaw rotation; host and a client slide, climb, jump off. Measure
  reconciles per 30 s, max correction, `bAddBaseVelocityOnExit` velocity error. Exit: 0 unexplained reconciles while sliding at ≤
  300 cm/s, grab success ≥ 95 % at 300 cm/s, exit momentum error ≤ 50 cm/s. **Stop and ask** if sliding itself reconciles (base
  riding broken for kinematic unattached primitives): then approach D is a prerequisite and T2/T3 move after the platform plan.
- TS-B **Collider disabled under a sliding player** (`SetCollisionEnabled(NoCollision)` at the defined point, with and without a
  20-frame telegraph): no penetration, no stuck `MovementBase`, exit into Falling with base velocity, ≤ 1 correction on the client.
  Stop and ask if Mover keeps a null/disabled base more than one frame (then a mode subclass clears it, T2).
- TS-C **Launch from a moving surface** (prototype on the walking-mode landing, a fixed-speed launch along the box's up plus base
  velocity): landing point equal on host and clients within 10 cm at 300 cm/s. Depends on nothing of H11 (a scratch queued move).
- Reuse: platform spikes P1–P4 (PL §3.4) are the same measurements for `Stand`; if they already ran, only TS-A/TS-B/TS-C.
- Acceptance: report with numbers per cell; the go/no-go for approach B written down.

### T1 `GSTNSurface` plugin: data contract and tags (`GSTNSurfaceCore`) — size M

- Files (`Plugins/GSTNSurface/Source/GSTNSurfaceCore/`): `Public/GSTN_SurfaceTags.h` (`namespace GSTN_SurfaceTags`),
  `Public/GSTN_SurfaceTypes.h` (`FGSTN_SurfaceBehaviour`, `FGSTN_SurfaceLaunchParams` — moved here from H11b's `GSTNCombatCore`
  with the same fields plus `bAddSurfaceVelocity`; `FGSTN_SurfaceQuery`, `FGSTN_SurfaceView`), `Public/GSTN_SurfaceProvider.h`
  (`IGSTN_SurfaceProvider`: `GetSurfaceBehaviour`, `Break`; replaces `IGSTN_ImpactSurface` and `IGSTN_LaunchSurface`),
  `Public/GSTN_SurfaceBehaviourComponent.h`, `Public/GSTN_SurfaceUtils.h` (`UGSTN_SurfaceUtils::Resolve(const FHitResult&)`,
  `ResolveComponent(const UPrimitiveComponent*, FName)`, `BlueprintPure`), `Public/GSTN_SurfaceSettings.h` (`UDeveloperSettings`:
  physical material table, `Default`), `Private/*.cpp`; `Config/DefaultGSTNSurface.ini`, `Config/Tags/GSTNSurface.ini`
  (`Surface.Type.{Wall,Glass,Crate,Trampoline}`); README; `GSTNSurface.uplugin` (`EnabledByDefault: false`, version 0.1).
- Tags (native): `Surface` root, `Surface.Behaviour.{WallSlide,WallClimb,Launch,Stand}`, reserved `Surface.Behaviour.{Slippery,OneWay}`,
  `Surface.Event.Launched`. The physical-material → type index bake that H11 planned inside `GSTNCombatNP` moves here (`Resolve`
  returns the index; combat keeps storing only the `uint8`).
- Tests `GSTNSurface.Core.{ResolveOrderComponentBeforeProviderBeforeMaterial, DefaultPassesEverything, PerComponentOverride,
  LaunchParamsValidated, TagsDeclaredOnce, MaterialTableBaked, NoRuntimeWriteInBlueprint(validator)}`.
- Acceptance: R-B canary (`GSTNSurface` alone in a blank project builds and tests); a Blueprint actor with the component and a
  Blueprint provider resolve correctly; `check_modules.py` layers file lists the new plugin with no outgoing GSTN edge.
- Spec: TS §2, §3. Edits to other plans: H11/H11b consume `GSTNSurfaceCore` (marked in m4-hits-plan.md on 2026-10-22).

### T2 Wall movement on moving surfaces, no core edit (project `WallMovement/`, later `GSTNMovement`) — size S/M

- Files: `Public/WallMovement/Transitions/GSTN_SurfaceWallEntryTransition.h` (`UGSTN_SurfaceWallEntryTransition :
  UGSTN_WallEntryTransition`: on a hit whose component is Movable, `SameWallPolicy` degrades to `ReentryCooldown`; otherwise parent
  behaviour), optional `UGSTN_WallSlideOnBaseMode : UGSTN_WallSlideMovementMode` only if TS-B showed a stale base to clear (override
  `CheckForWallExit`). No change to `UGSTN_WallMovementModeBase`, utils or sync state.
- Settings verified and documented: `bUseMovementBases = true`, `BaseRotation = FollowExceptRoll`, `bAddBaseVelocityOnExit = true`
  on the slide and climb modes of `SandboxCharacter_Mover` (shared Blueprint: announced before editing, §6).
- Tests: `GSTNMovement.Wall.Surface.{MovableHitUsesCooldownOnly, StaticHitKeepsHeightCap}` (project test module). Scenario
  `scenario_surface_wallslide_kinematic` (the T0 scratch actor made a test asset: host + client slide and jump off; 0 reconciles while
  sliding, exit velocity includes the base).
- Acceptance: a Blueprint with the new transition in Falling's `Transitions` grabs a moving kinematic box; the M3 wall scenarios
  still clean.
- Spec: TS §4.3, §5.1, WM "How to extend".

### T3 Platform shapes with behaviours (`GSTNPlatformCore`, `GSTNPlatform`) — size L

- Precondition: `GSTNPlatform` skeleton (PL §4.2–4.4: `FGSTN_PlatformSpec`, `UGSTN_EnemyPlatformComponent`, `GSTN_EnemyPlatform`
  channel, post-fixed-tick pose point, `FGSTN_PoseHistory`). If absent, this checkpoint creates the plugin with exactly that minimum
  for **fighters** (no proxy actor, no minion seams), as the first commit of the platform plugin.
- Files: `GSTNPlatformCore/Public/GSTN_PlatformShape.h` (`FGSTN_PlatformShape { shape, ShapeId 0..7, FGSTN_SurfaceBehaviour
  Behaviour, FName PartName, ActiveActionStates override }`, `FGSTN_PlatformSpec::Shapes` typed to it; `IsDataValid` rules of TS §4.1),
  `GSTNPlatform/Public/GSTN_PlatformShapeComponent.h` (the kinematic primitive per shape, implements `IGSTN_SurfaceProvider`,
  collision responses derived from `Behaviour` in `OnRegister`), `UGSTN_EnemyPlatformComponent::SetShapeMask(uint8, EGSTN_PlatformShapeState)`
  applied at the pose point; per-shape `Platform.State` and `DropGraceFrames` per shape; pose-history velocity sample exposed as
  `GetSurfaceVelocityAt(ShapeId, FVector)` for `bAddSurfaceVelocity`.
- Tags: none new (`Platform.State.*` per shape, already `GSTNPlatform`'s).
- Tests `GSTNPlatform.Shapes.{MaskDisablesShapes, CollisionDerivedFromBehaviour, PartNameWarningWithoutParts, BreakingGraceThenOff,
  PoseAppliedOncePerStep, ValidatorRejectsBoneAndCapsule}`. Scenario `scenario_surface_wallslide_enemy` (a test fighter with a
  `WallSlide` flank and a `Stand` top, moved by a scripted path at 300 cm/s: host and client slide, stand, jump off; 0 reconciles while
  on it, ≤ 1 at mount).
- Acceptance: a designer gives a fighter shapes with behaviours in a data asset without C++; R-B (`GSTNPlatform` builds with
  `GSTNSurfaceCore` and Mover only).
- Spec: TS §4.1, §4.2, §4.4 (input only), PL §4.2–4.6.

### T4 Launch from moving surfaces (`GSTNCombatNP`, after H11/H11b) — size M

- Files: H11's contact probe and the walking-mode landing hook call `UGSTN_SurfaceUtils::Resolve` (replacing the `IGSTN_LaunchSurface`
  cast); `FGSTN_LayeredMove_Launch` gains the base velocity term when `bAddSurfaceVelocity` (from the platform component's
  `GetSurfaceVelocityAt`, reached through `IGSTN_SurfaceProvider` so combat never links `GSTNPlatform`); H11b's "Mobility != Static"
  validator warning becomes "Movable only if the component is a platform shape" (i.e. implements the provider with `bMoving`).
  Tumbling victims: unchanged (bounce row).
- Tags: none new; `Surface.Event.Launched` is raised by the applier for presentation.
- Tests `GSTNCombat.NP.Hits.Launch.{MovingSurfaceAddsVelocity, DirectionFollowsPosedNormal, ResimReproducesLaunch}`. Scenario
  `scenario_surface_launch_part` (trampoline back on the scripted fighter: landing point equal on all machines within 10 cm;
  a tumbling victim thrown on it bounces once). `scenario_trampoline` (H11b) still clean.
- Acceptance: launch equal everywhere, host zero reconciles; combat has no dependency on `GSTNPlatform`.
- Spec: TS §4.3 launch, H11/H11b.

### T5 Wall-surface filter extension point (wall-movement core, owner OK TS.2 given 2026-10-22 22:14) — size S/M

- Files: `GSTN_WallMovementTypes.h` (`FGSTN_WallSurfaceFilter { FGameplayTagQuery RequiredBehaviours; bool bUnknownSurfacesPass }`
  added to `FGSTN_WallCheckSettings`), `UGSTN_WallQueryUtils::IsHitOnValidWall` evaluates it after the angle test (static resolve,
  cached per component in `FGSTN_WallTickState`), `UGSTN_WallEntryTransition::SweepForWall` likewise; defaults = empty query = today.
  Mode defaults: slide modes query `WallSlide`, climb modes `WallClimb` (set on the shared Blueprint, §6). Module dep: project (later
  `GSTNMovement`) → `GSTNSurfaceCore`, private.
- Tests: `GSTNMovement.Wall.Filter.{ForbiddenSurfaceRejected, UnknownPasses, ClimbOnlyRejectsSlide, CornerOntoForbiddenRejected}`.
  Scenario `scenario_surface_nowallslide` (a static wall with a component lacking `WallSlide` is never grabbed on any machine; a
  climb-only wall accepts the climb mode only).
- Acceptance: all M3 wall scenarios unchanged with empty queries; the owner's OK recorded in this plan's §6.
- Spec: TS §5.2.

### T6 `GSTNPlatformParts` bridge: part state → shape mask — size M

- Files (`Plugins/GSTNPlatformParts/Source/GSTNPlatformParts/`): `GSTN_PlatformPartsComponent.h/.cpp` (on the fighter: at registration
  resolves `PartName` → part index against the entity's `UGSTN_PartSetDefinition`; each frame at the platform's defined point reads the
  **confirmed** `BrokenMask` (host: end of F; clients: the same source presentation uses, combat-parts §6.5) and calls `SetShapeMask`;
  break ⇒ `Breaking` for `DropGraceFrames` then off; `Combat.Event.Part.Restored` ⇒ on). `IsDataValid`: unknown part names are errors here.
  `.uplugin` depends on `GSTNPlatform` and `GSTNParts` (bridge, MT §3.7 rule 3).
- Tags: none.
- Tests `GSTNPlatformParts.{MaskFollowsBrokenMask, RestoreReenables, ChildrenDropWithParent, GraceBeforeOff}`. Scenario
  `scenario_surface_part_break` (c1 slides on the flank, c2 breaks that part: telegraph visible, c1 falls with the base velocity, ≤ 1
  correction; the host's healer dummy restores it (P7b), c1 can grab it again).
- Acceptance: removing the bridge leaves parts and platforms building and passing (canary); the parts simulation is untouched.
- Spec: TS §4.4.

### T7 Lifecycle under a player, lag compensation, full scenario set — size M

- Files: platform state transitions of TS §6 wired to the enemy's combat state (launched, hitstop, death, grab) through the existing
  platform rules (PL §4.6) — any missing rule is added in `GSTNPlatform`, never in combat; H10's history stores the composed transform
  of based players (verify, fix if H10 recomputed from the base).
- Tests `GSTNPlatform.Lifecycle.{LaunchedDropsAfterGrace, HitstopFreezesPose, DeathDropsBeforeRelease}`. Scenarios:
  `scenario_surface_enemy_launched` (the enemy carrying a sliding player is launched: one correction, player keeps base velocity),
  `scenario_surface_enemy_hitstop` (pose frozen, player keeps sliding), `scenario_surface_lagcomp` (c2 hits c1 who is sliding on a
  moving enemy at 300 cm/s: hit confirmed with the same `FGSTN_LagCompTime`, no miss from base recomposition).
- Acceptance: TS §7 correction budget met in every scenario; host zero reconciles.
- Spec: TS §6, §7, PL §4.6–4.7.

### T8 Platform minions' shapes (`GSTNPlatformMinion`, late) — size S/M

- Files: proxy actor state +1 byte `EnabledShapes`; `IGSTN_MinionPublishObserver` payload gains the slot's part mask; the bridge computes
  the shape mask from it (same function as T6, shared through `GSTNPlatformCore`).
- Tests `GSTNPlatformMinion.Shapes.{MaskReplicated, GraceOnProxy}`; scenario `scenario_platform_minion_surface` (one platform minion with
  a slideable flank, 8 alive cap unchanged).
- Acceptance: PL §4.9 for minions; byte budget noted against `HostUploadBudgetKBps`.
- Spec: TS §4.5. Needs M8 complete and P16.

### T9 Content, validators, harness, docs — size M

- Content (`Plugins/GSTNSurface/Content`, `Plugins/GSTNPlatform/Content`, test map of H13): a non-slideable wall, a climb-only wall, the
  scripted test fighter with flank, top and trampoline back; Blueprint examples of a provider and of a shape set.
- Validators (M5 E10 mechanism, `GSTNPlatformEditor` or inside `GSTNPlatform` editor module): shapes vs wall angle range, shape far from
  mesh (warning), part names, Blueprint runtime writes to behaviour fields.
- Harness: `Tools/GASNPPTest/surface_scenarios.py` (imports `gasnpp_test`) with all `scenario_surface_*`; docs commit (spec updates,
  modularity §3.6/§4.2 rows, wall-movement.md "How to extend" entry, READMEs, combat.md §17 spike rows).
- Acceptance: the whole set clean at 60/15 ms with and without 2 % loss; canaries for `GSTNSurface`, `GSTNPlatform`, `GSTNPlatformParts`.

## 4. Spikes first and stop-and-ask conditions

| Spike | Stop and ask when |
|---|---|
| TS-A | sliding on a kinematic base reconciles by itself (> 0 unexplained per 30 s at 150 cm/s): base riding needs approach D before anything else |
| TS-B | a disabled base is kept > 1 frame or the capsule penetrates on loss; a mode subclass cannot fix it |
| TS-C | the launch landing point differs > 10 cm between host and client at 300 cm/s with the base velocity sample |
| P1 (platform) | if P1 already failed for standing, T2/T3 wait for approach D |
| T3 | the platform plugin's pose point cannot be shared by shapes and the hit snapshot (order of post-fixed-tick observers) |

## 5. Assumptions for unanswered owner questions (OQ TS.1–TS.8)

| Question | Assumption used here | Affects | Cost to change |
|---|---|---|---|
| TS.1 separate `GSTNSurface` plugin owning `Surface.*` | **Answered 2026-10-22 22:15: yes**; H11/H11b consume it, so T1 lands before H11 | T1, H11/H11b edits | done |
| TS.2 wall-core filter extension point | **Answered 2026-10-22 22:14: yes**, some surfaces are wall-slideable and others not | T5 | done |
| TS.3 which enemies | the same set as platforms (fighters + ≤ 8 platform minions), swarm never | T3, T8 | M (minions need the proxy anyway) |
| TS.4 enemy slows while a player slides | **Answered 2026-10-22 21:58**: depends on the part and the enemy. `FGSTN_OccupantResponse` per platform shape (enemy-level default on the definition, BP `OnOccupantChanged`); T3 adds the struct, T7 tests one slowing and one full-speed enemy | T3, T7 | done (data) |
| TS.5 part breaks under a player | `DropGraceFrames` telegraph then fall with base velocity; no riding the chunk | T6 | S |
| TS.6 trampoline parts: direction follows the posed part, surface velocity added; tumbling victims bounce; hitting the part never launches | yes to all | T4 | S (flags) |
| TS.7 who uses the surfaces | players and fighters; minions only on static trampolines | T4 | M (minion launch path) |
| TS.8 behaviours shipped | `WallSlide`, `WallClimb`, `Launch`, `Stand`; `Slippery`/`OneWay` reserved | T1 | S |

## 6. Blueprint / shared-asset edits that need the owner's OK before they happen

- `SandboxCharacter_Mover` (shared Blueprint): T2 adds `UGSTN_SurfaceWallEntryTransition` to Falling's `Transitions` (or replaces the
  existing entry transition); T5 sets the modes' `RequiredBehaviours` queries. Announced before each edit, never from two sessions.
- `Config/DefaultEngine.ini` collision: T3 adds the `GSTN_EnemyPlatform` object channel if the platform plugin has not already
  (plugin ini preferred, MT §3.7 rule 1; the project ini only if the engine requires it for channels).
- `.uproject`: enabling `GSTNSurface` (T1), `GSTNPlatform` (T3), `GSTNPlatformParts` (T6), `GSTNPlatformMinion` (T8).
- Wall-movement core edit (T5): one struct field and one check in `IsHitOnValidWall`/`SweepForWall`; the owner's OK is TS.2 (given 2026-10-22 22:14).
- H11/H11b plan text: the `IGSTN_ImpactSurface`/`IGSTN_LaunchSurface` → `IGSTN_SurfaceProvider` move (done in the plans on 2026-10-22, code not started).

## 7. Risks and parallelisation

- Risks are TS §10; the two that gate the order are TS-A (base riding on kinematic colliders) and the absence of a platform plan: T3
  carries the platform minimum so this feature does not wait for M8.
- Threads: (1) T0 + T2 on the PC with the wall code; (2) T1 (pure, any agent) then T3; (3) T4 follows the M4 thread after H11b; (4) T6/T7
  follow the M4b thread after P7b and H10; (5) T5 right after T1 (TS.2 approved); (6) T8 after M8/P16; (7) T9 last.
- Only one thread at a time touches `GSTNCombatNP`, the wall modes and the harness on the PC.
