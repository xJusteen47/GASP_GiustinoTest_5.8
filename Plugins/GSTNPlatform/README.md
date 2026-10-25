# GSTNPlatform

Enemies as solid platforms and traversal surfaces: kinematic, unattached colliders (shapes) authored in an enemy's frame, posed at
one defined point per simulation step, each offering a traversal surface (stand, wall slide, wall climb, launch) through
GSTNSurface. A player's Mover stands on a shape and the wall modes slide on it like on any moving base; nothing in the movement or
combat code knows it is an enemy.

Status: **experimental (0.1)**, `EnabledByDefault: false` (enabled in this branch's `.uproject`). Checkpoint T3 of
[traversal-surfaces-plan.md](../../docs/architecture/plans/traversal-surfaces-plan.md): the platform minimum for **fighters**
(many-enemies-replication.md §4.2–4.5) plus shapes with behaviours (traversal-surfaces.md §4.1, §4.2, §4.4 input). No
platform-minion proxy and no minion seams (combat-m8-minions.md §4d.5 stays for `GSTNPlatformMinion`); no part bridge
(`GSTNPlatformParts`, T6); no combat-state wiring (T7). Design:
[many-enemies-replication.md](../../docs/architecture/many-enemies-replication.md) §4,
[traversal-surfaces.md](../../docs/architecture/traversal-surfaces.md) §4. Deviations from the specs:
[traversal-doc-deltas.md](../../docs/notes/traversal-doc-deltas.md) (T3).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNPlatformCore` | exists (Runtime) | Pure data and math, no world, no Mover: `FGSTN_PlatformSpec`, `FGSTN_PlatformShape` (geometry, `FGSTN_SurfaceBehaviour`, `PartName`, activation and occupant overrides), `FGSTN_OccupantResponse`, `EGSTN_PlatformShapeState`, `EGSTN_PlatformActionState`, `FGSTN_PlatformShapeStates` (per-shape timelines by frame), `GSTNPlatformCollision` (behaviour → collision rules), `FGSTN_PoseHistory`, `FGSTN_PlatformPoseContext`, `FGSTN_PlatformScriptedPath`, validation (`CollectFindings`), the `Platform.*` tags. Covered by `GSTNPlatform.Core.*`. |
| `GSTNPlatformRuntime` | exists (Runtime) | `UGSTN_PlatformSettings`, `UGSTN_PlatformDefinition` (data asset), `UGSTN_EnemyPlatformComponent`, the colliders `UGSTN_Platform{Box,Sphere,Capsule}Collider` (surface providers), `UGSTN_PlatformSubsystem` (the pose point), `UGSTN_ScriptedPlatformComponent` (enemy on rails), `AGSTN_PlatformTestFighter` (test content), console commands. Covered by `GSTNPlatform.Runtime.*` and `GSTNPlatform.Shapes.*`. |

## Dependencies

- Plugins: `GSTNSurface`, `Mover` (plan T3 acceptance: builds with `GSTNSurfaceCore` and Mover only). No GAS and no Network
  Prediction code of its own; Mover brings Network Prediction with it.
- `GSTNPlatformCore`: Core, CoreUObject, Engine, GameplayTags, GSTNSurfaceCore.
- `GSTNPlatformRuntime`: the above plus DeveloperSettings, PhysicsCore, Mover, GSTNPlatformCore; private NetCore.
- Never `GASP_Giustino2`, `GSTNCombat*`, `GSTNParts`, `GSTNMinion`. The combat lag-compensation time of modularity §3.5 is not
  needed before T7.

## How it works

**Colliders.** One collider per shape, created when the component registers in a game world, named `Surface<ShapeId>`, Movable,
no physics simulation, query-only collision, **unattached** (absolute transform), net addressable (Mover's sync state names the
movement base by object reference, so the client resolves the server's `Surface1` to its own). The owner must be replicated or
stably named.

**Pose point.** `UGSTN_PlatformSubsystem::ApplyPosePoint` poses every platform once per simulation frame. Driver: the start of every
Mover simulation tick of the world (`UMoverComponent::OnPreSimulationTick`). The first tick of frame F poses all platforms at the end
of F-1 (`Frame = ServerFrame - 1`, `TimeMs = BaseSimTimeMs`); the other pawns of the frame find it done; a re-simulation goes back
in frames and poses again. `ComputeOwnerPose` (BlueprintNativeEvent, simulation contract: pure) gives the pose: the owner's
transform by default (approach B), a function of the frame time for `UGSTN_ScriptedPlatformComponent` (approach C, exact on every
machine). The collider is teleported (sweeps see it at once; no physics velocity is invented). When no simulation ran for
`PresentationFallbackSeconds`, the component poses itself from its game tick (presentation only).

```
OnPreSimulationTick(TimeStep)            // every Mover of the world, inside the fixed step, before its movement
  ctx = { Frame = TimeStep.ServerFrame - 1, TimeMs = TimeStep.BaseSimTimeMs, StepMs }
  if ctx == last applied: return         // another pawn of the same frame
  for platform in platforms:
    pose = platform.ComputeOwnerPose(ctx)          // scripted: Path.Evaluate(Origin, ctx.TimeMs / 1000)
    history.Record(ctx.Frame, pose)
    for shape: collider.collision = solid(state(shape, ctx.Frame)); collider.Teleport(pose * shape.Local)
```

**Shape states** (traversal-surfaces.md §4.4). Two inputs: `SetShapeMask(uint8 EnabledShapes, DisableAs)` (the T6 parts bridge
feeds the confirmed broken mask) and `SetActionState(EGSTN_PlatformActionState)` (T7 maps the fighter's combat state;
`ActiveActionStates` masks per spec and per shape). A change is stamped with the next frame the pose point simulates and kept as a
timeline, so a frame's collision is a function of the frame and a re-simulated frame finds what it had the first time:

```
Request(desired, DisableAs, grace[], F):          // F = latest posed frame + 1
  for each shape whose target changes:
    ChangeFrame = F; SolidBefore = solid(state(F - 1))
    on:  Active from F (no telegraph: gaining a surface never hurts)
    off: Breaking (still solid) for grace frames, then Inactive   // DisableAs = Inactive: at once; also cuts a running grace
state(shape, frame) = frame < ChangeFrame ? (SolidBefore ? Active : Inactive)
                    : on ? Active : frame < OffFrame ? Breaking : Inactive
```

`GetShapeState` / `GetShapeStateTag` (`Platform.State.*`) read the state for presentation. `Braced` is reserved (mount, T7).

**Collision from behaviour** (traversal-surfaces.md §4.1). Object channel `GSTN_EnemyPlatform`; responses are Ignore everywhere,
then Block on the channels of every rule whose tag query matches the shape's `Surface.Behaviour.*`:

| Rule (shipped, `GSTNPlatformCollision::MakeDefaultRules`) | Blocks |
|---|---|
| Stand, WallSlide, WallClimb or Launch | `MoverChannels` (default Pawn) |
| WallSlide or WallClimb | `WallProbeChannels` (default Pawn: this project's wall modes probe with the capsule's object type) |
| Stand or Launch | `StandAndLaunchExtraChannels` (default none; the depth marker's trace channel goes here) |

A shape with no traversal behaviour ignores characters (an impact-only surface). Because the wall probes share the capsule's
channel, a Stand-only top is kept from being grabbed as a wall by the surface the wall entry resolves (no `WallSlide`), not by
collision. A project replaces the table with `bUseCustomCollisionRules`.

**Surface provider.** Every collider implements `IGSTN_SurfaceProvider`: `GetSurfaceBehaviour` returns the shape's static
`Behaviour`, `GetSurfaceVelocity` returns `GetSurfaceVelocityAt(ShapeId, ContactPoint)`, the rigid-frame velocity between the end of
the frame before and the end of the frame the collider holds, from the pose history (zero for an inactive shape):

```
v(point) = (Loc[F] - Loc[F-1]) / dt + angularVelocity(Rot[F] * Rot[F-1]^-1, dt) x (point - Loc[F])
```

**How the wall slide v2 rides it** (wall-slide-v2.md §6.10, §6.14, unchanged code): the surface-driven Falling transition resolves
the flank through the provider (`WallSlide`) and enters `WallSlidingV2`; the Movable collider is the mode's dynamic base; each tick
the mode rides from the synced location seen in the base pose of the previous frame (blackboard record) to the base pose now. Both
poses are the pose point's, a function of the frame on every machine for a scripted enemy, so riding does not reconcile. Standing
uses Mover's own based movement (the base is in Mover's sync state).

## Using it

1. Enable the plugin. Add **GSTN Enemy Platform** (or **GSTN Scripted Platform** for an enemy on rails) to the fighter Blueprint.
2. Fill a **GSTN Platform Definition** data asset (Shapes: type, extent, local location and rotation, Behaviour, optional PartName,
   overrides) and set it as `Definition`, or fill the inline `Spec`.
3. Do **not** put a GSTN Surface Behaviour component on the same actor: it wins over every shape (resolution step 1; validation warns).
4. Keep the enemy replicated (or placed in the map). Run validation on the asset: shapes on a bone, Stand on a sphere or capsule,
   repeated ids are errors; a wall shape with no face in the wall modes' angle range and part names without the bridge are warnings.

Blueprint: `ComputeOwnerPose` can be overridden (pure: context and replicated state only), `OnShapeBreakRequested` receives
`IGSTN_SurfaceProvider::BreakSurface`; `SetShapeMask`, `SetActionState`, `GetShapeState(Tag)`, `GetShapeBehaviour`,
`GetSurfaceVelocityAt`, `GetOccupantResponse` are callable.

## Collision channel

`Config/Engine.ini` declares the object channel `GSTN_EnemyPlatform` (slot `GameTraceChannel4`, default response Block). The engine
merges a plugin's `Config/{TYPE}.ini` (here `Engine.ini`) into that branch when the plugin is mounted; a plugin `DefaultEngine.ini`
is **not** merged. The code finds the channel by name (`UGSTN_PlatformSettings::PlatformObjectChannelName`), so a project that needs
the slot moves the line to its own `DefaultEngine.ini`.

## Settings

Project Settings > Plugins > GSTN Platform (`UGSTN_PlatformSettings`, `GSTNPlatform.ini`): channel name, `MoverChannels`,
`WallProbeChannels`, `StandAndLaunchExtraChannels`, `bUseCustomCollisionRules` / `CustomCollisionRules`,
`PresentationFallbackSeconds`, the validation angles (`MinWallSurfaceAngle` 45, `MaxWallSurfaceAngle` 120, `MaxStandAngle` 44).
Every machine of a match must run the same values.

## Console commands (not in Shipping)

- `gstn.Platform.SpawnTestFighter X Y Z [Yaw]`: spawns `AGSTN_PlatformTestFighter` on the server of the world (it replicates):
  WallSlide flank (shape 0, -X face 175 cm from its origin, 800 cm wide, z 50..550), Stand top (shape 1, z 580), on rails along its
  local Y, ±300 cm at 300 cm/s. No mesh: use `gstn.Platform.Draw 1`.
- `gstn.Platform.Draw 1`: draws every platform shape (green Active, orange Breaking, grey Inactive).
- `gstn.Platform.SetMask Mask [Inactive]`: shape mask input on every platform of every game world.

## Gameplay tags

| Tags | Source | Declared in |
|---|---|---|
| `Platform`, `Platform.State.{Active,Braced,Breaking,Inactive}`, `Platform.Event.{Mounted,Dropped}` | native | `GSTNPlatformCore/Public/GSTN_PlatformTags.h`, namespace `GSTNPlatformTags` |

`Braced`, `Mounted`, `Dropped` are reserved (occupancy is T7). No tag is ever in a sync state. No tag ini, so the module registers no
tag search path.

## Tests

`Automation RunTests GSTNPlatform` (17): `Core.{TagsDeclaredOnce, ShapeComponentNames, SpecShapeIds, ShapeComposeIgnoresScale,
PoseHistory.SampleExactFrame, PoseHistory.VelocityFromTwoFrames}`, `Runtime.{Smoke, ChannelResolvedByName,
CollidersUnattachedNamedAddressable, PosePointFollowsOwner}`, `Shapes.{MaskDisablesShapes, CollisionDerivedFromBehaviour,
PartNameWarningWithoutParts, BreakingGraceThenOff, PoseAppliedOncePerStep, ValidatorRejectsBoneAndCapsule,
ScriptedPathIsAFunctionOfTime}`.

PIE network scenario `scenario_surface_wallslide_enemy`: `python Tools/GASNPPTest/surface_scenarios.py` (headless editor, listen
host + 1 client, map `L_WallSlideAB`, the fighter spawned by the driver; `--loss 2`, `--subjects client|host`, `--repeats N`). Table in
`Saved/Metrics/surface-scenarios-summary.md`.

## Known limits (v0.1)

- **Leaving a shape gives no base velocity yet.** The colliders are teleported, so `GetPhysicsLinearVelocityAtPoint` is zero:
  the wall modes' `bAddBaseVelocityOnExit` and the walking mode's leave-base velocity add nothing. The velocity exists
  (`GetSurfaceVelocityAt`, the provider's `GetSurfaceVelocity`); T4 (launch) and a mode subclass (T7) read it.
- The pose point runs at Mover's tick: simulation that ticks before Mover in a frame (combat, the ability system) sees the platforms
  one frame older, and on the first frame of a re-simulation at the newest pose. Nothing there sweeps against platforms today; the
  NP post-fixed-tick hook (combat-m8-minions.md spike 2) would call `ApplyPosePoint` instead.
- Approach B (a fighter moved by its own simulation) gives the client the interpolated pose: mount and loss cost one correction each
  (traversal-surfaces.md §7). Only approach C (scripted) is exact on clients.
- Convex shapes are not supported (Stand needs a Box). Occupancy (`OnOccupantChanged`, the AI limiter, the brace) is data only.
- The wall entry's same-wall policy compares world heights, meaningless on a moving base: re-grabbing the same shape can be refused
  on one machine and not on the other (the scenario resets the wall memory before each grab). T2's `UGSTN_SurfaceWallEntryTransition`
  fixes it.

## Changelog

- 0.1 (2026-10-25): skeleton for fighters (spec, component, channel, pose point, pose history), then shapes with behaviours (plan T3).
