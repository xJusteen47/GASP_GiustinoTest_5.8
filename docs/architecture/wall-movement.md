# Wall movement (Mover)

Source: `Source/GASP_Giustino2/{Public,Private}/WallMovement/` and `GSTN_MovementModeBase.h`.
Built on the **Mover** plugin (`Plugins/EngineOverrides/Mover`, see its README for Mover basics): modes are `UBaseMovementMode` subclasses registered on the Mover component; transitions are `UBaseMovementModeTransition`.

## Class map

```
UBaseMovementMode
 └ UGSTN_MovementModeBase                      (empty project base)
    └ UGSTN_WallMovementModeBase  (abstract)   movement core, see below
       └ UGSTN_ControlledWallMovementMode      stick mapping on the wall (MapMoveIntentToWall), detach, wall jump (CheckForWallExit)
          ├ UGSTN_SkiddingWallMovementMode     skid against horizontal movement (ComputeHorizontalSpeed)
          │  └ UGSTN_SimpleWallMovementMode    climbing: stick drives the vertical axis (GetVerticalMovement, ComputeTargetUp)
          ├ UGSTN_WallSlideMovementMode        gravity drives the vertical axis (ComputeVerticalSpeed)
          └ UGSTN_WallTraverseModeBase (abstract)  v2 family, lean pipeline (wall-slide-v2.md 6): capsule never rotates, proxy plane, one move sweep + one contact/adhesion sweep
             ├ UGSTN_WallSlideV2MovementMode       slide v2: control model = UGSTN_WallControlUtils::StepSlideVelocity; registered as "WallSlidingV2" next to the legacy slide
             └ UGSTN_WallClimbV2MovementMode       climb v2: stick drives both axes (StepClimbVelocity); registered as "WallClimbingV2"; capsule never tilts
UBaseMovementModeTransition
 ├ UGSTN_WallEntryTransition                   Falling -> wall mode (decides WHETHER to grab; the mode is TargetModeName)
 │  └ UGSTN_AbilityWallEntryTransition         (GAS/) same, but only if the wall ability can activate
 └ UGSTN_WallSurfaceModeTransition             Falling -> the wall mode the surface asks for (slide / climb by Surface.Behaviour.*, FGSTN_WallSurfaceModeList)
    └ UGSTN_AbilityWallSurfaceModeTransition   (GAS/) same behind the wall ability check
UBlueprintFunctionLibrary
 ├ UGSTN_WallMovementUtils                     stateless math: movement plane, per-axis speed, entry/jump velocity, velocity transport
 ├ UGSTN_WallQueryUtils                        stateless queries: wall validation, multi-probe detection, corner checks
 ├ UGSTN_WallEdgeUtils                         v2: look-ahead probes (ProbeAhead) and the pure edge classification / speed cap (FGSTN_WallEdgeSettings)
 ├ UGSTN_WallControlUtils                      v2: slide / climb velocity steps (FGSTN_WallControlSettings, FGSTN_WallClimbControlSettings)
 ├ UGSTN_WallSurfaceModeUtils                  v2: PickMode / StepVote / ChooseModeForHit (slide vs climb, hysteresis)
 └ UGSTN_WallMovementLibrary                   Blueprint-facing helpers (e.g. ResetWallMemory)
```

Tree verified against the headers on 2026-10-17 (v2 family added 2026-10-25, design and status in [wall-slide-v2.md](wall-slide-v2.md); A/B tooling: `Tools/GASNPPTest/wall_ab.py`, map `L_WallSlideAB`, `gstn.Wall.Metrics` CSV); re-check with `get_class_hierarchy` if the classes were refactored since.

## Data

`GSTN_WallMovementTypes.h` holds all tuning structs (`FGSTN_WallHorizontalMovement`, `...VerticalMovement`, `...Skid`, `...EntryMomentum`, `...Detach`, `...WallJump`, `...WallCheckSettings`, `...WallMovementPlane`), the optional input struct `FGSTN_WallInputs`, and the sync state `FGSTN_WallMovementSyncState` (wall memory: last wall normal, height, time).
Both structs derive from `FMoverDataStructBase` and carry `TStructOpsTypeTraits` for net serialization. A new field that affects simulation must be added to **serialization, equality/`ShouldReconcile` and interpolation/merge** of the struct, or it will desync.

