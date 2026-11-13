# GSTNLevelObjects

Placeable level objects that offer traversal surfaces. First object: the **trampoline** (`AGSTN_Trampoline`), an adjustable box
whose top face launches whoever lands on it. Objects here are data: they implement the GSTNSurface provider contract
(`IGSTN_SurfaceProvider`) and nothing else, so the combat impacts and the launch consumer read them without knowing the class.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Checkpoint H11b of the M4 hits plan.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNLevelObjects` | exists (Runtime, Default) | `AGSTN_Trampoline`. No GAS, Network Prediction or Mover: it needs no fork. Covered by `GSTNLevelObjects.*`. |

## Dependencies

- Plugins: `GSTNSurface` (provider interface, launch params and math), `GSTNCombat` (Core, kept as the plan's layering edge; the
  module does not use it yet).
- `Build.cs`: Core, CoreUObject, Engine, GameplayTags, `GSTNSurfaceCore`, `GSTNCombatCore`. Never `GSTNCombatNP`: the launch is
  consumed by the combat simulation (`FGSTN_SurfaceLaunch` in GSTNCombatNP), the object only answers questions.

## The trampoline

| Property | Meaning |
|---|---|
| `Size` (cm, min 10, edit widget) | depth X, width Y, height Z of the root `UBoxComponent` (extent = Size / 2). |
| `MeshAsset` (soft, default the engine cube) | optional visual; the CDO never loads it, an instance loads it in `OnConstruction`. Empty = invisible box. |
| `MeshFit` | `StretchToBox` scales the mesh to the box; `Keep` leaves its scale. `MeshOffset` moves it from the box centre. |
| `Launch` (`FGSTN_SurfaceLaunchParams`) | direction mode (SurfaceNormal, LocalDirection, Tilt), speed mode (Fixed, ScaledByIncoming), `bKeepTangentialVelocity`, `bResetAirJumps`, `bResetCoyote`, `bAddSurfaceVelocity`, `LaunchTag`. |
| `LaunchFaceToleranceDegrees` | contact normals within this angle of the actor's up are the launch face. |

- The launch face is the actor's **local up**: tilt the actor to tilt the launch, mount it on a wall to launch away from the wall.
  Directions are computed in the actor's frame, never from world up, so any gravity works.
- `GetSurfaceBehaviour` answers `Surface.Type.Trampoline` for every face (a tumbling victim bounces off any side, the H11 row) and
  adds `Surface.Behaviour.Launch` + the params only for the launch face; `Stand` always.
- **Rollback rule**: size and launch params are static configuration read from the level on every machine. A trampoline that moves
  or changes params during a match is not supported; `IsDataValid` warns when the root Mobility is not Static and errors on
  invalid launch params. (T4 of the traversal-surfaces plan relaxes this to "movable only for platform shapes".)
- The editor shows an arrow along the launch direction of the top face (editor-only component).
- Blueprint: make a child class and change `Size`, `MeshAsset`, `Launch` in the class defaults or per instance; no C++.

## How the launch happens (GSTNCombatNP)

`UGSTN_CombatSimComponent::StepSurfaceLaunch` runs in the ability system pre-tick for a combatant that is not tumbling and not in
hitstop: `FGSTN_SurfaceLaunch::Probe` sweeps its collision from the tracked Mover state along `velocity * dt` plus 2 cm along gravity,
resolves the hit through `UGSTN_SurfaceUtils::Resolve` and, if the surface has `Surface.Behaviour.Launch` and the combatant is not
already moving away, queues `FGSTN_LayeredMove_Launch` (a child of Mover's launch, so the Mover state tracker reports an external
launch and the jump state applies the kit's reset rules; the move's flags override them for that launch) through an instant GE.
No new sync state. A tumbling victim keeps the impact table's bounce row.

## Extending

- **Moving trampoline**: answer `GetSurfaceVelocity` from replicated poses (a platform shape) and set `bAddSurfaceVelocity`; the
  consumer already adds it. Lift the Mobility warning only for objects whose pose is part of the simulation.
- **One-way trampoline**: return `Launch` only when `Query.ContactNormal` is on the allowed side (the same per-face switch the top face uses).
- **Other objects**: implement `IGSTN_SurfaceProvider` on an actor or component; nothing in the combat code changes.

## Tests

`Automation RunTests GSTNLevelObjects`: `SizeDrivesBoxExtent`, `MeshFitStretch`, `LaunchDirectionModes`, `SpeedModes`,
`SoftMeshNotLoadedInCDO`. The consumer is covered by `GSTNCombat.NP.Hits.Launch.*` and the PIE scenario `trampoline`.
