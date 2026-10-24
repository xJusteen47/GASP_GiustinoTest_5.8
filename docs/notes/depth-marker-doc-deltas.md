# Depth marker: doc deltas found while implementing (to fold into depth-marker.md)

Each line: where, what changed against the spec, why.

## Core (2026-10-23)

- §5 `UGSTN_DepthMarkerMath` in Core: **not possible as written.** `UBlueprintFunctionLibrary` is an Engine class and Core may
  depend only on Core, CoreUObject and GameplayTags. Core holds `FGSTN_DepthMarkerMath` (plain C++ statics, this is what the tests
  call); the Blueprint wrapper `UGSTN_DepthMarkerMath` lives in Runtime.
- §5 `FGSTN_DepthMarkerMath` gains `Evaluate` (the whole default selector on numbers: frame check, grounded point, filter, choose,
  hysteresis, reference layers). The runtime selector only supplies traces, so every rule is testable without a world.
- §5 `SurfaceKey` is `int64`, not `uint64`: UHT rejects `uint64` as a Blueprint-visible property type.
- §5 `FGSTN_DepthCandidate` gains `ComponentTransform` (for the local-space plane, §2.3 step 7) and `bSurfaceKnown` (for
  `bUnknownSurfacesPass`); `FGSTN_DepthSubjectState` gains `bBaseMoves` (the grounded point ignores moving bases).
- §5 new types: `FGSTN_DepthFootprintHit` (R3 sweep result), `FGSTN_DepthReferenceOverride` (volume plane), `FGSTN_DepthFadeParams`,
  `EGSTN_DepthMarkerState`, `EGSTN_DepthTargetKind`. `FGSTN_DepthSelectorParams` gains `CoplanarAngleDegrees` (the 5 degrees of R1) and
  `bUnknownSurfacesPass`.
- §2.4 R2 vs R4: "nearer wins at once" is held back only while the previous switch was to a FARTHER surface and the dwell has not
  elapsed, otherwise a 30 ms flicker between two surfaces would still ping-pong (test `MinDwellPreventsPingPong`).
- §2.7: the ring below needs a probe this tick (`bProbed`); a grounded subject whose probe is gated shows no ring below.

## Runtime (2026-10-23)

- §5 `AGSTN_DepthReferenceVolume` is not built. The reference layers of the maths still take a `FGSTN_DepthReferenceOverride`, but no
  actor feeds it yet; the runtime selector passes an invalid override.
- §5 `UGSTN_DepthSurfaceResolver`, `IGSTN_DepthSubjectProvider`, the Editor module and the Mover adapter plugin are not built.
- §4.2 the dashed line is cube dashes from an instanced mesh with the dash maths on the CPU, not a shader quad: no custom material
  ships, so a project with no content still shows a line. §4.3 occlusion handling is not built.
- §4.1 no default decal content exists: the decal presenter draws only when a style names a decal material. The default look is a
  ring of flat cube segments plus a dashed line, from two engine assets named in the plugin ini.
- §3 the Audience setting of the spec's settings table lives only on the component (`LocalOnly`, `LocalAndAllies`, `All`). Allies
  are markers with the same `FGSTN_DepthSubjectState::TeamId` as a locally controlled pawn: the runtime does not depend on
  `AIModule`, so it cannot read `IGenericTeamAgentInterface`.
- §2.4 R4 cross-fade: with one marker per layer the cross-fade is a fade-in of the new surface (`SwitchBlendMs`), not a true blend
  of two. The blend length is kept on the marker because the target reports it for one frame only.
- §2.3 step 3 the probe is an async `Multi` line trace; an excluded blocking object that is not a registered marker owner hides the
  floor behind it (registered owners are in the ignore list).
- Console variables: `.Enable`, `.MaxMarkers`, `.TracesPerFrame` exist; `.Debug` and `.Stats` do not.

## Approach marker and projection planes (2026-10-23, owner requirement with a sketch)

New layer on top of the spec; the spec's section 2.7 above marker stays as it is.

