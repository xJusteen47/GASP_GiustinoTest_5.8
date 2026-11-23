# Brief: sprinter run start polish s2 (owner 2026-11-21 13:32: "anche la 8 non è male. Fai il giro di rifinitura")
Desktop, Opus high, budget ~6 $, updates _ue59 (lock). Branch claude/runstart-generic-desktop. s1 (Variant 8/18) and g3 (7/17) stay
as they are: build s2 as a NEW variant 9/19 (cvar help in GSTN_LocoBaseSubsystem.cpp lists 5-8: add 9, real build).
Goal: longer push so "ta-ta-TUM" is less rushed (s1 ~0.1 s per "ta"), clean feet (s1 per-contact L max 29 cm), frantic limbs from
frame 1, knees forward (LEG_IK=1 default), elbows really back (s1 read forward-down), stock root motion (rule 14).
Identity traits: 1 low sprinter drive (pelvis -10-15 %, torso 35-45 deg, elbows back); 2 ta-ta-TUM; 3 no clash idle/loop.
## Recipe (review s1, docs/notes/reviews/runstart-s1_h1/review_fable.md)
TAG=s2 R0=2.0 PRE=3 RATE_PRE=0.8 N_REV=18 HOLD=12 RATE_SAG=0.1 LEAN_OUT=20 ENTRY=4 ENTRY_LEAD=1 BOUNCE=0.3 LEAN_DEG=28 LEAN_IN=4
ARM=1.6 ARM_BACK=14 DROP_CM=14 DROP_IN=4 DROP_HOLD=12 DROP_OUT=19 python Tools/AnimPipeline/cards/runstart/runstart_generic_compose.py
Check torso angle f4-12 (40-46 deg in s1) and pelvis (~80 cm) with the one-off in handover runstart-generic-g3-desktop.md.
## Steps (all scripts exist, see runstart-generic-g3-desktop.md)
1. Cascadeur: open RunStart_Generic_s1.casc, save as RunStart_Generic_s2.casc; keys.set on frame N-1 if N > 60 (keys.delete beyond
   N-1 if fewer); run runstart_generic_write_casc.py with TAG s2 (exec with globals app/csc/TAG; namespace does not persist); check
   no backward knee (knee z vs hip-foot line >= 0, joints thigh/calf/foot); export export_joints bake up_axis Z.
2. Owner editor is OPEN: close only via Tools/UE59/Close-OwnerEditor.ps1 (ask the owner first). Lock _ue59/IN-USO.txt.
3. Commit, Run-Timed -Preset Build, Save-Assets runstart_generic_g_ue.py (RSGG_TAG=s2), rows runstart_generic_rows_ue.py
   (RSG_K=9 RSG_ASSET=.../AS_GSTN_RunStart_Generic_s2 RSG_REPORT=runstart_generic_s2), captures runstart_generic_captures.ps1
   -Variant 9 -Clip AS_GSTN_RunStart_Generic_s2 -Name s2 -RefName generic_h1 -Ref Saved\RunStartGeneric\generic_h1.
4. Review: brief like runstart-s1_h1/brief_review.md (compare with s1); Fable subagent; mp4 frame n = capture v(n+60) - tell the
   reviewer; look yourself at an az45 strip v86-v120 (ffmpeg crop=360:360:360:0). Only PASS + video to the owner.
5. Reopen the editor (UnrealEditor.exe GASP_Giustino2.uproject), free lock, push, handover with COSTO line.
## Also (owner 2026-11-21 13:33): footstep foley on our run start clips
Add the STOCK GASP foley notifies (copy the notify class, track and settings from M_Relaxed_Run_Start_F_Lfoot / M_Relaxed_Run_Loop_F;
ask the Unreal Assistant how stock GASP does it, rule 14) to AS_GSTN_RunStart_Generic_g3 and _s2 (and s1 if kept), timed to the
real plants: frames where contact_l / contact_r go 0 -> 1 in the clip's own curves (stock convention). Do it in
runstart_generic_g_ue.py (re-import keeps them) so it is reproducible; check in the capture that the events fire (audio log or
notify log), one event per plant, none during the churning foot spin.
