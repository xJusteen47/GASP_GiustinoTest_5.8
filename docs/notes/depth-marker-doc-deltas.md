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