- Situation: the character is airborne above a LOWER platform (marker A, straight along gravity, as before) and a HIGHER platform is
  nearby, offset sideways. When the character gets close, a second marker B appears on the **column projected along gravity onto
  that platform's top plane**, hanging in the air beyond the platform's edge. Both show at once; at most one B.
- Section 2 gains step 10, the **approach layer**, evaluated after the below target: `FGSTN_DepthMarkerMath::EvaluateApproach`.
  Candidates: (1) designer planes that overlap the character (`AGSTN_DepthMarkerPlaneVolume`,
  `UGSTN_DepthMarkerPlaneComponent`), which win over probing and are the fallback where probes find nothing; (2) walkable tops from
  capped probes: a ring of downward traces at `ApproachProbeRadiusCm` plus probes along the horizontal velocity (half and full
  `ApproachDistanceCm`).
- Edge distance of a probed candidate is the horizontal distance from the feet to the **nearest sampled point** of that platform
  (an upper bound of the true edge distance; the ring radius bounds the error). Nothing traces the true edge.
- A plane is kept when: not above the feet by more than `ApproachNearAboveFeetCm`; rising `ApproachMinRiseCm..ApproachMaxRiseCm`
  over the primary ground (skipped without a primary surface, and for designer planes); facing up (`ApproachMinNormalDot`, probed
  only); not the primary's plane (same surface key, or coplanar within `CoplanarTolerance` and `CoplanarAngleDegrees`); within
  `ApproachDistanceCm` and either moving towards it (`ApproachMinTowardSpeed`) or within `ApproachCloseDistanceCm`.
- Score: nearest edge distance, then approach speed. Hysteresis like the other markers: a shown plane stays up to
  `ApproachDistanceCm + ApproachHideMarginCm` without the movement test; `ApproachHoldMs` holds it when no probe confirms it
  (probes are staggered); a challenger replaces it only after `ApproachMinDwellMs` and when `ApproachSwitchMarginCm` nearer;
  `ApproachBlendMs` fades the new plane in. A gravity jump clears it (the probed tops belong to the old gravity).
- Budget: approach probes are the lowest priority under `MaxTracesPerFrame` (after primary probes and footprint sweeps), at the
  rank rates of the markers, `ApproachProbesPerTick` slots per tick in rotation; probe results live `ApproachCacheMs` and are
  re-expressed with the platform's current transform every frame.
- Section 5 new: `FGSTN_DepthPlaneCandidate`, `EGSTN_DepthTargetKind::ApproachPlane`, `EGSTN_DepthMarkerState::Approach`,
  `EGSTN_DepthLayer::Approach`, `FGSTN_DepthResult::Approach`, the approach block of `FGSTN_DepthSelectorParams`,
  `UGSTN_DepthTargetSelector::ChooseApproach`, and the style slot `ApproachStyle` (on `UGSTN_DepthMarkerStyle` and as a component
  override; empty = a built-in ring and line in another colour). The approach presenters are forced onto the approach layer.
- Section 7 new tests: `Core.ApproachMarkerOnHigherPlatformPlane`, `NoApproachWhenNotMovingToward`,
  `ApproachUsesVolumePlaneWhenPresent`, `ApproachHysteresisNoFlicker`, `ApproachNeverDuplicatesPrimaryPlane`,
  `ApproachProbePatternFollowsUp` (all with diagonal Up); `Runtime.ApproachProbesRespectTraceCap`,
  `ApproachProbesAreLowestPriority`, `PlaneComponentContainsAndProjects`, `ApproachStyleValidates`. The test map
  `L_DepthMarker_Min` should get a low platform with a higher one beside it and a gap.

## To align before the final version (2026-10-24)

- Player colour order: the prototype ranks players alphabetically from GameState::PlayerArray on every machine every frame, so a late joiner with an earlier name shifts everyone else's colour. combat-parts.md 6.0 wants the host to fix the player numbers at match start (alphabetical then) and late joiners to take the first free number; read that replicated number instead of ranking locally.
