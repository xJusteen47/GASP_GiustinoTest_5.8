# GSTNPlatformParts

The bridge between destructible parts (`GSTNParts`) and enemy platforms (`GSTNPlatform`): a platform shape named after a body part
goes away when that part, or an ancestor of it, is broken, with the shape's Breaking telegraph, and comes back when the part is
restored (traversal-surfaces.md 4.4, TS.5). Neither plugin names the other; removing this one leaves both building and working.

Status: **experimental (0.1)**, `EnabledByDefault: false` (enabled in this branch's `.uproject`). Checkpoint T6 of
[traversal-surfaces-plan.md](../../docs/architecture/plans/traversal-surfaces-plan.md).

## Module

`GSTNPlatformParts` (Runtime). Public: Core, CoreUObject, Engine, GSTNPlatformCore, GSTNPlatformRuntime. Private: GameplayTags,
GameplayAbilities, NetworkPrediction, GSTNPartsCore, GSTNPartsNP, GSTNCombatCore, GSTNCombatNP. Never the project module, an editor
module or the parts presentation.

## How it works

`UGSTN_PlatformPartsComponent` ("GSTN Platform Parts") sits next to a GSTN Enemy Platform component on a fighter whose part set is
attached (GSTN Parts Attach component or `FGSTN_PartsAttach`). At the platform's pose point, before every frame posed for the first
time (`UGSTN_EnemyPlatformComponent::OnPrePose`, never on a re-simulated frame), it:

```
set = FGSTN_PartsAttach::FindSet(owner)                   // re-resolve when the attached set changes (kit swap)
gates[shape] = bit(part named by shape) | bits(its ancestors)   // FGSTN_PlatformPartGates (GSTNPlatformCore), 0 = body
broken = host: owner combat sync state -> FGSTN_PartsFragment.BrokenMask (end of frame)
         client: the host's {BrokenMask, Frame} from the bridge          // confirmed values only
mask = shapes whose gate has no broken bit
if mask changed: platform.SetShapeMaskAtFrame(mask, bTelegraphBreak ? Breaking : Inactive, Frame)
```

The platform stamps the change on the frame being posed and keeps it as a timeline, so a replayed frame finds the collision it had.
A broken part's shape stays solid as `Breaking` for its `DropGraceFrames`, then goes; a restore clears the bit and the shape is `Active`
at once. Before a set is attached, and for a name the set lacks, a shape is body.

## Behaviour notes

- **Mask source (T6).** A client does not read its own copy of the break. The host reads the fragment at its pose point and replicates
  `{BrokenMask, Frame}` on the bridge; each client applies it to that frame with `SetShapeMaskAtFrame`. A mask that arrives within the
  shape's `DropGraceFrames` costs no correction, because the Breaking telegraph keeps the past frames solid.
- **Restore.** The bridge does not listen to `Combat.Event.Part.Restored`. It follows the broken mask, and the cleared bit makes the shape `Active`.
- **Seams used from GSTNPlatform:** `OnPrePose`, `SetShapeMaskAtFrame` and `FGSTN_PlatformPartGates`.

## Using it

1. Enable the plugin. On the fighter: GSTN Enemy Platform (shapes with `PartName`), GSTN Parts Attach (or a kit with a part set),
   GSTN Platform Parts.
2. Validation: a part name the set lacks is an **error** here (the platform plugin's own "no bridge" warning goes away when this
   module is loaded). The set checked is `ValidationPartSet`, else the attach component's `DefaultPartSet`.

## Console commands (not in Shipping)

- `gstn.PlatformParts.AttachTest [PartName=Flank] [ShapeId=0]`: every platform of every game world gets a one-part test set and a
  bridge, shape ShapeId naming the part (the harness's test fighter has no combat component).
- `gstn.PlatformParts.SetBrokenMask Mask`: the mask every bridge reads instead of the combat state; -1 goes back to it.

## Tests

`Automation RunTests GSTNPlatformParts` (5): `MaskFollowsBrokenMask`, `RestoreReenables`, `ChildrenDropWithParent`, `GraceBeforeOff`,
`UnknownPartNameIsError`. PIE: `python Tools/GASNPPTest/surface_scenarios.py --phases part` (`scenario_surface_part_break`).

## Known limits (v0.1)

- The scenario breaks the part through `SetBrokenMask` on every machine at once, not through a hit replicated to the client: the
  harness fighter has no combat state. The hit-driven break and the healer restore (P7b) need a combat fighter with a flank (T9 content).
- A fighter predicted locally (a player-controlled enemy) would feed its predicted mask: none exists today.

## Changelog

- 0.1 (2026-11-06): bridge component, part gates, debug commands (plan T6).
