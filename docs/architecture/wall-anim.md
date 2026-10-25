# Wall slide animation states

Source: `Source/GASP_Giustino2/{Public,Private}/WallMovement/Animation/`, tests `Private/WallMovement/Tests/GSTN_WallAnimTests.cpp` (`GSTN.Wall.Anim.*`).
General pattern (layers decoupled from the main graph, skin variants): [anim-layers.md](anim-layers.md).

## What the layer gets

`UGSTN_WallAnimInstance::WallAnim` (`FGSTN_WallAnimState`), updated every frame on the game thread from the Mover presentation state:

| Field | Meaning |
|---|---|
| `Phase` | `None`, `Enter` (just grabbed, `EnterDuration`), `Active`, `Exit` (just left, `ExitDuration`) |
| `SlideState` | `Idle` (still or straight down), `SlideLeft`, `SlideRight`, `Skid` (fast, wins over the sides) |
| `PhaseTime`, `SlideStateTime` | time in the current phase / loop state |
| `bIsActive` | phase is not `None`: the layer wants the pose |
| `bIsClimbing` | the mode is the climb v2 |
| `LateralSpeed`, `VerticalSpeed`, `PlaneSpeed` | velocity on the wall: right positive, up positive (gravity-up, any gravity direction) |
| `LateralAlpha`, `VerticalAlpha` | the same scaled to [-1, 1] for blend spaces |
| `ExitModeName` | mode that took over on leaving (floor, air, wall jump variants of the exit) |

"On a wall" = the active Mover mode is a `Settings.WallModeClass` (default `UGSTN_WallTraverseModeBase`: slide and climb v2). A slide <-> climb hand-over in place
is not an exit. Legacy modes are ignored unless the class is set to `UGSTN_ControlledWallMovementMode`.

## Rules

```
Skid      if PlaneSpeed >= SkidEnterSpeed (750), kept while >= SkidExitSpeed (650); never while climbing (bSkidWhileClimbing)
Left/Right if |Lateral| >= LateralEnterSpeed (120), kept while >= LateralExitSpeed (80) and the sign is the same
Idle      otherwise
```

Defaults sit around the slide tuning: `TerminalDownSpeed` 600 (a steady slide down is Idle, not Skid), `HorizontalControlMaxSpeed` 350, `MaxEntrySpeed` 1500.
The brake input is not in the sync state, so the skid is a speed rule; a fast entry brakes down through it naturally.

## Layer graph (placeholder clips until the final ones)

```
ABP_GSTN_WallLayer (parent GSTN_WallAnimInstance, implements ALI_GSTN_Wall), layer WallLayer:
  state machine
    Enter   --(Phase == Active)--> Loop
    Loop    = inner state machine on SlideState: Idle / SlideLeft / SlideRight / Skid (each a sequence or a blend space on LateralAlpha, VerticalAlpha)
    Loop    --(Phase == Exit)----> Exit
    Enter   --(Phase == Exit)----> Exit
    Exit    --(Phase == Enter)---> Enter     (re-grab during the exit)
  entry state: Enter; all rules read WallAnim through property access (thread safe)
```

Main ABP (`SandboxCharacter_Mover_ABP`): reparent to `GSTN_CharacterAnimInstance`, then one `Blend Poses by bool` (`bOverrideLayerActive`, blend time
`OverrideBlendTime`, inertialization) between the GASP pose and a `Linked Anim Layer` node `WallLayer`, with `ABP_GSTN_WallLayer` as its instance class until
anim sets link it by data (anim-layers.md part B).
