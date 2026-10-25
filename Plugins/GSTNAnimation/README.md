# GSTNAnimation

Feature animation decoupled from the main anim graph, for Unreal Engine 5.8. A feature (a wall mode, a spin, a drill) brings its
animation as a **linked anim layer** chosen by data; the character's main graph never names it. A skin, or a trait of it
(male / female body, heavy / slim build), can pick a **variant** of some features' animations, while the gameplay (timelines,
hitboxes, hurtboxes, baked sockets, movement) stays identical: variants are presentation only.

Status: **experimental (0.1)**, `EnabledByDefault: false`. Design: [anim-layers.md](../../docs/architecture/anim-layers.md).
First user: the wall layer of the project ([wall-anim.md](../../docs/architecture/wall-anim.md)).

It also has **limb contact IK**: a Control Rig node that pulls a hand or a foot onto a nearby surface when the clip already brings it close
(wall slide, climb). Design: [limb-contact-ik.md](../../docs/architecture/limb-contact-ik.md).

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNAnimationCore` | exists (Runtime, Default) | The override layer seam, the anim set data, the pure resolution and the component that applies it; the limb contact data, rules and probe. No GAS, Network Prediction or Mover. Covered by `GSTNAnimation.Core.*`. |
| `GSTNAnimationRig` | exists (Runtime, Default) | Control Rig units: `GSTN Limb Contact IK`. Depends on ControlRig, RigVM, AnimationCore and `GSTNAnimationCore`. |

## Classes

| Class | Kind | Role |
|---|---|---|
| `IGSTN_OverrideLayerSource` | C++ interface | A linked layer says whether it wants the pose now, with which blend time and priority. |
| `UGSTN_CharacterAnimInstance` | `UAnimInstance` | Parent for the main graph: polls the linked layers, exposes `bOverrideLayerActive`, `OverrideBlendTime`, `ActiveOverrideLayerClass`. |
| `UGSTN_AnimSetDefinition` | primary data asset `GSTN_AnimSet` | One feature slot (`SetTag`, `Anim.Set.*`): `LayerClass` and `MontageOverrides` (`Reference` -> `Visual`). |
| `FGSTN_AnimSetRule`, `UGSTN_AnimSetCollection` | data (`GSTN_AnimSetCollection`) | "Use this set when the cosmetic tags match `When`", with a priority; one collection per source (character, kit, weapon, skin). |
| `UGSTN_AnimSetUtils` | function library | `ResolveWinners`, `ResolveLayerClasses`, `ValidateMontageOverride`. Pure and deterministic. |
| `UGSTN_AnimSetComponent` | actor component | Holds the sources and cosmetic tags of one character, loads asynchronously, links the winning layers on the mesh, answers `ResolveVisualMontage`. |
| `FGSTN_LimbSurface`, `FGSTN_LimbContactSettings`, `FGSTN_LimbProbeSettings` | data | A surface (point, normal) a limb may touch; how one limb attaches; how the game thread probes. |
| `UGSTN_LimbContactUtils` | function library | `ComputeContact` (gap, target, weight), `StepContactAlpha`, `AlignAxisToSurface`, `InverseTransformSurface`, `ProbeSurface` (game thread). |
| `FGSTN_RigUnit_LimbContactIK` | Control Rig node "GSTN Limb Contact IK" | Two-bone IK of one limb onto its surface, smoothed weight, palm/sole alignment. One node per limb. |

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

- Plugins: ControlRig (engine, enabled by default).
- `GSTNAnimationCore` (`Build.cs`): public Core, CoreUObject, Engine, GameplayTags; private Projects (finds the plugin's `Config/Tags`).
- `GSTNAnimationRig` (`Build.cs`): public Core, CoreUObject, Engine, ControlRig, RigVM, GSTNAnimationCore; private AnimationCore.
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
- **Limbs touching another surface** (ledge, ladder): in that feature's layer instance, fill one `FGSTN_LimbSurface` per limb in component space
  (`UGSTN_LimbContactUtils::ProbeSurface` + `InverseTransformSurface` on the game thread), and end its layer graph with a Control Rig of
  `GSTN Limb Contact IK` nodes reading them. The wall layer is the example.

## Using it in another project

Enable the plugin, reparent the main anim graph to `UGSTN_CharacterAnimInstance` (it adds nothing else), add `UGSTN_AnimSetComponent`
to the character with a default collection, and call `SetSource` / `SetCosmeticTags` where the game applies its loadout and skins.
