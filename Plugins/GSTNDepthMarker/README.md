# GSTNDepthMarker

A presentation-only depth cue for Unreal Engine 5.8: a marker on the surface under a character joined to it by a dashed line,
a hollow ring on the last platform's plane when the character is off-stage, and a second marker on a nearby higher platform's plane
(the approach marker). Computed locally on every machine from the
presented transform; never simulated, never replicated, never created on a dedicated server.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Design: [depth-marker.md](../../docs/architecture/depth-marker.md).
Binding rules: [modularity-and-tags.md](../../docs/architecture/modularity-and-tags.md) §3.7, §7.1.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNDepthMarkerCore` | exists (Runtime) | Pure types and `FGSTN_DepthMarkerMath`, the target selection maths (filter, nearest surface, hysteresis R1-R5, reference plane, above/below layers, fade). No world, no Engine. Covered by `GSTNDepthMarker.Core.*`. |
| `GSTNDepthMarkerRuntime` | exists (Runtime) | Settings, world subsystem (local players only, never created on a dedicated server), `UGSTN_DepthMarkerComponent` (the opt-in, never replicated), the gravity selector, the CharacterMovement and generic actor adapters, the style asset, the presenters, the trace scheduler with a per-frame cap, the plane volume and component of the approach marker, the Blueprint wrapper `UGSTN_DepthMarkerMath`. Covered by `GSTNDepthMarker.Runtime.*`. |

## Dependencies

- Plugins: none.
- `GSTNDepthMarkerCore` (`Build.cs`): Core, CoreUObject, GameplayTags. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
  AbilitySystemSimulation, NetworkPrediction or Mover.
- `GSTNDepthMarkerRuntime`: Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNDepthMarkerCore. A Mover adapter would
  be a separate plugin (spec §6.1); the runtime knows no movement type besides `UCharacterMovementComponent` and plain actors.

## Using it

1. Enable the plugin (`EnabledByDefault` is false).
2. Add `UGSTN_DepthMarkerComponent` to a pawn, by pawn data, a Game Feature `AddComponents` action or Blueprint. That is the whole
   opt-in: no code asks "is this an enemy". The component is not replicated and is inert on a dedicated server.
3. Tune Project Settings > Plugins > GSTN Depth Marker (trace channel, budgets, selection tolerances). Platforms must **Block** the
   trace channel (default `Visibility`). Pawns are excluded by object type.

Per pawn data: `Style` (soft ref), `Selector` (instanced), `Audience` (`LocalOnly`, `LocalAndAllies`, `All`; enemies opt in with
`All`), `TintOverride`, and `HideWhen` / `ShowOnlyWhen` tag queries read from an owner that implements `IGSTN_DepthTagSource`.
A pawn can implement `IGSTN_DepthSubject` to report its own state; a project movement type registers a
`UGSTN_DepthSubjectAdapter` class with `FGSTN_DepthAdapterRegistry` (highest `Priority` wins, later wins ties).

## What is drawn by default

With no style set the subsystem uses a built-in style made of **engine primitives only**, named as soft references in
`Config/DefaultGSTNDepthMarker.ini` (`/Engine/BasicShapes/Cube` and `/Engine/EngineDebugMaterials/DebugMeshMaterial`, parameters
`Color` and `Opacity`): a hollow ring of flat cube segments and a dashed line of cube dashes, below the character on the first
surface along its own gravity, a lighter ring plus line above it on the last platform's plane when it is mid-air below it, and a
ring plus line in light blue on a nearby higher platform's plane (the approach marker).
**No decal content ships**: `UGSTN_DepthPresenter_Decal` draws nothing until a style names a decal material (its
`MaterialOverride`) or the settings do (`DefaultDecalMaterial`). Clear the two ini entries and the built-in presenters also draw
nothing until a style names its own mesh and material. The debug material is an engine editor asset: a packaged game should
point the settings at its own content.

## Approach marker and projection planes

The character is airborne above a lower platform (marker A, straight down along gravity) and a higher platform is nearby, offset
sideways. When the character gets close, a **second marker B** appears at the character's position projected along gravity onto the
higher platform's top plane, hanging in the air beyond the platform's edge, so the player reads that surface's height. Both show at
once; there is at most one B. It has its own style slot, `ApproachStyle` (on the style, with a component override; the built-in
look is a ring and a dashed line in another colour). The above marker of the spec is a different layer and stays.

- **Probing**: a ring of downward traces around the character plus two along its horizontal velocity, a few per tick in rotation,
  lowest priority under the per-frame trace cap. A walkable top qualifies when it rises above the primary ground, is not above the
  feet by more than `ApproachNearAboveFeetCm`, its nearest sampled point is within `ApproachDistanceCm` and the character moves
  towards it (or is within `ApproachCloseDistanceCm`). Nearest wins, then approach speed. Never the primary target's plane.
- **Explicit planes** (override and fallback): place an `AGSTN_DepthMarkerPlaneVolume` (or add a `UGSTN_DepthMarkerPlaneComponent`
  to any actor). Its box is the region of influence and its **top face** (plus `PlaneOffsetCm`, along its own up) is the plane;
  while a character overlaps the box that plane wins over probing. `Priority` picks among overlapping ones.
- **Stability**: hysteresis between `ApproachDistanceCm` and the hide margin, a hold time while probes are staggered, a minimum dwell
  and a switch margin before another platform takes over, a fade-in on appearance; a gravity jump clears it. All tuning is the
  `DepthMarker|Approach` block of `FGSTN_DepthSelectorParams` (settings `DefaultParams`, or a component's own selector).
- The edge distance is measured to the nearest **sampled** point of the platform, an upper bound of the true edge distance.
  Details and deltas: [depth-marker-doc-deltas.md](../../docs/notes/depth-marker-doc-deltas.md).

## Extending in Blueprint

- Look: make a `UGSTN_DepthMarkerStyle` (Data Asset), list presenters, assign it on the component, the settings or
  `UGSTN_DepthMarkerSubsystem::PushStyleOverride` (a local override, per machine).
- A Blueprint child of `UGSTN_DepthMarkerPresenter` overrides `OnMarkerUpdated(View, Holder)` and can drive any actor or Niagara
  system (the diegetic option); `Holder` is the transient actor that owns presenter components.
- A Blueprint child of `UGSTN_DepthTargetSelector` overrides `BuildProbe` / `ChooseTarget` / `ChooseApproach`; the maths stay callable through
  `UGSTN_DepthMarkerMath`.

## Replacing content

Everything visible is a soft reference: the settings (`DefaultSegmentMesh`, `DefaultSegmentMaterial`, `DefaultDecalMaterial`,
`DefaultStyle`, `DefaultSelector`) and the presenters' `MeshOverride` / `MaterialOverride`. The plugin ships no `Content/`.

**Grid plane layer.** `UGSTN_DepthPresenter_Plane` ("Plane (grid material quad)") is in the built-in below, above and approach looks and
is off while its `PlaneMaterial` is empty. Set `PlaneMaterial` (optionally `MeshOverride`; `PlaneExtentCm` = side length, default 150) and
a quad is drawn centred on the marker point, oriented to the layer's present normal. The material instance gets, every frame:
`MarkerWorldPosition` (vector, world cm), `MarkerNormal` (vector, unit), `Opacity` (scalar, the layer alpha including edge alpha) and
`EdgeDistance` (scalar, cm: horizontal distance to the platform edge on the approach layer, 0 elsewhere). A world-aligned grid reads
`MarkerWorldPosition` and `MarkerNormal` and fades with `Opacity`.

The approach marker never depends on the ground below: with no primary surface, or only the "no ground" ring, it still shows.

## Gameplay tags

None declared (spec §6.4). The target filter reads `Surface.Behaviour.*` through a `FGameplayTagQuery` the caller supplies.

## Console variables

`gstn.DepthMarker.Enable`, `gstn.DepthMarker.MaxMarkers`, `gstn.DepthMarker.TracesPerFrame` (negative = the settings). `.Debug` and
`.Stats` of the spec are not built.

## Tests

`GSTNDepthMarker.Core.*` (30): every test runs with several Up vectors (flipped, sideways, diagonal) so a hidden world-Z assumption
fails; the approach marker has `ApproachMarkerOnHigherPlatformPlane`, `NoApproachWhenNotMovingToward`,
`ApproachUsesVolumePlaneWhenPresent`, `ApproachHysteresisNoFlicker`, `ApproachNeverDuplicatesPrimaryPlane`, `ApproachProbePatternFollowsUp`.
`GSTNDepthMarker.Runtime.*` (15, headless): scheduler cap, ranking, adapters, dedicated-server rule, replication flag, excluded hits,
style validation, a subsystem tick smoke test, the trace cap with approach probes, approach probes as lowest priority, plane component
containment and projection, approach style validation.

## Known limits (v0.1)

- A switch to a new surface fades in; there is no true cross-fade between two surfaces (one marker per layer).
- Probes are `Multi` async line traces: an excluded blocking object (a pawn that blocks the trace channel and is not registered as a
  marker owner) hides the floor behind it. Registered owners are ignored by the probes.
- The Editor module, the test map, the Mover adapter plugin, the Surface and Minion bridges and the harness scenarios are not built.
