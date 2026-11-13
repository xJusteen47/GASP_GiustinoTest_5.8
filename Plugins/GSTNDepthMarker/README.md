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
surface along its own gravity (both in the colour of the player, same rank logic as the toy look; the dashes grow with the drop:
`DashGrowthMax` x4 between `GrowStartCm` 100 and `GrowEndCm` 1200, length and gap together, `FGSTN_DepthMarkerMath::DashScale`), a lighter ring plus line above it on the last platform's plane when it is mid-air below it, and a
ring plus line in light blue on a nearby higher platform's plane (the approach marker).
**No decal content ships**: `UGSTN_DepthPresenter_Decal` draws nothing until a style names a decal material (its
`MaterialOverride`) or the settings do (`DefaultDecalMaterial`). Clear the two ini entries and the built-in presenters also draw
nothing until a style names its own mesh and material. The debug material is an engine editor asset: a packaged game should
point the settings at its own content.

## Presented pose (no stepping)

The marker follows the PRESENTED character, not the simulated one. Mover simulates the root at the fixed tick (62.5 Hz) and shows the character
through a visual mesh that `UMoverComponent::FinalizeSmoothingFrame` (smoothing mode `VisualComponentOffset`) writes every frame; Network Prediction
runs that in `OnWorldPreActorTick`, so the subsystem (`OnWorldPostActorTick`) already ticks after it. The jitter came from the adapters reading
the root/capsule (`UGSTN_DepthSubjectAdapter::GetPresentedOffset` now adds the visual mesh offset from its archetype rest place to the feet; the
Character mesh, else the first non-root mesh; `bFollowPresentedMesh` turns it off) and from a probe answer being applied on the column of the frame
that issued it (it is now re-projected on the current column with `ReprojectTarget` on probed frames too, like the in-between ones).

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

## Toy look (prototype)

A second, selectable look (`UGSTN_DepthMarkerStyle::PopulateToyPresenters`): a fake light along gravity. On a surface: a stained-glass
decal (dark silhouette, coloured core) plus a tapered beam (`UGSTN_DepthPresenter_Cone`, tip at the character). In the air (no-ground
ring, above, approach marker): the beam ends in a fog puff (`UGSTN_DepthPresenter_FogPuff`, camera-facing or plane-aligned quad).
Colour per player: the settings `PlayerPalette` (red, blue, yellow, green) indexed by the **alphabetical rank of the player names**
(case-insensitive, tie-break by player id; `FGSTN_DepthToy::ColorRankOf`), then `TeamPalette`, then the style tint; the component's
`TintOverride` always wins. Beam and puff take the colour through custom primitive data (layout in `FGSTN_DepthToy`, base index
`ToyCustomPrimitiveDataBase`), no per-frame dynamic material; the decal keeps a dynamic material (decals cannot read it).
The plugin ships no content: the project names `ToyStyle` in its own ini; the assets come from `Tools/DepthMarker/setup_depth_marker_toy_look.py`.
`DefaultLook` (Classic or Toy) is the default; `gstn.DepthMarker.Style toy|classic` overrides it per machine at runtime.

### Toy look: the landing spot leads (near-production prototype)

The decal is the marker, the beam only points at it. One size drives both (`FGSTN_DepthToyParams`, on the style as `Toy`, tuned in the
style asset): `LandingRadius = foot radius x LandingRadiusFootMultiplier x curve(height)`, tight and crisp when low (`NearScale`,
`SoftnessNear`), larger and softer when high (`FarScale`, `SoftnessFar`). The decal radius, the beam base radius and the fog puff of an air
marker (`PuffSpan` x the landing diameter) all come from it (`FGSTN_DepthToy::ResolveLook`), so the beam is never wider than the decal; at the
character it is `BeamTopFootMultiplier` (0.6) of the capsule radius. Smoothing: the size and the beam end follow an exponential filter
(`SmoothingSeconds`), the beam end only along Up; the decal itself stays on the raw column point, so it never lags behind the character.

Visibility rules (all values in `Toy`):
- Nothing is drawn while the column from the character to the landing spot is shorter than `MinHeightCm` (40); it fades in over `FadeBandCm`.
  The same fade drives the decal, the beam and the puff, so nothing pops.
- Beam clearance: a sphere (the capsule of the beam seen from the side, radius between the top and the landing radius) is swept down the
  column with the existing trace budget (`UGSTN_DepthMarkerSubsystem::IssueClearance`, after the primary probes, never above the cap, at the rank
  rate of the marker). Any blocking hit that is not a floor (normal dot Up < 0.7) fades the beam out (`ClearanceFadeOutSeconds`), the decal
  stays; `FGSTN_DepthSubjectState::bOnWall` does the same without a sweep. The beam eases in when the marker first shows.
- The decal projects straight along -Up in a thin box (`DecalDepthCm`, deeper only on a slope), fades by the angle of the surface normal
  (`SlopeFadeStartDot` / `SlopeFadeEndDot`) and the material reads the scene normal under each pixel, so it does not smear onto walls.

Why these choices (what the research behind them says):
- Blob shadows (Mario 64 onwards) are a depth cue: a dark soft disc right under the character. Failure mode: the "Peter Pan" gap. So the spot
  is centred on the column at all times, fades in smoothly from the minimum height and never pops; the dark shadow stays modest and a thin
  bright, player-tinted ring carries the "this is where you land" read.