`FGSTN_WallMovementSyncState` fields and how `ShouldReconcile` compares them (all of them, constants in `GSTN_WallMovementTypes.cpp`):

| Field | Wire | Reconcile when |
|---|---|---|
| `bHasWall`, `bIsSkidding`, `bHasWallMemory` | 3 bits | differs |
| `WallNormal` | fixed vector 16 bit/axis | on a wall and angle > ~5 deg |
| `WallContactPoint` | packed vector | not compared (pivot only, follows from position) |
| `TimeWithoutWall`, `PullAwayTime` | float | differs by > 0.02 s |
| `LastWallNormal`, `LastWallHeight`, `LastWallTimeMs` | only with `bHasWallMemory` | with memory: angle > ~5 deg / > 2 cm / > 1 ms |
| `MemoryResetTimeMs` | 1 bit flag, +8 bytes (double) only when set (>= 0) | set vs unset, or > 1 ms apart |
| `bLastWallOnMovingBase`, `LastWallBaseHeight` (T2) | 1 bit flag (only with memory), +4 bytes (float) when set | with memory: flag differs, or > 2 cm |

## Prediction rules for wall state

Nothing the simulation reads may live outside the sync state or a rollback-aware store. The Mover `SimBlackboard` (`GetSimBlackboard_Mutable`) is **not rewound on rollback**: do not keep wall data there.

- **Wall memory and its manual reset** are sync state (`LastWall*`, `MemoryResetTimeMs`). A manual reset is the instant movement effect `FGSTN_ResetWallMemoryEffect` (`GSTN_WallMemoryEffects.h`), applied by Mover at the start of a step and stored in `MemoryResetTimeMs`. `UGSTN_WallMovementLibrary::ResetWallMemory` just queues it. Queue it from the same simulated event on server and clients (an ability), otherwise the server state wins on the next correction.
- **Entry handoff (`EntryInfo`) and the ridden dynamic wall (`BaseInfo`)** are entries of Mover's rollback blackboard (`SimContext.Blackboard`, `GSTNWallBlackboard::EnsureEntries`): 64 slots, persistent, `InvalidatedOnRollback`. The entry cannot be unset, so "none" is an empty struct (zero normal, invalid `Base`). Modes create the entries in `OnRegistered`, the entry transition on first use. A rewound entry holds this machine's prediction of the frame before, not the authority's: the v2 modes (`bRideFromSyncedLocation`) take only the base and its previous pose from `BaseInfo` and the character's place on it from the start sync state, so a correction while riding is kept (wall-slide-v2.md 6.14).
- **`UGSTN_WallEntryTransition::Evaluate` is read-only.** Mover calls it more than once per tick and again when re-simulating; the only write of the transition is in `Trigger` (entry handoff). The decision depends on the start sync state, `CommonBlackboard::LastModeChangeRecord` (rollback blackboard) and tags.
- `MemoryResetTags` resets only **while** the tag is present (it is no longer latched into a time). Use the reset effect for a permanent forget.
- **`LastModeChangeRecord` can be stale after a correction** (traversal T2). It is `InvalidatedOnRollback` and nothing rewrites it: a rollback to a frame after the authority's
  take-off but before this machine's own erases the record of the take-off, and the re-simulation starts already in the air. The "went through `MemoryResetModes`" reset
  then differs between server and client and the next grab of the same wall reconciles. `bInferModeChangeLostToRollback` (on in the surface-driven transitions) reads a
  record whose mode is not the start state's mode as "the change out of the record's mode was lost": a reset mode entered after the wall still resets the memory. Two lost
  changes (land and take off inside one rollback window) are not recovered.

## Tests

`Private/WallMovement/Tests/GSTN_WallReentryTests.cpp`: `GSTN.Wall.V2Surface.*`, the re-entry rules (`UGSTN_WallReentryUtils`, pure) with and without the T2 switches, the moving-wall memory and its wire format.
`Private/WallMovement/Tests/GSTN_WallMovementSyncStateTests.cpp`: automation tests `GSTN.Wall.SyncState.*` for `ShouldReconcile` (every field, in and out of tolerance) and the `NetSerialize` round trip. Rollback behavior of the blackboard entries and of the reset effect needs the network harness (testing.md): `scenario_jump_wall`, `scenario_server_correction` on the wall, `force_reconcile_on_mode`.

