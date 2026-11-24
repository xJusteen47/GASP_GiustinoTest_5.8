# Greybox capsule and simple locomotion graph (owner 2026-11-24: 1A 2A 3A 4A)

Thread "Prototype run start feel without animations". Scope: ALL movement and moves; the run start is the main example.
Principle agreed with the owner: the feel is found on the speed curve and the frame data (Mover, run stages, timelines); the animation
must only not contradict it. Stop searching the run start feel inside the clip.

## Decisions
- **1A**: the GASP stock locomotion (`gstn.Loco.Base 1`, already the default) is the placeholder; tune the curve: apply the ready ramp
  (`Tools/GAS/setup_run_start_ramp.py`, owner-local `DA_Locomotion_Default`, never committed) and read it on screen with
  `gstn.Debug.SpeedGraph 1` (speed over time, t50 / t90 of the last start from rest).
- **2A**: greybox capsule with a nose + simple chooser-driven graph + warping per row.
- **3A**: Claude generates the capsule clips by script as blocking (2-4 key poses on pelvis / spine / head): locomotion set as chooser
  rows; the Overlord moves as VISUAL montage overrides of the capsule skin (same sections and length; hitboxes keep reading the reference
  montage, the anim set validator checks the timing).
- **4A**: capsule placeholder clips are exempt from the level-2 animation review (rule 6); the owner looks at them in game. Rule 6 stays
  full for real animations.

## Technical choices (reversible, Claude, rule 4)
- **D1** The capsule mesh is on the **mannequin Skeleton**, skinned to a handful of bones (root -> pelvis -> spine_01/03/05 -> neck_01 ->
  head, markers rigid on hands and feet), and follows the hidden driver through **Copy Pose From Mesh** (by bone name). It is the
  "automatic retarget" the owner asked for without IK Rig / Retargeter assets: Copy Pose needs the same parent chain, which a subset of the
  mannequin gives; and a clip made for the capsule is a mannequin clip, valid later for real characters. If proportions ever need to
  differ, swap the node for Retarget Pose From Mesh + an IK Retargeter (only the capsule ABP changes).
- **D2** The capsule body bottom is weighted to `root` (the ground): a lower pelvis (crouch, landing) squashes the capsule by itself.
  On top, `GSTN Greybox Shape` scales and tilts the root by springs (air stretch, landing squash kick, squash on a speed gain, lean into
  the acceleration). Tuning: class defaults of `ABP_GSTN_GreyboxCapsule` (`ShapeSettings`).
- **D3** Simple graph: the C++ decides the state (`EGSTN_SimpleLocoState`), the ABP machine switches on it with crossfade 0, each state is a
  `GSTN Chooser Player` on the same chooser (first column LocoState). The row gives the clip, start time, rate, blend (inertial), reselect,
  and the warping targets. Why a node and not GASP's Blend Stack + On State Entry: scripts cannot author per-state function bindings.
- **D4** Warping: Orientation / Stride Warping after the slot, settings copied from the stock ABP, Graph mode, alphas bound to the instance
  (targets reset to 0 on each state change: warping only where a row asks). Stride warping in Graph mode measures the clip's root motion:
  our clips have none, so it may need Manual mode with a stride scale; check in PIE.

## What exists (branch claude/project-thread-mswztw)
| Piece | Where |
|---|---|
| `FGSTN_AnimNode_ChooserPlayer` (+ editor node), `FGSTN_AnimNode_GreyboxShape` (+ editor node), `IGSTN_GreyboxShapeSource` | GSTNAnimationCore / Editor (plugin now depends on engine plugin Chooser) |
| Authoring: `SetNodeProperty`, `BindNodePinToProperty`, `CopyNodeSettingsFrom`, `FindAnimGraphNodes` | `UGSTN_AnimGraphAuthoringLibrary` |
| `UGSTN_SimpleLocoAnimInstance` (+ pure `FGSTN_SimpleLocoLogic`), `UGSTN_GreyboxAnimInstance` (+ springs) | GSTNCombatNP Presentation |
| `gstn.Loco.Base 2` (simple graph), `gstn.Greybox 0/1` (`UGSTN_GreyboxSubsystem`) | game module, Animation/ |
| `gstn.Debug.SpeedGraph` | GSTNCombatDebug (DebugDraw) |
| Assets script | `Tools/GAS/greybox/setup_greybox_assets.py` (GREYBOX_STEPS, GSTN_SAVE=1 via Save-Assets.ps1) |
| Tests | `GSTNAnimation.Core.Greybox.*`, `GSTNCombat.NP.Greybox.*`, `GSTNCombat.Debug.SpeedGraph.*` |

## Desktop steps (Remote Control, `_ue59` lock)
1. Build (`Tools/CI/Run-Timed.ps1 -Preset Build -WaitMutex -NoHotReload`), tests `GSTNAnimation.Core.Greybox`, `GSTNCombat.NP.Greybox`,
   `GSTNCombat.Debug.SpeedGraph` (+ the GSTN suite as usual).
2. `Save-Assets.ps1 -Script Tools\GAS\greybox\setup_greybox_assets.py -Tag greybox` (editor closed). The mesh step needs the
   GeometryScripting plugin (editor only); the script reports the API it finds when a call is missing.
3. PIE: `gstn.Greybox 1`, `gstn.Loco.Base 2`, `gstn.Debug.SpeedGraph 1`; capture run start, stop, skid, jump, crouch, one attack.
4. Then 3A: capsule clips (next section of this note, to write when 1-3 pass).
