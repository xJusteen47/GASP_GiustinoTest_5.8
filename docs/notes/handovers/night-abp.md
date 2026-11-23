# NIGHT-ABP handover: feet creep + crouch after reactions, DIAGNOSIS ONLY (desktop, 2026-11-16)
Branch `claude/night-knockdown` (after 9307e910). Folder `_ue59`, Opus medium. No ABP, asset, map or setting changed or saved. Lock set, then freed.
## What was measured
- `pie_capture_react_ingame.py` rows now carry world `foot_l/foot_r/root/pelvis` of the drawn mesh; env `PIE_CAPTURE_CVARS` runs console
  variables before Play. Reader `python Tools/GASNPPTest/react_feet_rows.py <frames.jsonl>` (per segment: capsule travel, foot travel, root-capsule).
- knockdown_getup (`Saved/nightkd/react_base`, ORB on) and the same with `a.AnimNode.OffsetRootBone.Enable 0` (`react_noorb`): IDENTICAL root offsets
  (`docs/notes/reviews/night-abp/rootoff_*.txt`). Root bone is 16.3 cm (horizontal) off the capsule from the tumble through knockdown and get-up,
  then after the get-up (capsule still) it goes 14.5 (v165) -> 3.0 (v175) -> 0 (v190) while foot_l moves 446.9 -> 457.7 (~11 cm): that is the creep.
  Pelvis-capsule z dips to -4..-5.5 in the same frames: the "crouch".
## Diagnosis
- NOT Offset Root Bone (disabling it changes nothing) and the capsule does not move. The decay over ~25 frames matches the get-up timeline's 20-frame
  hold-and-blend-out (react-hook-1): the reaction clips' pose has its root ~16 cm off the origin, and blending it back to the idle pose slides the root
  (and so the planted feet) and sags the pelvis. The graph order is Slot(DefaultSlot) -> DeadBlending -> OffsetRootBone -> Procedural_Feet (dump:
  `Saved/nightkd/abp_probe.txt`, `GSTN_AnimGraphAuthoringLibrary.describe_graphs`).
- Not proven: mesh component offset vs root bone (the mesh_rel/root_cs logging failed on the Python names; 2 attempts, stopped per rule 5).
## Options for the owner (animations are his)
- A (recommended): lock the root bone translation of slot poses to the capsule in the ABP (reactions drawn centred on the capsule, no slide on blend-out).
  Changes where every reaction is drawn by ~16 cm: owner decision. ~4-6 $.
- B: fix the root keys of the reaction clips in the anim pipeline (animation data). C: leave it.
