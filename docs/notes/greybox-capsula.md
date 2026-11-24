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
- **Owner note 2026-11-24 14:05 (bones only in attacks)**: outside attacks the capsule body is rigid (`PoseWeight` = the attack clip's weight;
  GSTN Greybox Shape blends to the reference pose and moves the crouch squash to the root). The run is read only by a procedural sway
  on the root: footfall bob and nod twice per stride, side rock and nose twist once per stride, cadence = speed / `StrideLength`, amplitude
  by speed. Run start = frantic: for `FranticSeconds` (0.6 s, Toad timing of run-start-frantic) after leaving rest the sway runs at
  `FranticCadenceHz` (6) with `FranticAmplitude` (1.4), then fades to the speed's cadence. `make_capsule_clips.py` now keeps only attack
  overrides (`MOVE_FILTER` Overlord, sequences Jab/Punch/Kick/Attack) and replaces the old map, so hit reactions and rolls are the rigid
  capsule too.
- **Owner note 2026-11-24 14:06**: additive lean in curves (`TurnLeanDegPer1000` 8, cap `MaxTurnLeanDeg` 25, separate from the forward lean)
  and a run stop plant like Toad in Mario 3D World: braking hard (`RunStopDecel` 1200) from a run (`RunStopMinSpeed` 250) kicks the
  pitch spring backward once (`RunStopKickDeg` 22), as if a foot went forward, and the spring brings it back.
- **Owner note 2026-11-24 14:07 (skid, Toad / Luigi reference)**: the skid starts as the run stop (the backward plant kick on the skid's
  first frame), the capsule keeps the old run's facing while the run presentation's `IsSkidding()` lasts (yaw offset from the actor),
  then turns to the new facing on a spring (`SkidTurnFrequency` 4 Hz, `SkidTurnDamping` 0.6, a little overshoot) and starts with the
  frantic cadence. Leans and the plant read the acceleration in that held body frame. `bSkidHoldsFacing` turns it off.
- **Owner note 2026-11-24 14:10**: grey capsule (`MI_GSTN_Greybox_Grey`), red nose (`MI_GSTN_Greybox_Red`), both instances of
  `/Engine/BasicShapes/BasicShapeMaterial` (tint `Color`); the nose is material id 1 and sits in the upper half (about 125 cm of 14..156).

## Desktop run 2 (2026-11-24, round 2, merged up to 2e8e7ee + desktop fixes)
Evidence: `docs/notes/reviews/greybox-capsula/round2/` (videos side | back | game at 1x and 0.25x; `sheet_*.jpg` frames of capture r2_d).
- Build OK, no compile fixes. Tests GSTNAnimation.Core.Greybox + GSTNCombat.NP.Greybox + SpeedGraph: 6/6.
- Asset script fixes: `GREYBOX_REBUILD_MESH=1` did not rebuild (delete_asset left the referenced mesh loaded): now rewrites in place with
  `copy_mesh_to_skeletal_mesh`; `EnableMaterialIDs` is `enable_material_i_ds` in Python (the nose stayed id 0); `BasicShapeMaterial`
  has no SkeletalMesh usage (checker material in game): own parent `M_GSTN_Greybox` (Color -> Base Color, used with skeletal mesh),
  not compiled headless (compiled on load). The headless editor crashes on exit in `FGSTN_DepthAdapterRegistry::Unregister`
  (GSTNDepthMarker module shutdown, after the saves; unrelated).
- make_capsule_clips: PASS, 19 montage overrides + 2 sequence overrides (`AS_OverlordDropkick_Land`, `AS_OverlordDropkick_Loop`).
What the frames show (r2_d, Base 2; r2_c Base 1 same behaviour):
- Grey capsule, red nose cone in the upper half; floats (shadow gap) and tilts as a rigid block (run start v60-84, skid plant v315-320).
- Skid: backward plant, keeps facing, then turns 180 on the spring (nose visible from behind from ~v340).
- Landing squash: visible but mild (v450-462). Crouch: shorter and wider (v468-480), BUT the bottom is flat: the lower hemisphere goes
  through the floor while crouched (bug, v468-480).
- Dropkick: limb markers appear, the capsule does NOT lie near horizontal (stays upright, slight tilt); lumpy surface on the body
  v126-150 (deformation artefact).
