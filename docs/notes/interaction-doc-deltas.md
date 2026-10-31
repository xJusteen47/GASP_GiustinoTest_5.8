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

## I1 runtime (2026-10-24)

- Section 7: `GSTNInteractRuntime` does not depend on `UMG`: the minimal presentation draws with the debug helpers. A real prompt widget is
  a project presenter; add `UMG` when one ships in the plugin.
- Section 5 not built: outline, overlay and ground-ring highlight, the style asset (`UGSTN_InteractStyle`) with `ActiveMeanings` queries,
  nearby markers (`MaxNearbyMarkers`), the 80 ms cross-fade, the override stack. Presenters are listed in the settings instead of a style
  asset; the owner asked for no visual polish.
- Section 3.2: `UGSTN_InteractSubsystem::GetFrame()` is the game state's server world time times `FrameRateHz`. The spec assumed an accessor
  for the server's sim frame from combat; this is the non-NP stand-in. `SetFrameOverride` exists for tests and replay tools. A bridge in
  I2 must stamp with the NP frame instead; the two clocks must not be mixed in one world.
- Section 3.2: spawned handles are `0x80000000 | serial`, stable-key handles clear the high bit, so the two never collide. Level-placed
  handles hash `AActor::GetPathName(World)`; stability under World Partition is still *Unsure* (the registry rejects a duplicate and logs it).
- Section 3.4: `UGSTN_InteractorComponent` is an `IGSTN_InteractFocusSource` (the interface is C++ only); `IGSTN_InteractPointingSource`
  is a Blueprint-implementable interface with one event, `GetPointingInput`. Without it the interactor points with location, forward and,
  as a stand-in for the stick, the planar velocity.
- Section 3.4: `ServerInteract(Id, Verb)` is validated by `IsStillInteractable` with the owner's pointing origin and the component's
  `CanInteractServer`; only registry records (components) are resolvable. Candidates of providers are the consumer's to act on.
- Section 4: `ProbeEveryNFrames` is honoured by the component's tick stride.
- Section 10: `GSTNInteract.Runtime.*` has six tests: the five named plus `SettingsAndRulesAssetAreSane`. `RecordReplicatesWithStamps` checks
  the replicated property flags and that a client building a record from the stamp gets the server's record field for field; the actual
  property replication path is for the later harness scenarios (`scenario_interact_late_spawn`), not for a headless test.
- `FGSTN_InteractRecordStamp` replicates `COND_InitialOnly`; `UpdateRecord` therefore changes the server's local stamp only. A moved
  replicated record needs a reliable multicast or a second stamp: left for the first consumer that moves records (M10).

## What I2 needs (seams, not built)

- Sync fragment `FGSTN_InteractFocusFragment` and the tick extension at the tag-projection hook (M3 section 7.2 step 10) with an explicit priority.
- The `TargetKey` seam in `FGSTN_ActivationContext` (M2 7a / M3) so the ability reads the focus id the tag matched.
- A sim-frame accessor usable by non-NP spawn code (or a bridge-owned clock) to replace `GetFrame()` for stamping.
- Combat's snapshot reader to build the opaque `FGSTN_InteractHostContext` for snapshot providers, and the 16 bit yaw of the command to feed
  `FGSTN_InteractQuery::StickDirection` (the Core re-quantises around `Up`).
- The central provider-id file (modularity 5b) in place of `GSTNInteractProviders`.
