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

## Dependencies

- Plugins: none.
- `GSTNDepthMarkerCore` (`Build.cs`): Core, CoreUObject, GameplayTags. Never GASP_Giustino2, another GSTN plugin, GameplayAbilities,
  AbilitySystemSimulation, NetworkPrediction or Mover.

## Gameplay tags

None declared (spec §6.4). The target filter reads `Surface.Behaviour.*` through a `FGameplayTagQuery` the caller supplies.

## Tests

`GSTNDepthMarker.Core.*`: every test runs with several Up vectors (flipped, sideways, diagonal) so a hidden world-Z assumption fails.

## Next steps

Runtime module (subsystem, component, presenters), Editor module, `GSTNDepthMarkerMover` adapter plugin, `GSTNDepthMarkerSurface` and
`GSTNDepthMarkerMinion` bridges, the test map, harness scenarios, spikes D1-D3.
