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
          └ UGSTN_WallSlideMovementMode        gravity drives the vertical axis (ComputeVerticalSpeed)
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

`UGSTN_WallEntryTransition` is added to the Transitions of the Falling mode. It decides whether to grab (probe distance, surface angle range, vertical policy `EGSTN_WallEntryVertical`, same-wall policy `EGSTN_SameWallPolicy`). How momentum is kept is configured on the **target mode** (`EntryMomentum`). Wall memory is read from `FGSTN_WallMovementSyncState`, which every wall mode updates; it resets when the character passes a `MemoryResetModes` mode, when a `MemoryResetTags` tag is present in the **simulated sync state**, via `UGSTN_WallMovementLibrary::ResetWallMemory`, or by grabbing a different wall.
Debug drawing: `Private/WallMovement/GSTN_WallMovementDebug.{h,cpp}` (private, module-internal).
