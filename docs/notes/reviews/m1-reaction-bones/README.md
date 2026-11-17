# M1 step 3: base hit-reaction clips, "bones detached/stretched"

Clips: `/Game/GASNPPTest/Combat/Reaction/AS_React_*` (AnimPipeline bakes). Image: `groundhit_az315_asis_vs_scale1.png`
(AS_React_GroundHit frames 0,2,6,10,14,17, one view = 3/4 front, same frames, top = as-is on SKM_UEFN_Mannequin, bottom = diagnostic copy).

## Chain (no retarget exists)
- Skeleton of every AS_React_*: `SK_UEFN_Mannequin` (same as the mesh and the game character). No source skeleton, no IK Retargeter,
  no chain mapping, no import data (`asset_import_data` empty): the clips are baked straight onto the UEFN skeleton by
  `animpipe/ue_rig.py` from the Control Rig `CR_Mannequin_Body` (UE5 mannequin rig) via a level sequence.
- Local bone TRANSLATION lengths are correct: `_lock_lengths` pins them to the source frame 0 (check: only root +16.3, pelvis, ik_*, VB bones differ from the reference pose, as intended).

## Finding (measured, Saved/Logs/ci/react_bones.txt, script Tools/GASNPPTest/react_bones_m1.py, command in README header of this folder's txt)
All 8 reaction clips carry NON-UNIT SCALE KEYS, constant in every frame, on 5 bones (reference pose scale is 1.000 everywhere):
| bone | scale3d in clip |
|---|---|
| head | (1.15, 1.15, 1.15) |
| thigh_l, thigh_r | (1.00, 1.12, 1.12) |
| foot_l, foot_r | (1.00, 0.89, 0.89) |
`_lock_lengths` rewrites translation only and copies `scale3d` through from the Control Rig bake, so the rig's scale leaks into the clip:
oversized head, fat thighs, shrunk feet, and the scaled thigh moves its children (calf/foot attachment) off the mesh joints.
It is a bake bug (scale not locked to 1), NOT a retarget setting, so I applied no fix to the original or to the game assets.

## Diagnostic copy (not part of the game)
`/Game/AnimPipeline/Diag/AS_React_GroundHit_scale1` (local, Content/AnimPipeline is not versioned): same clip, scale forced to 1 on those 5
tracks (script `Tools/GASNPPTest/react_scale1_m1.py`). Bottom row of the image. Visible difference is moderate (head, thighs, feet), the pose is identical.
If Fable agrees, the obvious fix is in `animpipe/ue_rig.py::_lock_lengths`: write `scl.append(Vector(1,1,1))` (or `unreal.Vector.ONE`) instead of `t.scale3d`, then re-bake the 8 clips.
Camera: standalone capture at dist 270, fov 55, centre (0,15,95), so lying poses sit low in the frame.
