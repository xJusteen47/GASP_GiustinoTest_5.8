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

## 3A: capsule clips (`Tools/GAS/greybox/make_capsule_clips.py`, after setup_greybox_assets.py)
- Locomotion `AS_GSTN_Greybox_<State>` (Idle, RunStart, Run, RunStop, Skid, Air, Crouch): blocking from numbers (2-4 key poses, smoothstep),
  body chain + light arms/legs so the markers read; written into the matching rows of `CHT_GSTN_SimpleLoco`.
- Moves `AS_GSTN_Greybox_Move_<montage>`: one per reference montage whose name contains Overlord / React / Roll / Dodge (MOVE_FILTER), same
  length; timing from length shares per move kind (jab, ftilt, dropkick, dair, hit, roll, generic). Listed in
  `ABP_GSTN_GreyboxCapsule` > `MoveOverrides`: the capsule shows the clip at the driver montage's time and weight
  (`UGSTN_GreyboxAnimInstance::UpdateMoveOverride`, blend in `GSTN Greybox Shape`); the driver keeps the reference montage, so hitboxes
  and bone-following hurtboxes do not change.
- 4A: placeholders, no level-2 review; the owner looks at them in game. Retouch: edit the key poses at the top of the script and rerun, or
  edit the clips by hand (a rerun overwrites them: rename a clip you hand-edit).

## Desktop run 2026-11-24 (thread "Prototype run start feel without animations", `_ue59`)
Status: build, tests, assets and the capsule clips work; first in-game video below. Evidence: `docs/notes/reviews/greybox-capsula/`
(`*_1x.mp4` real speed, `*_025x.mp4` 0.25x; side | back | game, frame label bottom left).
- **Build** fixes (UE 5.9): `GetAnimClass()` is non-const (read `AnimClass`), bitfields passed by value to `Emplace`, mission test tag
  helpers return by value (C4172 as error). `GeometryScripting` enabled in the .uproject, `TargetAllowList` Editor (reversible).
- **Tests**: Greybox / SpeedGraph 6/6. Full GSTN suite 1880/1883: `ChooserPlayerTime` (float tolerance, fixed) and 2 Loadout screen tests
  (`GSTNCombat.GAS.Loadout.Screen`, `.ScreenSupportAndSuper`) that fail because the Backpack / KitMods plugins register global loadout
  rows: pre-existing (M11), not greybox.
- **Assets**: `SK_GSTN_GreyboxCapsule` (624 vertices, manual weights; needed `copy_bones_from_skeleton` + `mesh_create_bone_weights`
  first), `ABP_GSTN_GreyboxCapsule`, `CHT_GSTN_SimpleLoco` (7 rows), `ABP_GSTN_SimpleLoco` (state machine, inertialization, slot,
  orientation + stride warping copied from the stock ABP). The chooser toolset functions are toolset-only: Python calls them with
  `call_method`. Warning on compile: the editor nodes live in an `Editor` module, UE wants `UncookedOnly` (works in editor/PIE; must move
  before any packaging).
- **Clips**: 7 locomotion clips in the chooser rows, 23 move clips in `MoveOverrides` (`check_greybox_assets.py` confirms the CDO and the
  clip rotations). Every combat montage uses `DefaultSlot`: no extra slot in the simple ABP.
- **1A ramp** applied to the owner-local `DA_Locomotion_Default` (stage 1 `AccelRampFrames` 20 -> 62; restore 20). Not committed.
- **Speed graph**: fixed a double-speed spike (62.5 Hz sim in 60 fps frames) that broke t50/t90; now measured over 0.1 s. Run start from
  rest on the capture: plateau ~576 cm/s, t50 0.45 s, t90 0.80 s (graph) / 0.47 / 0.70 s (computed from the capture rows).
  Only `Shot showui` captures it (`PIE_CAPTURE_SHOT_UI=1`); HighResShot and the scene captures leave the canvas out.

What the video shows (watched frame by frame):
- The capsule follows the hidden driver everywhere (run, stop, skid turn, jump, crouch, moves); the mannequin is not visible.
- Lean: clear forward bend on the run start and the run (spine chain), back bend in the skid and the dropkick. It bends like a banana
  rather than tilting as a block (the bottom is weighted to the ground). Squash on landing/crouch is weak: the crouch reads as a curl, not
  as a shorter capsule.
- Nose: points the facing; it swings round in the skid (back view f316-324).
- Moves: the dropkick and the dair play the capsule clips (pelvis stays level, feet markers go forward/tuck; the real montage would tilt the
  body). The jab of KIT_Test_Playtest plays a dynamic montage (`AnimMontage_0`, no asset), so it has no override and the capsule copies the
  real jab pose. The blocking moves read weakly on a capsule (only the markers show the limbs).
- Hitboxes stay on the hidden mannequin (dropkick foot sphere drawn where the driver's foot is, not on the capsule markers).
- Stride warping in Graph mode: not judged (capsule feet are markers); the clips have no root motion, so Graph mode is likely a no-op.

Left: Playtest jab override (dynamic montage: key the override by sequence too, or play the jab from an asset); stronger squash/tilt on
the capsule if the owner wants the shape to read more; editor nodes to an UncookedOnly module before packaging; the 2 Loadout tests.

## Round 2 (2026-11-24, from the desktop run findings)

All reversible; class defaults, so the owner retunes them on ABP_GSTN_GreyboxCapsule.
- **Capsule hangs from the pelvis** (`setup_greybox_assets.py`, `GREYBOX_REBUILD_MESH=1 GREYBOX_STEPS=mesh`): every vertex under the pelvis
  is rigid on the pelvis, so the lower body tilts with it (no more banana, a dropkick can lie flat).
- **Ground squash from the pose**: `UGSTN_GreyboxAnimInstance` reads the driver's pelvis height (component space, above the feet) over its
  reference height on the ground (`PelvisSquash`, floor `MinPelvisSquash` 0.35, blend `PelvisSquashBlendSpeed` 20/s). GSTN Greybox Shape
  scales the pelvis along its reference up axis by it, faded by how upright that axis is (lying flat = no squash), so a crouch or a landing
  is a shorter capsule that still touches the ground.
- **Stronger defaults**: AirStretch 0.2 (max 1.3), LandSquash 0.45, AccelSquash 0.06, MinSquash 0.6, Lean 5 deg per 1000 (max 18).
- **Clips**: the pelvis takes `PELVIS_LEAN_SHARE` (0.35) of every lean, and a new `tilt` key tilts the whole body on the pelvis (dropkick
  -60 + lean -30, roll 50..70).
- **Dynamic montages**: `SequenceMoveOverrides` (sequence -> clip) is asked when a montage has no own entry, at the segment's time. The
  script fills it with every montage's sequence, the AnimSequences matching `SEQUENCE_FILTER` (Jab, Punch, Kick, Attack) and `SEQUENCES`;
  the capsule logs `GSTN Greybox: no capsule override for montage ... playing <path>` once per uncovered clip.
- **Owner note 2026-11-24 14:02**: the capsule floats (bottom 14 cm above the feet), its top stays under the collision capsule's top (156 cm),
  it is chubbier (radius 34); `GREYBOX_SHAPE="radius,bottom,top"` overrides. The hand and foot markers are hidden in locomotion and shown
  while the driver plays a montage (`bShowLimbsInMoves` / `bShowLimbsInLocomotion`, `LimbBlendSpeed`; GSTN Greybox Shape scales the
  `LimbBones` by `LimbScale`).
