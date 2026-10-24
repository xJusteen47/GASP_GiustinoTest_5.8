# Interaction: doc deltas found while implementing (to fold into interaction.md)

Each line: where, what changed against the spec, why.

## Owner questions INT.1 to INT.6: defaults taken (2026-10-24)

INT.1 is a *Decision* in the spec (the character points). INT.2 to INT.6 are unanswered; the spec's *Proposal* was used for each.

| Question | Default taken |
|---|---|
| INT.1 point with the character or the screen centre | `StickThenFacing` (Decision); `CameraAim` exists as data, off |
| INT.2 button to cycle the focus | Maths supports it (`CycleSkipFrames`, `bCycleRequested`); default 0 = off; the runtime interactor exposes `RequestCycle()` but binds no input |
| INT.3 faint markers on other candidates | Off by default; not built in I1 (`MaxNearbyMarkers` omitted) |
| INT.4 do allies see your focus | No: only the locally controlled interactor is presented |
| INT.5 priority among meanings | Data in the meaning table; proposal order Grab > Pickup > Objective > Equipment > Mount (no default entries: the leaves belong to consumers) |
| INT.6 unusable items greyed or ignored | Greyed per candidate through `bFocusWhenUnactionable` (default false = ignored) |

## I0 core (2026-10-24)

- §7, §9 `UGSTN_InteractMath` in Core: **a plain `UObject` holder of statics, not a `UBlueprintFunctionLibrary`.** The library class is in
  Engine and Core may depend only on Core, CoreUObject and GameplayTags (same finding as depth-marker). The two Blueprint-pure helpers
  (`MakeCandidateId`, `HashStableKey`) are plain statics; a Blueprint wrapper can live in Runtime if a designer needs one.
- §9 `FGSTN_InteractQuery` is a plain struct, not a USTRUCT: it carries pointers to the caller's rules, registry, tags and providers and is
  valid for the call only. `FGSTN_InteractFocusState`, `Result`, `Candidate`, `CandidateId`, `Rules` stay USTRUCTs.
- §9 `FGSTN_InteractCandidateId::Handle` is `int32` in the property (the bit pattern of the uint32): UHT rejects `uint32` as a
  Blueprint-visible property type. `ToKey()` and the constructor use `uint32`.
- §9 `FGSTN_InteractCandidate` numbers: position is `FIntVector` whole centimetres (the quantisation of §2.3 happens at registration, not
  per probe), `RadiusCm`, `Priority` (-1 = the meaning's default), `QueryIndex` indexes the query's `Requirements` array.
- §2.1 new rules field `StickThreshold` (0.3): the spec says "past the Aim zone" of the classifier; Core cannot read the classifier, so the
  caller passes the stick magnitude and the rules say where it starts to point.
- §2.1 quantisation: the planar pointing yaw is rounded to 16 bits relative to a basis derived from `Up` (`FindBetweenNormals(Z, Up)`);
  the 3D aim direction is rounded to 1/32768 per component. The combat classifier's own 16 bit yaw is in its own frame; a bridge should
  hand over the direction it already holds.
- §2.3 ties: "costs within `TieEpsilon` tie" is not a transitive relation and cannot be a sort comparator. Costs fall in buckets of
  `TieEpsilon` (integer division of the fixed point cost), then higher priority, then the lower id. Margins in §2.5 compare the exact
  fixed point costs.
- §2.2 the arc interpolates linearly in surface distance from `ArcNearDeg` (at `NearRadiusCm`) to `ArcFarDeg` (at the candidate's reach);
  `AimArcDeg` replaces both in 3D mode, and the height band is not applied there (pitch reaches shelves).
- §2.4 an untraced `StaticGeometry` candidate beyond `MaxLosTraces` is dropped (not focusable), never assumed clear. The LOS origin is the
  query `Origin`: a caller wanting a chest point passes the chest as origin.
- §2.5 `ProbeEveryNFrames` is data for the caller; `UpdateFocus` runs whenever called.
- §3.2 ghost: `FGSTN_InteractFocusState::AdoptAuthoritative(Id, SinceFrame, bGhost)` is how a reconcile hands the server's id to a machine;
  a ghost is dropped the moment a record with that id exists, or replaced after the dwell by the best known candidate.
- §4 the registry grid cell is configurable (`Configure`); the runtime uses `HardMaxReachCm * StickyReachScale + 100 cm` so one ring of 27
  cells covers the relaxed reach plus a radius. `Purge` drops tombstones; `RemoveImmediate` is for local, non-simulated records.
- §8 no tag ini and no tag search path in Core: the plugin declares only the roots as native tags.
- Providers: ids 1 (registry) and 16+ (consumers) are defined in `GSTNInteractProviders`, standing in for the central id file of
  modularity §5b until it exists.
