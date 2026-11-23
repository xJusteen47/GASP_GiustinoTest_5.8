# GSTNInteract

One focus, one prompt: what the Interact button will act on. A pure core that decides the focus from numbers (pointing, reach and
arc, scoring, a sticky incumbent), a stamped candidate registry, a meaning table, and a game-thread runtime with interactable and
interactor components and a minimal local presentation.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Design: [interaction.md](../../docs/architecture/interaction.md).
Binding rules: [modularity-and-tags.md](../../docs/architecture/modularity-and-tags.md) §3.7, §7.1.
Doc deltas found while implementing: [interaction-doc-deltas.md](../../docs/notes/interaction-doc-deltas.md).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNInteractCore` | exists (Runtime) | Pure types, `UGSTN_InteractMath` (`ComputePointing`, `ScoreCandidate`, `GatherAndScore`, `UpdateFocus`, `IsStillInteractable`), `FGSTN_InteractRegistry` (3D hash grid, stamps, tombstones), `FGSTN_InteractMeaningTable`, the interfaces (`IGSTN_InteractCandidateProvider`, `IGSTN_InteractVisibilityQuery`, `UGSTN_InteractCandidateFilter`), `FGSTN_InteractRules`, the native root tags. No world. Covered by `GSTNInteract.Core.*`. |
| `GSTNInteractRuntime` | exists (Runtime) | `UGSTN_InteractSubsystem` (registry owner, frame source, lookup, WorldStatic line of sight), `UGSTN_InteractableComponent`, `UGSTN_InteractorComponent`, `IGSTN_InteractTarget` / `IGSTN_InteractPointingSource` / `IGSTN_InteractFocusSource`, settings, the rules asset, a minimal presentation (`UGSTN_InteractPresenter`, a debug ring-and-text presenter, `UGSTN_InteractPresentationSubsystem`), console variables. Covered by `GSTNInteract.Runtime.*`. |
| `GSTNInteractEditor` | not built | Validators and gizmos (spec §7). |
| `GSTNInteractCombat` | not built (I2) | A separate plugin: the simulation fragment, tick extension and `TargetKey` stamping. Needs the M6 extension seams. |

## Dependencies

- Plugins: none.
- `GSTNInteractRuntime`: Core, CoreUObject, Engine, GameplayTags, DeveloperSettings, GSTNInteractCore. No UMG: the built-in prompt is
  debug text; a project presenter may use any widget.
- `GSTNInteractCore` (`Build.cs`): Core, CoreUObject, GameplayTags. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
  AbilitySystemSimulation, NetworkPrediction or Mover. `UGSTN_InteractMath` is therefore a plain `UObject` holder of statics, not a
  `UBlueprintFunctionLibrary` (that class lives in Engine).

## Using the core

```cpp
FGSTN_InteractQuery Query;            // plain struct, pointers valid for the call
Query.Frame = Frame;                  // server frame numbers: stamps are compared with it
Query.Origin = Chest;  Query.Up = Up; // Up from the simulation, never world Z
Query.Facing = Facing; Query.StickDirection = Stick; Query.StickMagnitude = Mag;
Query.Rules = &Rules;  Query.Registry = &Registry;  Query.Meanings = &Meanings;
FGSTN_InteractFocusResult Result = UGSTN_InteractMath::UpdateFocus(Query, State);
```

`State` is the only persistent value (`FocusId`, `FocusSinceFrame`, ghost and cycle fields). The result carries the compiled meaning
index to project as the single `Interact.Target.*` tag (0 = focused but not actionable). The same function runs in the game thread
interactor today and in a Network Prediction tick extension later: it reads no world, clock or component.

## Using the runtime

1. Add `UGSTN_InteractableComponent` to an actor Blueprint; set `Meaning` (an `Interact.Target.*` leaf that is listed in
   Project Settings > Plugins > GSTN Interact > Meanings), `DisplayName`, optional verbs, priority, reach, requirement query.
2. Add `UGSTN_InteractorComponent` to the pawn. It probes every tick for the locally controlled pawn and keeps the focus. Call
   `SetInteractHeld(true/false)` on press and release, `Interact(Tap|Hold)` when the action fires.
3. React on the server: bind `OnInteracted` on the interactable, or implement `IGSTN_InteractTarget` on the actor (`CanInteractServer`,
   `OnInteractedServer`). A refused press raises `OnInteractRefused` on the pressing client and changes nothing else.
4. A pawn that owns pointing data implements `IGSTN_InteractPointingSource`; otherwise location, forward and velocity are used.

Level-placed actors register on both machines from frame 0 with a handle hashed from their stable name (nothing replicates).
Spawned actors replicate one stamp (`Handle`, `FromFrame`, `ActionableFrom`), set `MinAgeFrames` ahead of the server frame so a client
has the record before it simulates `FromFrame`; destroying the actor stamps `ToFrame` and leaves a tombstone. Frames come from the
game state's server world time times `FrameRateHz` (62.5).

## Presentation (minimal)

`UGSTN_InteractPresentationSubsystem` is not created on a dedicated server and draws only locally controlled interactors. Each gets
one instance of every class in `PresenterClasses` (default `UGSTN_InteractPresenter_Debug`: a ring at the anchor and the name, verbs
and hold progress as text, through the debug draw helpers, so nothing shows in Shipping). A ghost or an id that resolves to no actor
draws nothing. Subclass `UGSTN_InteractPresenter` in C++ or Blueprint (`OnFocusEntered/Updated/Left`) for a real look, and listen to
`OnPresentedFocusChanged` (tag `Message.Interact.FocusChanged`) for audio. Outline, overlay, ring decal, nearby markers and the style
asset of spec section 5 are not built; no visual polish is planned until the owner asks.

## Console variables

`gstn.Interact.Enable` (0 stops probing), `gstn.Interact.Presentation` (0 hides the prompt, the focus still runs),
`gstn.Interact.Debug` (1 draws every record near each local interactor). Stat group `STATGROUP_GSTNInteract` (`Probe`).

## Settings

Project Settings > Plugins > GSTN Interact: `DefaultRules` (all of `FGSTN_InteractRules`), `Meanings`, `FrameRateHz`, `GridMarginCm`,
`PresenterClasses`. An interactor may name a `UGSTN_InteractRulesAsset`. The plugin ships no ini; defaults are in the code.

## Rules at a glance

- Pointing: stick direction past `StickThreshold`, else the facing; `CameraAim` is off by default. Angles are planar around `Up`.
- Reach and arc: reach to the candidate's surface; the arc narrows from `ArcNearDeg` to `ArcFarDeg`; inside `NearRadiusCm` the angle
  is ignored; a height band along `Up` rejects other floors.
- Scoring: angle, distance and priority terms in 1/4096 fixed point; costs in the same `TieEpsilon` bucket tie, then higher priority,
  then the lower candidate id: a total order, so the result never depends on gather order.
- Sticky focus: relaxed limits for the incumbent, `MinDwellFrames`, a switch margin, a hold lock, immediate replacement of a gone
  incumbent, ghosts for ids the server holds and this machine does not know yet.
- Line of sight: at most `MaxLosTraces` segments, incumbent first, through the injected `IGSTN_InteractVisibilityQuery`.
- Records: immutable, stamped (`FromFrame`, `ActionableFrom`, `ToFrame`), append-only; removal stamps `ToFrame` and leaves a tombstone.

## Gameplay tags

Native: `Interact`, `Interact.Target` (pickers use `Categories="Interact.Target"`) and `Message.Interact.FocusChanged` (local
presentation event). No tag ini ships with this plugin, so nothing registers a tag search path: the leaves (`Grab`, `Mount`,
`Pickup`, `Equipment`, `Objective`) are declared by the plugin that adds the meaning, designer leaves by the content's own ini.

## Tests

`GSTNInteract.Runtime.*` (6 tests, headless world): the replicated stamp and its record, no presentation on a dedicated server, only
local players presented, the game thread focus equal to the core's, line of sight blocked by WorldStatic only (pawns and WorldDynamic
never), settings.

`GSTNInteract.Core.*` (25 tests, no world): pointing, arc, near radius, height band, priority, ties, order independence of the cap,
quantisation, sticky focus, dwell and margin, hold lock, gone incumbent, ghost, stamps, tombstones, duplicates, no flicker among fifty
items, line of sight budget, requirements, cycle skip, server validation, meaning table, rules invariants. Most run with six `Up`
vectors so a hidden world Z fails.

## Known limits (v0.1)

- The runtime registers a record once; a moved actor must call `UpdateRecord` (a removal plus a new record).
- Provider candidates (snapshot providers) have no component: they can be focused, but pressing them is the consumer's to resolve.
- The simulation fragment and the combat bridge (I2) are not built.

- Costs are compared through tie buckets of `TieEpsilon`, so two costs just either side of a bucket edge are ordered by cost even
  when closer than the epsilon (the spec's pairwise tie is not transitive).
- Bit-identical floating point across CPU architectures before the quantisation is assumed, as the hit model does.
- The cap prefilter uses distance only; the arc is checked after the cap.