- Playtest jab: plays NO montage on the driver (frames.jsonl `montage` null, timeline m6 v491-520) and no "no capsule override" log line:
  the jab is presented without a montage, so neither montage nor sequence overrides can catch it; the capsule stays rigid during it.

## Round 3 (2026-11-24, thread "Test greybox capsule on desktop", from desktop run 2)
- **Crouch through the floor (cause)**: the squash was a non-uniform scale on the root. FTransform has no shear, so a parent's scale
  composes per local axis: the mannequin pelvis/spine bones have local X as the body's up, so (wide, wide, short) on the root became
  (tall, wide, short) on them and the lower hemisphere went under the feet. The same cross-axis scale on the bent spine is the likely
  cause of the lumpy dropkick body (air stretch). Fix in `FGSTN_AnimNode_GreyboxShape`: the root only tilts; the pelvis bone takes the
  whole squash along its own up axis (`UpAxisOf`, `AxisSquashScale`) and its height above the root is scaled by it
  (`PelvisHeightAfterSquash`), i.e. a squash about the ground. Test `GSTNAnimation.Core.Greybox.SquashScale` checks the bottom stays
  above the ground for squash 0.35..1.3.
- **Dropkick upright (cause)**: the move is a 3-sequence montage (Startup, Loop repeated until landing, Land); the loop/land sequence clips
  used the generic keys (rest -> anticipation -> action -> rest), so the body was upright at every loop wrap. Fix: per-sequence clips
  with roles (`segment_role`, `role_keys` in make_capsule_clips.py): startup ends on the action pose, loop holds it, land leaves it.
  `UpdateMoveOverride` now looks up the segment under the montage's time (not only the first) and, for multi-sequence montages, asks
  SequenceMoveOverrides before the montage's own clip; the time is scaled to the capsule clip's length. Dropkick action pose flatter
  (tilt -72, lean -25).
- **Run start sway** (owner 14:32): `FranticAmplitude` 1.4 -> 0.7. If ABP_GSTN_GreyboxCapsule saved its own value, set it there too.
- **Playtest jab**: still to find what presents it without a montage (desktop, editor).
- **Owner 14:38**: "skirts / rolls of flesh" at the end of the dropkick instead of a plain squash: body and nose now rigid on the pelvis
  (`RIGID_BODY`, `GREYBOX_RIGID_BODY=0` restores the spine blend) and the clips put the whole lean on the pelvis (`PELVIS_LEAN_SHARE` 1).
  Hand markers radius 7 -> 10 and red (material id 1). Needs `GREYBOX_REBUILD_MESH=1 GREYBOX_STEPS=mesh` before make_capsule_clips.py.

## Desktop run 3 (2026-11-24, round 3, `_ue59` fast-forwarded to 38c923f3 + merge of 8de77741)
Evidence: `docs/notes/reviews/greybox-capsula/round3/` (videos side | back | game, `*_1x` real speed, `*_025x` 0.25x; `sheet_*.jpg`).
Captures `Saved/Greybox/r3_a` (KIT_Test_Playtest: gb_loco + dropkick_whiff) and `r3_b` (KIT_Test_OverlordDAirV10: dair_whiff), Base 2.
- Build OK, no compile fixes. Tests GSTNAnimation.Core.Greybox + GSTNCombat.NP.Greybox + SpeedGraph: 6/6 (SquashScale included).
- `setup_greybox_assets.py` (mesh, rebuild in place, 624 vertices, materials Body/Nose) and `make_capsule_clips.py`: PASS. Dropkick
  segments: Startup role startup (8 frames), Loop role loop (6), Land role land (15); the montage is found under /Game/GASNPPTest (no
  root added). 19 montage + 20 sequence overrides. `ShapeSettings.FranticAmplitude` on the ABP was already 0.7 (the script now resets a
  saved 1.4; first try read it off the CDO instead of `shape_settings` and failed before saving, fixed in the same commit).
- **Playtest jab (cause)**: Attack.Neutral of KIT_Test_Playtest is `GA_Playtest_GroundHit` (WPN_Test_OverlordDropkick), a Blueprint
  child of `UGSTN_GA_TestHitMove`: timeline of 32 frames with hitboxes only, `Montage` None, no timeline asset, empty event graph. Nothing
  presents it: frames.jsonl of run 2 shows timeline 6 / `Test.State.HitMove` on v495-520 with the hands still (hand_r moves < 2 cm). No
  montage, sequence, anim set or linked layer to hook; the run 1 `AnimMontage_0` was not this move. Options (owner): A give the ability a
  reference montage (e.g. an Overlord jab) so the existing montage override works with no code; B a capsule map keyed by ability class /
  running timeline, timed by the timeline frame (new code + clip).
