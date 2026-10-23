# GSTNDepthMarker

A presentation-only depth cue for Unreal Engine 5.8: a marker on the surface under a character joined to it by a dashed line,
and a hollow ring on the last platform's plane when the character is off-stage. Computed locally on every machine from the
presented transform; never simulated, never replicated, never created on a dedicated server.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Design: [depth-marker.md](../../docs/architecture/depth-marker.md).
Binding rules: [modularity-and-tags.md](../../docs/architecture/modularity-and-tags.md) §3.7, §7.1.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNDepthMarkerCore` | exists (Runtime) | Pure types and `FGSTN_DepthMarkerMath`, the target selection maths (filter, nearest surface, hysteresis R1-R5, reference plane, above/below layers, fade). No world, no Engine. Covered by `GSTNDepthMarker.Core.*`. |
| `GSTNDepthMarkerRuntime` | exists (Runtime) | Settings, world subsystem (local players only, never created on a dedicated server), `UGSTN_DepthMarkerComponent` (the opt-in, never replicated), the gravity selector, the CharacterMovement and generic actor adapters, the style asset, the presenters, the trace scheduler with a per-frame cap, the Blueprint wrapper `UGSTN_DepthMarkerMath`. Covered by `GSTNDepthMarker.Runtime.*`. |

## Dependencies

- Plugins: none.
- `GSTNDepthMarkerCore` (`Build.cs`): Core, CoreUObject, GameplayTags. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
  AbilitySystemSimulation, NetworkPrediction or Mover.
- `GSTNDepthMarkerRuntime`: Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNDepthMarkerCore. A Mover adapter would
  be a separate plugin (spec �6.1); the runtime knows no movement type besides `UCharacterMovementComponent` and plain actors.

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
surface along its own gravity, and a lighter ring plus line above it on the last platform's plane when it is mid-air below it.
**No decal content ships**: `UGSTN_DepthPresenter_Decal` draws nothing until a style names a decal material (its
`MaterialOverride`) or the settings do (`DefaultDecalMaterial`). Clear the two ini entries and the built-in presenters also draw
nothing until a style names its own mesh and material. The debug material is an engine editor asset: a packaged game should
point the settings at its own content.

## Extending in Blueprint

- Look: make a `UGSTN_DepthMarkerStyle` (Data Asset), list presenters, assign it on the component, the settings or
  `UGSTN_DepthMarkerSubsystem::PushStyleOverride` (a local override, per machine).
- A Blueprint child of `UGSTN_DepthMarkerPresenter` overrides `OnMarkerUpdated(View, Holder)` and can drive any actor or Niagara
  system (the diegetic option); `Holder` is the transient actor that owns presenter components.
- A Blueprint child of `UGSTN_DepthTargetSelector` overrides `BuildProbe` / `ChooseTarget`; the maths stay callable through
  `UGSTN_DepthMarkerMath`.

## Replacing content

Everything visible is a soft reference: the settings (`DefaultSegmentMesh`, `DefaultSegmentMaterial`, `DefaultDecalMaterial`,
`DefaultStyle`, `DefaultSelector`) and the presenters' `MeshOverride` / `MaterialOverride`. The plugin ships no `Content/`.

## Gameplay tags

None declared (spec §6.4). The target filter reads `Surface.Behaviour.*` through a `FGameplayTagQuery` the caller supplies.

## Console variables

`gstn.DepthMarker.Enable`, `gstn.DepthMarker.MaxMarkers`, `gstn.DepthMarker.TracesPerFrame` (negative = the settings). `.Debug` and
`.Stats` of the spec are not built.

## Tests

`GSTNDepthMarker.Core.*`: every test runs with several Up vectors (flipped, sideways, diagonal) so a hidden world-Z assumption fails.
`GSTNDepthMarker.Runtime.*` (headless): scheduler cap, ranking, adapters, dedicated-server rule, replication flag, excluded hits, style
validation and a subsystem tick smoke test in a throwaway game world.

## Known limits (v0.1)

- A switch to a new surface fades in; there is no true cross-fade between two surfaces (one marker per layer).
- Probes are `Multi` async line traces: an excluded blocking object (a pawn that blocks the trace channel and is not registered as a
  marker owner) hides the floor behind it. Registered owners are ignored by the probes.
- The Editor module, the test map, the Mover adapter plugin, the Surface and Minion bridges and the harness scenarios are not built.
