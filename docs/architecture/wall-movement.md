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
             └ UGSTN_WallSlideV2MovementMode       control model = UGSTN_WallControlUtils::StepSlideVelocity; registered as "WallSlidingV2" next to the legacy slide
UBaseMovementModeTransition
 └ UGSTN_WallEntryTransition                   Falling -> wall mode (decides WHETHER to grab)
    └ UGSTN_AbilityWallEntryTransition         (GAS/) same, but only if the wall ability can activate
UBlueprintFunctionLibrary
 ├ UGSTN_WallMovementUtils                     stateless math: movement plane, per-axis speed, entry/jump velocity, velocity transport
 ├ UGSTN_WallQueryUtils                        stateless queries: wall validation, multi-probe detection, corner checks
 └ UGSTN_WallMovementLibrary                   Blueprint-facing helpers (e.g. ResetWallMemory)
```

Tree verified against the headers on 2026-10-17; re-check with `get_class_hierarchy` if the classes were refactored since.

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

## Prediction rules for wall state

Nothing the simulation reads may live outside the sync state or a rollback-aware store. The Mover `SimBlackboard` (`GetSimBlackboard_Mutable`) is **not rewound on rollback**: do not keep wall data there.

- **Wall memory and its manual reset** are sync state (`LastWall*`, `MemoryResetTimeMs`). A manual reset is the instant movement effect `FGSTN_ResetWallMemoryEffect` (`GSTN_WallMemoryEffects.h`), applied by Mover at the start of a step and stored in `MemoryResetTimeMs`. `UGSTN_WallMovementLibrary::ResetWallMemory` just queues it. Queue it from the same simulated event on server and clients (an ability), otherwise the server state wins on the next correction.
- **Entry handoff (`EntryInfo`) and the ridden dynamic wall (`BaseInfo`)** are entries of Mover's rollback blackboard (`SimContext.Blackboard`, `GSTNWallBlackboard::EnsureEntries`): 64 slots, persistent, `InvalidatedOnRollback`. The entry cannot be unset, so "none" is an empty struct (zero normal, invalid `Base`). Modes create the entries in `OnRegistered`, the entry transition on first use.
- **`UGSTN_WallEntryTransition::Evaluate` is read-only.** Mover calls it more than once per tick and again when re-simulating; the only write of the transition is in `Trigger` (entry handoff). The decision depends on the start sync state, `CommonBlackboard::LastModeChangeRecord` (rollback blackboard) and tags.
- `MemoryResetTags` resets only **while** the tag is present (it is no longer latched into a time). Use the reset effect for a permanent forget.

## Tests

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
- Register the mode on the Mover component (Blueprint `SandboxCharacter_Mover`, `Modes` map) and add transitions to the source mode's `Transitions`.

## Entry rules

`UGSTN_WallEntryTransition` is added to the Transitions of the Falling mode. It decides whether to grab (probe distance, surface angle range, vertical policy `EGSTN_WallEntryVertical`, same-wall policy `EGSTN_SameWallPolicy`). How momentum is kept is configured on the **target mode** (`EntryMomentum`). Wall memory is read from `FGSTN_WallMovementSyncState`, which every wall mode updates; it resets when the character passes a `MemoryResetModes` mode, while a `MemoryResetTags` tag is present (Mover tags of the simulated sync state, plus owner tags), via `UGSTN_WallMovementLibrary::ResetWallMemory` (`FGSTN_ResetWallMemoryEffect`, stored in the sync state), or by grabbing a different wall. See "Prediction rules for wall state".
Debug drawing: `Private/WallMovement/GSTN_WallMovementDebug.{h,cpp}` (private, module-internal).
