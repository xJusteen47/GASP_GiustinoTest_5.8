# GSTNAnimation

Feature animation decoupled from the main anim graph, for Unreal Engine 5.8. A feature (a wall mode, a spin, a drill) brings its
animation as a **linked anim layer** chosen by data; the character's main graph never names it. A skin, or a trait of it
(male / female body, heavy / slim build), can pick a **variant** of some features' animations, while the gameplay (timelines,
hitboxes, hurtboxes, baked sockets, movement) stays identical: variants are presentation only.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Design: [anim-layers.md](../../docs/architecture/anim-layers.md).
First user: the wall layer of the project ([wall-anim.md](../../docs/architecture/wall-anim.md)).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNAnimationCore` | exists (Runtime, Default) | The override layer seam, the anim set data, the pure resolution and the component that applies it. No GAS, Network Prediction or Mover. Covered by `GSTNAnimation.Core.*`. |
| `GSTNAnimationEditor` | exists (Editor, Default) | `UGSTN_AnimGraphAuthoringLibrary`: anim graph authoring for Python scripts (layer interfaces, state machines, rules on an enum through Property Access, sequence players, the override-layer blend in a main graph, child asset overrides, a text dump of the graphs and a compile report). Knows no feature; nothing at runtime depends on it. Used by `Tools/GAS/setup_wall_anim_layer.py`. |

## Classes

| Class | Kind | Role |
|---|---|---|
| `IGSTN_OverrideLayerSource` | C++ interface | A linked layer says whether it wants the pose now, with which blend time and priority. |
| `UGSTN_CharacterAnimInstance` | `UAnimInstance` | Parent for the main graph: polls the linked layers, exposes `bOverrideLayerActive`, `OverrideBlendTime`, `ActiveOverrideLayerClass`. |
| `UGSTN_AnimSetDefinition` | primary data asset `GSTN_AnimSet` | One feature slot (`SetTag`, `Anim.Set.*`): `LayerClass` and `MontageOverrides` (`Reference` -> `Visual`). |
| `FGSTN_AnimSetRule`, `UGSTN_AnimSetCollection` | data (`GSTN_AnimSetCollection`) | "Use this set when the cosmetic tags match `When`", with a priority; one collection per source (character, kit, weapon, skin). |
| `UGSTN_AnimSetUtils` | function library | `ResolveWinners`, `ResolveLayerClasses`, `ValidateMontageOverride`. Pure and deterministic. |
| `UGSTN_AnimSetComponent` | actor component | Holds the sources and cosmetic tags of one character, loads asynchronously, links the winning layers on the mesh, answers `ResolveVisualMontage`. |

## How it resolves

```
for each slot (Anim.Set.*): competing rules = sets of every source whose When matches the cosmetic tags
winner = highest SourcePriority, then highest rule Priority, then first listed
layer class = the winner's, or the best competing set that has one (a montage-only variant keeps the base layer)
montages: the winner's overrides only; ResolveVisualMontage(Reference) returns Reference when none
```

Default source priorities (`GSTNAnimSet`): character 0, kit 100, weapon 200, skin 300.

## Hitboxes stay the same

1. Simulation never calls the component; timelines, hit/hurt windows, baked sockets and Mover read only the reference montage.
2. Only the code that plays a montage on the mesh asks `ResolveVisualMontage`.
3. Data validation of `UGSTN_AnimSetDefinition`: each override must keep the reference's section names, section lengths (within one frame
   at 30 fps), rate scale, and add no root motion.

## Dependencies

- Plugins: none.
- `GSTNAnimationCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags; private Projects (finds the plugin's `Config/Tags`).
- `GSTNAnimationEditor` (`Build.cs`): public Core, CoreUObject, Engine; private UnrealEd, BlueprintGraph, AnimGraph, AnimGraphRuntime, KismetCompiler.
  The Property Access node of a rule (plugin PropertyAccessNode, class private to its module) is created by class name with its `Path` set by reflection.
- `GSTNMotionToolsEditor` (`Build.cs`, editor, PostEngineInit): public Core, CoreUObject, Engine; private InputCore, Json, PythonScriptPlugin,
  Slate, SlateCore, UnrealEd, WorkspaceMenuStructure. Plugin dependency: PythonScriptPlugin (the panel runs `Content/Python/gstn_motion_tools`).
- Dependents: the project module (wall layer). Planned: the loadout apply path (kit / weapon / skin collections), spin and drill.

## Gameplay tags

| Tags | Source | Declared in |
|---|---|---|
| `Anim`, `Anim.Set`, `Anim.Trait` (roots) | native | `Public/GSTN_AnimTags.h`, namespace `GSTNAnimTags` |
| `Anim.Set.<Feature>` leaves | native, in the feature's own module (`Anim.Set.Wall`: project, `GSTN_AbilityTags.h`) | |
| `Anim.Trait.Body.Male`, `.Body.Female`, `.Build.Heavy`, `.Build.Slim` | ini examples | `Config/Tags/GSTNAnimation.ini` (registered by the module) |

## How to extend

- **New feature with its own animation**: declare an `Anim.Set.<Feature>` leaf in the feature's module, make a layer interface
  (`ALI_*`) and a layer anim Blueprint whose C++ parent implements `IGSTN_OverrideLayerSource`, add one Linked Anim Layer node for that
  interface in the main graph, and a set + rule in the character's default collection.
- **Variant for a skin trait**: make a child anim Blueprint of the base layer that swaps clips, a set with the same `SetTag`, and a rule in
  the skin's collection with `When` = the trait (e.g. `Anim.Trait.Build.Heavy`). No code.
- **New trait**: a new `Anim.Trait.*` leaf in the game's `Config/Tags` ini, added to the skin's tags.

## Using it in another project

Enable the plugin, reparent the main anim graph to `UGSTN_CharacterAnimInstance` (it adds nothing else), add `UGSTN_AnimSetComponent`
to the character with a default collection, and call `SetSource` / `SetCosmeticTags` where the game applies its loadout and skins.

## Motion Tools (authoring, editor only)

Non-destructive helpers for move animation on a Level Sequence with a Control Rig track (Window > GSTN Animation > Motion Tools).
Each operation writes only its own **additive section** ("layer") on the Control Rig track, one row each; the base section is never
written, every layer can be switched off, re-weighted, hand-edited or removed, and re-applying rewrites the same channels.

- **Overlap** (follow-through) on a chain (Testa, Busto, Braccio, Polso, Mantello, Coda, or the viewport selection): preset
  (Leggero, Pesante, Frusta, Rigido) + three sliders (Ritardo in frames at the chain tip, Morbidezza 0..1, Rimbalzo 0..1), weight, mode
  (Molla = damped spring; Sfasamento = the motion replayed later down the chain). Key protection: weight 0 on the base key frames, the
  first and the last frame (on a baked clip, keyed every frame, give the owner key frames instead). Optional floor guard: the layer may
  not push the chain's collision spheres (radii of the runtime ground-contact fix) under a plane.
- **Easing** presets on a key interval of the selected controls (Lineare, Morbido, Scatto, Anticipo, Overshoot, Assestamento), on the
  Easing layer under Overlap. The interval's end values stay.
- The same operations from Python and MCP: `import gstn_motion_tools as mt; mt.overlap(chain="testa", preset="pesante")`,
  `mt.easing([...], 10, 16, "scatto")`, `mt.layer("Overlap", active=False)`, `mt.describe()`; toolset `GSTNMotionToolsToolset` when the
  editor's ToolsetRegistry is present. Settings applied are stored in the sequence's metadata tag `GSTN.MotionTools`.
- Code: math `Content/Python/gstn_motion_tools/core.py` (no `unreal`), operations `api.py`, Sequencer I/O `sequencer_io.py`, panel bridge
  `panel.py`, in-editor smoke test `selftest.py`; cloud tests `tests/` (`python -m unittest gstn_motion_tools.tests.test_motion_tools
  gstn_motion_tools.tests.test_editor_glue` from `Content/Python`). Panel: `Source/GSTNMotionToolsEditor`.
- It is a speed-up tool used where it fits (owner, 2026-11-17): hand-keyed curves stay where it is not enough.
