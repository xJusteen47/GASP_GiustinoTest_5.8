# ABP_WallSliding (5.7 GaspMoverProject) - text export

Source: `GaspMoverProject/Content/GiustinoMoves/WallSlide/ABP_WallSliding.uasset` (42536 bytes, sha256 prefix CA51BE5C).
`GaspMoverProjectNPP` holds a byte-identical copy (same hash), so there is NO separately owner-modified ABP there; the
Saved/Autosaves `ABP_WallSliding_Auto1` (97 KB, 2026-09-03) is an older/other snapshot and was not exported.
Method: loaded in the 5.9 editor (staged copy, deleted afterwards) and exported with `AssetTools.export_assets` (T3D),
script `Tools/GASNPPTest/abp_export_m1.py`, raw T3D at `Saved/Logs/ci/abp_export/` (UTF-16, not committed).

## Summary: it is an empty shell

- Parent class: `AnimInstance` (no custom C++/BP parent). Target skeleton: `SK_UEFN_Mannequin`.
- Variables: none. Functions: none. Interfaces: none (`BPI_GetAnimInfo` exists next to it but is NOT implemented here).
- State machines: none. Control Rig nodes: none. `CR_WallIkControlRig` is NOT referenced by this ABP
  (the only references in the file are the skeleton and itself).

## AnimGraph (3 nodes, not wired)

| Node | Detail |
|---|---|
| `AnimGraphNode_Slot` | slot node, "Source" pose pin + "Pose" output; default slot name (no SlotName override exported) |
| `AnimGraphNode_Root` | "Result" pin; compiler message: "Result was visible but ignored" (ErrorType 4) |
| `K2Node_CallFunction` | `KismetAnimationLibrary::K2_TwoBoneIK` (RootPos, JointPos, EndPos, JointTarget, Effector, bAllowStretching, StartStretchRatio, MaxStretchScale); inputs/outputs unlinked, a leftover experiment |

No `LinkedTo` between Slot, Root and TwoBoneIK in the AnimGraph: the graph outputs nothing.

## EventGraph

- `Event BlueprintUpdateAnimation` - **disabled** (EnabledState=Disabled).
- `TryGetPawnOwner` - disabled, unconnected.

## Implication for keep/drop

The wall slide look in the 5.7 project comes from the animation assets (`WallSlideIdle`, `Driving_WallSlideIdle` level
sequence, left/right pose assets) and from `CR_WallIkControlRig` driven elsewhere, not from logic inside this ABP.
Nothing in the ABP is worth porting except: a slot node (already provided by the 5.9 layer ABP slots) and the idea of a
two-bone IK pass for hands/feet (curves `hand_l/r_ik_weight`, `foot_l/r_ik_weight` on `WallSlideIdle` suggest the IK was
meant to be curve-weighted). Recommend DROP the ABP, KEEP the anim assets, decide IK via the Control Rig in the new layer.
