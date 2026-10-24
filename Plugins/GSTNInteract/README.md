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
| `GSTNInteractRuntime` | added by I1 | See the Runtime section once it lands. |
| `GSTNInteractEditor` | not built | Validators and gizmos (spec §7). |
| `GSTNInteractCombat` | not built (I2) | A separate plugin: the simulation fragment, tick extension and `TargetKey` stamping. Needs the M6 extension seams. |

## Dependencies

- Plugins: none.
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

`GSTNInteract.Core.*` (25 tests, no world): pointing, arc, near radius, height band, priority, ties, order independence of the cap,
quantisation, sticky focus, dwell and margin, hold lock, gone incumbent, ghost, stamps, tombstones, duplicates, no flicker among fifty
items, line of sight budget, requirements, cycle skip, server validation, meaning table, rules invariants. Most run with six `Up`
vectors so a hidden world Z fails.

## Known limits (v0.1)

- Costs are compared through tie buckets of `TieEpsilon`, so two costs just either side of a bucket edge are ordered by cost even
  when closer than the epsilon (the spec's pairwise tie is not transitive).
- Bit-identical floating point across CPU architectures before the quantisation is assumed, as the hit model does.
- The cap prefilter uses distance only; the arc is checked after the cap.