## The four quantities (do not mix them)

1. **Raw probe normal** ("Surface Detection"): what the probes measure this substep. Noisy.
2. **Filtered normal** ("Normal Filter"): kept in the sync state. **It is the movement plane**: velocity, input mapping, adhesion and probe directions use it.
3. **Capsule rotation** ("Orientation"): visual/collision pose. Chases the filtered normal with its own smoothing and never feeds back into the plane.
4. **Position against the wall** ("Adhesion"): fixed gap, independent of the others.

## Tick pipeline

`GenerateMove`: intent on the wall (`ComputeWallIntent`) -> velocity in the plane per axis (`GenerateWallMove`).

`SimulationTick`: ride the movement base, face the wall, voluntary exits (`CheckForWallExit`), then per substep (`MoveAlongWall`):
1. sweep along the wall tangent (blocking hits slide; concave candidates; moving down into a floor -> `FloorModeName`)
2. multi-sphere probes (center/top/bottom + lookahead), blend normals; discard edges leading onto roofs/floors
3. no wall -> wrap probe around convex edge; none for `NoWallGraceTime` -> `NoWallModeName`
4. filter normal: soft deadzone, rate limit; frozen at rest
5. target orientation: parallel-transport the frame onto the filtered normal + upright bias
6. smooth toward it and apply rotation around the contact point, validated by an overlap test
7. adhere to the wall (instant push-out, rate-limited pull-in, penetration resolution)
8. parallel-transport the velocity onto the new plane (speed preserved around corners)

## How to extend

- **New wall behaviour**: subclass `UGSTN_ControlledWallMovementMode` (or the base) and override only the extension points listed in the class map. Do not add feature branches to the core.
- **New math or query**: add a static function to `*Utils` (stateless, `BlueprintPure`), not to the mode.
- **New tunable group**: add a `USTRUCT` in `GSTN_WallMovementTypes.h` and a `Category = "Wall|<Section>"` property on the mode.
- Gravity can be any direction and walls can move/rotate: always use the gravity-up from the movement plane / Mover, never `FVector::UpVector`. Check `bUseMovementBases` paths when changing position or velocity.
- **Held from outside (hitstop)**: `IsMovementLocked(const FMoverTickStartData&)` is a protected virtual. Default: the Mover sync state of the tick start carries `Combat.Mover.Frozen` (a combat freeze layered move; looked up by tag name, so this module does not depend on the combat plugin). A locked tick skips `CheckForWallExit` (wall jump, button detach, pull-away) and leaves `TimeWithoutWall` / `PullAwayTime` as they were; an unlocked tick is unchanged. Override it for another kind of lock.
- Register the mode on the Mover component (Blueprint `SandboxCharacter_Mover`, `Modes` map) and add transitions to the source mode's `Transitions`.

## Entry rules

`UGSTN_WallEntryTransition` is added to the Transitions of the Falling mode. It decides whether to grab (probe distance, surface angle range, vertical policy `EGSTN_WallEntryVertical`, same-wall policy `EGSTN_SameWallPolicy`). How momentum is kept is configured on the **target mode** (`EntryMomentum`). Wall memory is read from `FGSTN_WallMovementSyncState`, which every wall mode updates; it resets when the character passes a `MemoryResetModes` mode, while a `MemoryResetTags` tag is present (Mover tags of the simulated sync state, plus owner tags), via `UGSTN_WallMovementLibrary::ResetWallMemory` (`FGSTN_ResetWallMemoryEffect`, stored in the sync state), or by grabbing a different wall. See "Prediction rules for wall state".
The re-entry rules themselves (reset, same wall, cooldown, deny, height cap) are the pure `UGSTN_WallReentryUtils::Judge` (`MoveLibrary/GSTN_WallReentryUtils.h`). Two switches,
off in the plain transitions and on in the surface-driven ones (traversal T2, wall-slide-v2.md 6.15): `bInferModeChangeLostToRollback` (above) and `bBaseRelativeHeightCap`:
on a moving wall (a Movable component) the height cap compares heights in the wall's own frame, using the wall origin's height that modes with `bRecordWallBaseInMemory`
(the v2 modes) store in the memory; a memory without it and a moving wall give cooldown only; static walls keep world heights.
Debug drawing: `Private/WallMovement/GSTN_WallMovementDebug.{h,cpp}` (private, module-internal).