What the frames show:
- Crouch: shorter and wider, the bottom stays above the floor (v461-485): fixed.
- Dropkick: lies near horizontal from v51 to the landing: fixed. Surface smooth most of the time, but a crease along the top shows late in
  the loop (v126-138) and small dents during the turn (v42-48). Hand markers red and bigger; two grey markers float ~60 cm above the
  body while it lies flat (foot markers where the driver's feet are, not on the capsule).
- Run start sway (v40-94): rigid rocking, amplitude by eye about the same as run 2 (not clearly smaller).
- Jab: the capsule stays rigid (expected, see cause).
- Dair: tilts on the move, red hands shown and hidden, upright again after it.
ESITO: build/test/asset OK; crouch and dropkick pose fixed; jab not hookable (no animation on the driver); dropkick crease and floating
foot markers left.
SUBAGENTI: none.
COSTO: not measured (one thread, ~45 min, captures 144 s + 90 s).

### Playtest jab, owner 2A (same day)
`Tools/GAS/greybox/set_playtest_jab_montage.py` (Save-Assets, backup in Saved/Backups/GAS) copies TL_Overlord_Jab1's montage into
GA_Playtest_GroundHit's timeline: `AM_OverlordJab1V3`, no section, rate 1.0417 (62.5/60). Montage 0.467 s = 0.448 s at that rate =
28.0 sim frames against the ability's TotalFrames 32 (0.512 s): the montage ends 4 frames before the move. Hitboxes, frames and cancel
window unchanged. The capsule override for AM_OverlordJab1V3 already existed (no make_capsule_clips rerun).
Capture `Saved/Greybox/r3_jab` (jab1_long, Base 2): the driver plays AM_OverlordJab1V3 from v22 (frames.jsonl), the capsule tilts back
then forward and the red hand marker grows and swings forward (v24-46), then fades. Evidence `round3/jab_*.mp4`, `sheet_jab_2A_side.jpg`.
The hitbox debug draw does not show in the scene captures, so "hitboxes unchanged" rests on the untouched timeline data, not on the video.
Note "play timeline": Play Combat Timeline And Wait / `PlayableTimelines` (move-timeline-decoupling 1A) plays a timeline asset, and the
montage it shows is still the timeline's Montage field; with none set, nothing is presented, so it did not cover the jab by itself.

## Round 4 (2026-11-24, owner notes 14:32 to 14:45)
- Run start: the frantic start sets the sway amplitude (`SwayAmplitudeTarget`, lerp) instead of a max with the speed's; with the fast
  ramp the speed reached full inside the frantic window, so FranticAmplitude had no effect (run 3 video).
- Dropkick action pose: legs in line with the lying body (leg 12/8, was 95/90: feet floated ~60 cm above it).
- Jump squat: squash to `JumpSquatSquash` (0.72) during the sim's squat frames (`IsAnimOwnerJumpSquatting`), released into the air
  stretch on take-off. Landing kick `LandSquashPer1000` 0.45 -> 0.6.
- Brakes plant against the motion (`PlantLean`, `KickPlant`): any hard brake along the ground velocity (forward, sideways, backward)
  tips the top against the motion, "as if the leg on the side of the motion went in front".
- Wall: modes `WallModeNames`; braced like on the ground, top leans `WallLeanDeg` off the wall (normal from 4 presentation traces,
  `FindWallNormal`), friction rattle with the slide speed, wall skid = plant on a hard brake along the wall.
- Kit look (`UGSTN_GreyboxSubsystem`): `SM_GSTN_Greybox_Collar` (half ring behind the upper back, colour `ColourForKit`, i.e. the kit's
  stand-in box colour) and `SM_GSTN_Greybox_Greatsword` (~126 cm, on the back) for kits whose name contains `gstn.Greybox.SwordKits`
  (default "Overlord,Playtest"). Built by setup_greybox_assets.py step `accessories`, attached to the capsule's pelvis.
- Moves: Jab2 (other hand), DAir landings (crouched land), dropkick tech bounce (dropkick then a tucked bounce).