- A decal bleeds through solid objects onto walls behind: thin box, straight along -Up, fade by surface angle (above).
- Fake god rays (cone or cylinder mesh, additive unlit, gradient along the axis, fresnel soft edge, DepthFade where it meets geometry, camera
  distance fade, a faint moving pattern): `M_DM_Beam` follows that recipe; it also carries a very low contrast version of the stained-glass
  cells so beam and spot read as one light. The beam is a cone that continues above the character to its apex (`ApexExtension`) and the material
  cuts it at the character, so the radius there is exactly the top radius.
- Why fake: screen-space light shafts exist only for a directional light (no per-character control), volumetric fog with shadowed lights is
  expensive and flickers with spot or point lights at low settings, coloured translucent shadows are not supported with Virtual Shadow Maps in
  UE5, and a mesh shaft is cheap but looks poor when the camera is close or inside it. So the marker is a mesh beam plus a decal that never
  rely on scene lights or shadows, with a camera-distance and pixel-depth fade so the beam disappears when the camera is near or inside it.

Visual tests: `GSTN.Visual.DepthMarker.Toy*` (`Private/Tests/GSTN_DepthMarkerToyVisualTests.cpp`) build a small world (floor, wall, platform,
capsule characters with the real marker component), render it with a scene capture and the real project materials and save
`Saved/Notes/ui-snapshots/depth-marker-toy-{heights,wall,air,closeup}.png`. They need a real RHI and the toy style asset and pass without drawing
under `-nullrhi`:
`UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests GSTN.Visual.DepthMarker; Quit" -unattended -nosplash -nosound -NoP4 -RenderOffscreen -AllowCommandletRendering`.
Materials are generated by `Tools/DepthMarker/setup_depth_marker_toy_look.py` and compile-checked by `Tools/DepthMarker/check_materials.py`
(both with `-RenderOffscreen -AllowCommandletRendering`).

### Toy look: the light passes through the character (crack glow)

The fake light along gravity is meant to pass THROUGH the character: a crystal body, or an outer shell over a crystal core, shows the player colour
through its cracks, which are noticeable only when the light passes through, i.e. when the character is airborne. The beam therefore leaves the core of
the character (`BeamStartFraction`, 0.5 of the subject height above the feet), the decal and the glow use the same player colour.
- The marker computes `LightThrough` (0 on the ground, then the same fade as the beam over the column height, 1 when airborne with no ground in range;
  `FGSTN_DepthToy::LightThroughTarget`, smoothed by `LightThroughSeconds`) and writes it with the player colour into the custom primitive data of every
  mesh component (skeletal or static) of the owner: settings `CharacterCPDIndexLight` (16) and `CharacterCPDIndexColor` (17..20, negative = off).
  No character Blueprint edit; the glow stops when the marker goes away. Only with a toy style.
- `MF_DM_CrackGlow` (material function, generated): voronoi cell edges in local space (cracks stick to the body parts; some cells stay whole), crack
  emissive = colour x LightThrough, plus a fake subsurface interior glow (stronger where the surface faces the camera). Outputs `Emissive`, `CrackMask`,
  `Light`, `Color`: add `Emissive` to the emissive of any character material. `M_DM_CrystalCharacter` is the naked crystal (dark glassy body, rim light,
  glowing cracks and interior in the air); `M_DM_CrackShell` is a plain opaque shell with the layer on top.
- Console variable `gstn.DepthMarker.CrystalPreview 1|2|0`: for the local pawns with a marker, swap every mesh material for the crystal (1) or the
  shell with the crack glow (2) at runtime, 0 restores. Materials are named in `Config/DefaultGSTNDepthMarker.ini` (`CrystalPreviewMaterial`,
  `CrackShellPreviewMaterial`); try it in PIE with the toy look on (`gstn.DepthMarker.Style toy`) and jump.
- Snapshot: `GSTN.Visual.DepthMarker.ToyCrystal` (`depth-marker-toy-crystal.png`).

## Console variables

`gstn.DepthMarker.Enable`, `gstn.DepthMarker.MaxMarkers`, `gstn.DepthMarker.TracesPerFrame` (negative = the settings), `gstn.DepthMarker.Style` (`toy`, `classic`, empty = settings), `gstn.DepthMarker.CrystalPreview` (0, 1, 2). `.Debug` and
`.Stats` of the spec are not built.

## Tests

`GSTNDepthMarker.Core.*` (31, with `DashScaleGrowsWithLengthAndClamps`): every test runs with several Up vectors (flipped, sideways, diagonal) so a hidden world-Z assumption
fails; the approach marker has `ApproachMarkerOnHigherPlatformPlane`, `NoApproachWhenNotMovingToward`,
`ApproachUsesVolumePlaneWhenPresent`, `ApproachHysteresisNoFlicker`, `ApproachNeverDuplicatesPrimaryPlane`, `ApproachProbePatternFollowsUp`.
`GSTNDepthMarker.Runtime.*` (16, headless; `MarkerFollowsPresentedPoseNotSimulatedSteps` steps the root every 16 ms and moves the mesh every 4 ms and bounds the per-frame delta of feet and marker point): scheduler cap, ranking, adapters, dedicated-server rule, replication flag, excluded hits,
style validation, a subsystem tick smoke test, the trace cap with approach probes, approach probes as lowest priority, plane component
containment and projection, approach style validation.

## Known limits (v0.1)

- A switch to a new surface fades in; there is no true cross-fade between two surfaces (one marker per layer).
- Probes are `Multi` async line traces: an excluded blocking object (a pawn that blocks the trace channel and is not registered as a
  marker owner) hides the floor behind it. Registered owners are ignored by the probes.
- The Editor module, the test map, the Mover adapter plugin, the Surface and Minion bridges and the harness scenarios are not built.
