# Merge skid/camera/jump on _ue59 (desktop, Opus medium, 2026-11-18) — setup DONE (06:30Z run), skid capture NOT done
Branch claude/project-thread-98dml5, from 72d1afaf. Merged in order, no conflicts: 77vkmg (106ab72f), edmqov (8f28c410), qaz1xr (f797d1ad).

## Done
- Build OK (Saved/Logs/ci/merge3-build1.log, -build3.log: `Select-String 'Result: Succeeded'`).
- Tests GSTN: 1782 Success + 1 Fail (Saved/Logs/ci/merge3-test1.log.ue.log, count `Select-String 'Result=\{Success\}'` / `'Result=\{Fail'`).
  The fail was stale data: GSTNDefence.NP.Fragment.ActiveBytesWithinTable expected 26 differences, 77vkmg added 2 (downed start/free). Fixed to 28,
  re-run 4/0 (merge3-test2.log.ue.log). Full suite = 1783/0.
- Skid clip = ONE motion (owner 01:30Z: SS_180_Turn2 stops AND starts moving the other way). Mapping applied: Enter/Active stretch [0, Stop] to the
  rest frame, Exit plays on from Stop at ExitPlayRate (unchanged), and NEW: when the sim's Exit (8 f) ends first, the clip plays to its end on the
  ground (UGSTN_SkidAnimInstance::MaxTailSeconds 0.6 s, 0 = old cut). RunLayers tests 2/0 (merge3-test3.log.ue.log). Commit 69358126.
- Owner's "root-fix script" is NOT Python: it is an IK Retargeter, GiustinoMoves/Skid/RootCorrection/RTG_RootCorrection (UEFN->UEFN, root motion op
  GenerateFromTargetPelvis, height CopyHeightFromSource; read from its strings). The owner's SS_180_Turn2 differs from the M1 copy in
  Ported57/Skid (hashes differ; M1 copy untouched). Staged (not versioned): Content/GiustinoMoves/Skid/SS_180_Turn2 + RootCorrection/RTG_RootCorrection.
  New Tools/GAS/port_skid_owner_clip.py moves them to Ported57/SkidOwner, batch-retargets the copy with RTG_RootCorrection -> SkidOwner/SS_180_Turn2,
  logs root/pelvis at 0/half/end (Saved/SkidOwner/port_skid_owner_clip.log). NOT RUN.

## Blocked: Claude Code permission denied (twice) launching UnrealEditor-Cmd -run=pythonscript. Nothing below was run.
Run headless, editor closed, one at a time, check ESITO / git status after each:
1. `GROUND_CONTACT_RULES_ONLY=1` ground_contact_setup.py (montages.txt lists AM_OverlordDAirLand*), commit DA_GroundContact_Default.
2. port_skid_owner_clip.py, then Tools/GAS/setup_skid_anim_layer.py (log skid_clip = Ported57/SkidOwner/SS_180_Turn2). Commit Content/Blueprints/Animation.
3. MinStage 0: DA_Locomotion_Default > Run Stages > Skid (and the Overlord kit's locomotion profile if it has its own). No script yet.
   Tune StopFraction (0.6) to where the body rests in SS_180_Turn2 (pelvis speed ~0 in the port log); consider ExitFrames vs clip tail.
4. Tools/GASNPPTest/camera_anchor_setup.py (optional DA).
5. setup_test_kit_archetypes.py, setup_qa_movement_1117.py, setup_overlord_kit_shared_f4.py (jump kit data; ini defaults already compiled).
6. Skid capture at run stage 1, KIT_Test_Overlord, 3 PIE players, side+top, 4x slow -> Fable look, FACING FIRST (clip root turns 180 + sim turns
   actor = possible double turn). Only PASS to owner.
Kept: owner's uncommitted AM_OverlordDAirV9.uasset. _ue59 is built and playable (skid = placeholder lean until step 2-3; kits keep old jump data).
Cost ~6 $.

## Update 06:30Z (owner allowed the editor scripts)
All headless, one at a time, ESITO OK (logs Saved/Logs/ci/py_*.log.ue.log):
- ground_contact_setup (rules only): montages.txt lists AM_OverlordDAirLand*; DA_GroundContact_Default committed.
- port_skid_owner_clip (2nd attempt: 5.9 DuplicateAndRetarget wants target_path). FINDING (Saved/SkidOwner/port_skid_owner_clip.log): the owner's
  SS_180_Turn2 is ALREADY root-fixed (root follows the pelvis, pelvis local ~(0,0,z); root yaw 0 -> 179.7 by t=0.417 of 0.833 s; root travels
  +65 cm then back to +49). Re-applying RTG_RootCorrection gives an odd root (yaw 108 at t=0, pelvis local (78,-25,-3)). The layer uses the
  re-retargeted copy SkidOwner/SS_180_Turn2 (handover default); the untouched owner clip is SkidOwner/SS_180_Turn2_Src.
  RECOMMENDED (da confermare, coordinator -> owner): SKID_CLIP=/Game/Blueprints/Animation/Ported57/SkidOwner/SS_180_Turn2_Src and re-run
  setup_skid_anim_layer.py (a run with that env was refused by the permission system). Facing risk confirmed by the data: the clip root turns 180
  in its first half, the sim turns the actor after the skid -> likely double turn unless the layer drops the root track.
- setup_skid_anim_layer: ESITO OK, skid_clip = SkidOwner/SS_180_Turn2. setup_skid_minstage_0.py (new): 1 profile, DA_Locomotion_Default MinStage 0.
- camera_anchor_setup (DA_CameraAnchor_Default), setup_test_kit_archetypes, setup_qa_movement_1117, setup_overlord_kit_shared_f4: all kits + GA archetypes
  re-saved and committed (9bfe6bec).
- Capture NOT run: pie_capture_timed.ps1 (windowed editor) was refused by the permission system. Ready: new sequence mv_skid_base in
  pie_capture_move.py (reverse at render frame 30 = base stage). Command: $env:PIE_CAPTURE_VIEWS='az90,game'; Tools/GASNPPTest/pie_capture_timed.ps1
  -Kit KIT_Test_Overlord -Out docs/notes/reviews/skid-ss180turn2-1/cap -Sequences mv_skid_base, then pie_capture_grid.py, then Fable (facing first).
- Not re-run after the data changes: the full GSTN suite (C++ unchanged since 1783/0) and movement QA.
## Update 07:15Z
- Capture 07:42 done: FAIL, the re-retargeted clip sinks the body (docs/notes/reviews/skid-ss180turn2-1/REVIEW.md). Owner chose the original clip: setup_skid_anim_layer.py default is now SkidOwner/SS_180_Turn2_Src (007ec463), NOT yet run (auto-mode classifier denied it even after the owner's rule; a new session should load the rule).
- Next thread: run setup_skid_anim_layer.py, commit Content/Blueprints/Animation, capture mv_skid_base (PIE_CAPTURE_VIEWS=az90,game), review facing first.
