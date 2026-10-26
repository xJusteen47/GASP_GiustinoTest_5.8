# Gravity volumes, Super Mario Galaxy style (`GSTNGravity`)

Status: **specification, 2026-10-26.** Owner request (17:21): "aggiungi alle specifiche volumi dove la gravità cambia alla Mario
Galaxy". Verbatim in [game-design-direction.md](game-design-direction.md) §6l. It builds on the standing direction of §2d: gravity
can change arbitrarily, and "up" is always the opposite of the entity's local gravity at that frame, never world Z. Questions
GRV.1–GRV.4 are in [open-questions-m4-m10.md](open-questions-m4-m10.md). Nothing here is built.

## 1. What exists today (checked on `feature/gas-npp-5.8`)

- Everything downstream already follows the local gravity: the jump state machine, fast fall and gravity phases
  (`GSTN_JumpLogic.h`, `GSTN_GravityPhases.h`), dashes and dodges (`GSTN_DashTypes.h`), hit resolution and reactions ("the victim's
  local up, never world Z"), the Mover sim-state tracker (quantised gravity direction), the camera spec (camera.md §7.2) and the depth
  marker (along local gravity). M3 tested tilted gravity in the harness.
- **Gravity itself is a Mover component setting, not simulated state.** `UMoverComponent::GetGravityAcceleration()` returns
  `GravityAccelOverride` when `bHasGravityOverride` is set, otherwise the engine physics volume's `GetGravityZ()` along world Z, otherwise
  −980 on Z. The harness sets it on every machine while the pawn stands still (`SetTestGravity`), and the jump phases rewrite the
  override every frame and detect outside changes by comparing with what they wrote last (`FGSTN_GravityBase`).
- So gravity that **changes while the character moves through the level** has no home yet: an override set from an overlap event
  is game-thread state, a rollback does not restore it, and the engine physics volume only scales world Z.

## 2. Goals

1. Galaxy-style fields: walking around small planets, cube planets, flipped and sideways areas, tubes, zero gravity bubbles, gravity
   switches.
2. **Gravity is a pure function of the entity's position and the frame**, computed inside the simulation every frame, so prediction
   and resimulation get the same value on every machine with no new sync state.
3. Designers place and tune fields in the editor (shapes, strength, priority), with a visualiser; no C++ per level.
4. A plugin usable alone; Mover, combat and activation are optional bridges.

## 3. Fields

A **field** is a region (where it applies) plus a rule (which way it pulls). It is a component (`UGSTN_GravityFieldComponent`), so a
planet mesh actor can carry its own field, and a plain `AGSTN_GravityVolume` actor wraps one for empty space.

| Field type | Pull at point P | Galaxy use |
|---|---|---|
| `Directional` | a constant direction (the component's −Z by default) | flipped or sideways areas, flat levels |
| `Point` | towards a centre (`bInverted`: away from it) | round planets; inside a hollow sphere |
| `Line` | towards the nearest point of a segment (`bInverted`: away) | cylinder planets; inside a tube |
| `Ring` | towards the nearest point of a circle | ring and torus planets |
| `Box` | towards the nearest point of an inner box (optional edge radius) | cube planets: flat faces, smooth edges |
| `Spline` | towards the nearest point of a spline, or along minus the spline's up at that point (twisting paths) | ribbons and tracks |
| `Zero` | none | zero gravity bubbles |

Data, `FGSTN_GravityFieldSpec`:

```
FGSTN_GravityFieldSpec {
    EGSTN_GravityFieldType Type;
    FGSTN_GravityRegion Region;          // Box, Sphere or Capsule, in the component's space: where the field applies
    float StrengthCmPerSecond2 = 980;    // constant inside the region, as in Galaxy (no falloff by default)
    bool bInverted = false;
    int32 Priority = 0;                  // higher wins where regions overlap
    FGameplayTagContainer Tags;          // Gravity.Field.* designer identity, for queries (an ability allowed only in zero gravity)
}
```

**Which field applies** (pure, deterministic): among the fields whose region contains P, the highest `Priority`; on a tie, the one
whose pull target (centre, axis, box surface) is nearest to P; then the lower **registry order**. The registry is sorted once at load by
the actor's level path and name, so every machine builds the same order. Outside every field the level's default applies (question
GRV.2).

**Moving and switching fields.** Fields are static, or move only as a pure function of the frame: a field attached to an activation
zone member follows its track or rotation response, and a **gravity switch** (Galaxy's arrow panels) is a `Directional` field whose
direction is picked by the zone's `Value(F)` (bridge `GSTNGravityActivation`, [level-activation.md](level-activation.md)). Gravity
stays `g(P, F)`, a pure function.

## 4. Simulation

**One writer per pawn, every simulated frame.** At the start of each frame (resimulated frames included), the pawn's gravity is
`g = Field(P, F) × PhaseScale`, where P is the position in the Mover sync state at the end of the previous frame, and it is written to
Mover's gravity override before the movement modes run. Because P and F come from the simulation, a resimulation computes the same
`g`; the override is rewritten every frame, so its being component state does not matter.
- Pawns with combat: the combat gravity writer (the jump phases, which already run in the ability system pre-tick before Mover, CLAUDE.md
  rule 5) gets its base from the field instead of detecting outside changes. Seam: `FGSTN_GravityBase` gains an optional base source
  (`IGSTN_GravityBaseSource`, implemented by the bridge `GSTNGravityCombat`); without it the detection stays as today.
- Pawns without combat: the bridge `GSTNGravityMover` writes the field's value itself.
- Spike **G1**: confirm that the fork's pre-tick hook (`UNpAbilitySystemComponent::OnSimulationPreTick`, gas-npp.md) also runs on
  resimulated frames before Mover, and that no Mover path caches gravity across frames.

**Up and orientation.** The simulation's up is `−g` at once: floor checks, movement plane, jumps and hits use the new gravity on the
frame it changes, which keeps them simple and deterministic. What turns smoothly is **presentation**: the mesh rotates towards the new
up at `MeshUpRateDegPerSecond` (*Proposal* 720, question GRV.1) through a mesh offset, like Mover smoothing, and the camera already
rate-limits its up (camera.md §7.2, 90°/s). Velocity is kept in world space across a change, as in Galaxy, so a jump between two planets
carries its momentum.

**Zero gravity.** Mover deduces up from gravity and falls back to world Z when gravity is zero. In a `Zero` field the writer also sets
Mover's up override to the pawn's current up (from the orientation in the sync state), so the character keeps its last up and floats.
Moving and turning in zero gravity use the falling mode's air control (a dedicated swim-like mode is later, only on need).

**Cost.** A level has a handful of fields; the subsystem keeps them in a small bounding-volume list, and evaluation is closed-form
except `Spline` (nearest point on pre-sampled points, 64 by default). Minions call the same evaluation for their lightweight movement
(M8), once per minion per frame.

## 5. Network

- No new sync state: gravity is recomputed from replicated position and frame, and switches read the replicated zone value.
- Near a region boundary the client and the server can pick different fields for a frame because their positions differ slightly;
  that is one movement correction. Mitigation: overlapping regions with priorities, so most crossings happen in the air. The harness
  counts corrections at boundaries (§9); if they are frequent, a one-byte "current field" with hysteresis goes into the sync state.
- Late joiners need nothing: the fields are level data.

## 6. Consequences for other specs

| Spec | Change |
|---|---|
| [camera.md](camera.md) §7.2 | none in principle: up is rate-limited and forward transported. Add test `CameraYawContinuousAroundPlanet` (the deterministic reference direction for `CameraYaw` must not jump while running around a planet; encoder and simulation share it) |
| [input.md](input.md) | the stick-reference hold already planned for the camera (follow-up in PR #4) keeps "forward" steady while up turns around a planet |
| combat jump phases (`FGSTN_GravityBase`) | optional base source seam (§4); owner OK needed, it edits `GSTNCombatCore` |
| [combat-m8-minions.md](combat-m8-minions.md) | lightweight movement calls `Evaluate(P, F)`. Enemies on round planets cannot use the navmesh (it assumes world Z up): question GRV.3 |
| [combat-m10-held-objects.md](combat-m10-held-objects.md) | in `Directional` fields thrown objects keep their analytic segments; in curved fields (planets) they integrate per frame, within the host budget |
| [depth-marker.md](depth-marker.md) | unchanged: straight along the local gravity at the character (towards the centre on a planet) |
| [game-modes.md](game-modes.md) | gravity levels **disable the world `KillZ`** (it is world Z) and use KO volumes shaped for the level |
| [wall-movement.md](wall-movement.md) | walls are classified against the local up; spike G2 checks a curved floor and a cube planet edge |
| [level-activation.md](level-activation.md) | gravity switches through `GSTNGravityActivation` (§3) |
| physics props | loose debris follows the field as a per-body force, presentation only; question GRV.4 |

## 7. Plugin layout and tags

| Module or plugin | Contains | Depends on |
|---|---|---|
| `GSTNGravityCore` | field types, pure evaluation and selection, native tags `Gravity.*` | engine only |
| `GSTNGravityRuntime` | field component, volume actor, world subsystem (registry, order, `Evaluate(P, F)`), editor visualiser (arrows on a grid), mesh up smoothing component | Core |
| bridge `GSTNGravityMover` | per-frame writer for Mover pawns, zero-gravity up | `GSTNGravity` + Mover |
| bridge `GSTNGravityCombat` | base source for the jump phases | `GSTNGravity` + `GSTNCombat` |
| bridge `GSTNGravityActivation` | fields moved or switched by an activation zone | `GSTNGravity` + `GSTNActivation` |

Tags (root `Gravity.*`, owner `GSTNGravityCore`): `Gravity.Field.*` (designer identity leaves in ini), `Gravity.State.Zero` (projected
while in a zero field, for ability requirements), `Gravity.Event.FieldChanged` (presentation event and cue trigger).

## 8. Risks and spikes

| Risk | Spike or mitigation |
|---|---|
| Gravity override written at the wrong point of a resimulated frame | G1 (§4) |
| GASP animation assumes world Z (motion matching trajectory, foot traces, Mover smoothing) | **G2**: the GASP character on a small planet and a cube planet; fix what assumes Z in the project's anim layer, never in the sample content |
| Navmesh on curved surfaces | question GRV.3; v1 enemies only on `Directional` fields |
| Corrections at region boundaries | harness count; hysteresis byte only if needed (§5) |
| Engine `KillZ` and physics volumes along world Z | disable `KillZ` in gravity levels; fields never use engine physics volumes |

## 9. Tests

- Automation (`GSTNGravity.Core.*`): `DirectionalIsConstant`, `PointPullsToCentre`, `InvertedPointPushesAway`, `LinePullsToAxis`,
  `RingPullsToCircle`, `BoxFacesFlatEdgesSmooth`, `SplineUpFollowsTwist`, `HigherPriorityWins`, `TieByNearestTargetThenOrder`,
  `RegistryOrderStableAcrossLoads`, `ZeroKeepsLastUp`, `EvaluateIsPureFunctionOfPositionAndFrame`.
- Harness `scenario_gravity_planet` (host and two clients, 60/15 ms): run around a small planet, jump to a second planet, cross a flip
  volume; final compare, corrections counted per boundary crossing, camera yaw continuity logged.
- Test map TM1 station 12, "Gravity" ([plans/test-maps.md](plans/test-maps.md)): flip and sideways volumes, a small round planet, a cube
  planet, two planets with a jump gap, a zero gravity bubble, an arrow switch on an activation zone.

## 10. Order of work (*Proposal*)

Core and Runtime touch no shared code and can be built any time (parallel-safe). Then spike G1 and the Mover bridge; then G2 with the
GASP character; then the combat seam (owner OK) and the minion hook when M8 starts. No milestone number yet: the planning thread places
it after M5 unless the owner wants it sooner.
