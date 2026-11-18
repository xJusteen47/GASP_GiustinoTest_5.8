# Skid in the owner's UE 5.7 project: how it was wired, and how to copy it into ABP_GSTN_Locomotion (5.9)

Thread 2026-11-18, desktop, read only (no editor). Source: `C:\Users\jacop\Documents\Unreal Projects\GaspMoverProject` (EngineAssociation 5.7).
Method: C++ read directly; Blueprints read from the Find-in-Blueprints cache inside the .uasset (UTF-16, each char +1, decoded
with a 10-line Python script), plus name tables (`strings`). Pin links and exact bool values are NOT in that cache: items marked
(?) are inferred.

## What the 5.7 project has

| Piece | Asset | What it does |
|---|---|---|
| Sim (C++) | `Source/GaspMoverProject/.../AdditionalMoves/GiustinoAdditionalMoves.{h,cpp}` | `SandboxCharacter_Mover` calls `TryQueueSkidFromInput(Get_MoveInput)`. If ground speed >= 80 and dot(velocity, input) <= -0.2 it queues `FGiustinoSkidLayeredMove`: AdditiveVelocity, priority 10, 450 ms, adds -dir(v) * min(|v|, 2.75 |v|) = -v, so it cancels the ground velocity while input stays opposite. Cancelled when input stops or is no longer opposite. **It never touches orientation**: the capsule keeps turning toward the input with the normal walking mode. |
| Old BP path | `GiustinoMoves/AC_GiustinoEnhanchedMoves` | "Check if should skid" (MinSpeedToSkid, MinSkidDot) sets ShouldSkid, exposed by BPI `Get_EnhanchedMovementProperties`. Also a Play Montage (Mover Actor) with `SS_180_Turn_Montage` + LayeredMove_AnimRootMotion (AdditiveVelocity, prio 2, finish SetVelocity = SpeedAfterSkid) + AddOrUpdateWarpTarget. Whether the montage branch still runs (?) cannot be told offline; the ABP does not depend on it. |
| ABP flag | `Blueprints/SandboxCharacter_Mover_ABP` event graph | Computes its own `bIsSkidding`: dot(velocity, acceleration) < **-0.8** plus a Gait check; `SkidAlpha` via FInterpTo / Select Float. |
| State machine | same ABP, GASP experimental SM | States `Transition to Skid` -> `Skid Loop` -> `Skid End` (enum `E_ExperimentalStateMachineState` NewEnumerator12/13, set by `OnStateEntry_TransitionToSkid` / `OnStateEntry_SkidLoop`). The asset still carries the compiler warning "Transition to Skid -> Skid Loop will never be taken": in practice one clip plays in Transition to Skid, then the SM leaves. |
| Chooser | `ExperimentalStateMachineData/CHT_MoverCharacterAnimations` | Nested chooser row **"Skid"** whose only asset is **`GiustinoMoves/Skid/SS_180_Turn2`**, fed to the State Machine Blend Stack Input like every other GASP row. |
| MM path | `MotionMatchingData/CHT_PoseSearchDatabases_Relaxed` | Row "Skid", bool column `bIsSkidding` -> `PSD_Relaxed_Skid` (schema `PSS_Relaxed_Skid`: trajectory, velocity, foot_l/foot_r position; asset `SS_180_Turn1`, UnmirroredAndMirrored). So with MM active the skid came from MM too. |
| Root / warping | same ABP, after the SM / MM node | **Offset Root Bone**: Translation `Interpolate` (Get_OffsetRootTranslationMode), Rotation **`Accumulate`** (Get_OffsetRootRotationMode), half-life 0.2 by default, `Get_OffsetRootTranslationHalfLife` reads bIsSkidding (0.3 is the value in that function (?)), max error 30. **Orientation Warping** in `RootBoneTransform` space, evaluation `Graph`. `Is Skidding` also masks the Aim Offset / additive leans and feeds `Get_ProceduralTargetTime`. |
| Clip | `GiustinoMoves/Skid/SS_180_Turn2` | Root-corrected (root follows pelvis, yaw 0 -> 180 by mid clip, see memory skid-clip-owner). Only skid clip carrying the `ReOrientRootBoneModifier` and `RotateRootModifier` modifiers. Root motion on (GASP default). |

## Why 5.7 turned once

The clip is played **upstream of Offset Root Bone with rotation Accumulate**. In Accumulate the mesh root's world yaw follows the
clip's root motion and ignores the capsule's rotation. The capsule turns 180 (Mover, toward the input) and the clip root turns 180,
but what you see is the clip's 180 only; at the end mesh and capsule both face the new way, so the offset is ~0 and nothing snaps.
Orientation warping (root space) only bends the legs toward the velocity on top. Warping stays on, nothing is counter-rotated.

## Why 5.9 turns twice (likely)

Our skid plays in the override layer (`ABP_GSTN_SkidLayer`, `UGSTN_SkidAnimInstance`) as a pose whose root already turned 180,
on top of a capsule the sim (`GSTN_LayeredMove_RunSkid`) also turns 180. If that layer pose does not go through the Offset Root
Bone in Accumulate (layer after it, or root motion not consumed), the two rotations add. Check where the layer is linked first.

## Steps for ABP_GSTN_Locomotion (copy of SandboxCharacter_Mover_ABP)

1. Sim: as 5.7, let the capsule turn toward the input during the skid (total 180 by the end). Holding facing and turning in the
   exit (`GSTN_LayeredMove_RunSkid.cpp:200`) also works, as long as the total is 180 and nothing snaps it.
2. Profile `bSkidInMainGraph` = true (skid override layer off), so the clip is not played twice.
3. Main SM: one `Skid` state copied from the Slide group (Transition to Skid only; drop Skid Loop, it was dead in 5.7). Entry
   function sets the state enum; enter on `IsSkidding` (BlueprintPure from 0127e6f4), leave when it is false or the clip ends.
4. `CHT_GSTN_LocomotionAnimations`: row "Skid" keyed on that state enum -> `SkidOwner/SS_180_Turn2_Src` (the owner's untouched
   root-corrected clip; do not re-run the RTG). If the 3-section timing is wanted, drive it with `GetSkidClipTime` in a Sequence
   Evaluator, still inside the state.
5. Keep Offset Root Bone rotation = Accumulate while skidding (GASP default), translation Interpolate, half-life 0.3 when skidding.
6. Keep Orientation Warping on (RootBoneTransform, Graph). Mask Aim Offset / additive leans while skidding, like 5.7.
7. Optional (MM instead of SM): PSD with the skid clip + `bIsSkidding` column in the PSD chooser, as 5.7.

Check first in the capture: one 180 only, no snap at the end, feet not sliding sideways at mid clip.

## Not readable offline (ask the owner for one screenshot each if needed)
- The Skid row's columns in CHT_MoverCharacterAnimations, and the transition rule bodies.
- The body of Get_OffsetRootRotationMode (whether it switches to Interpolate or Release in some state).
- Whether the AC_GiustinoEnhanchedMoves montage branch was still wired.
