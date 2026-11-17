# Hitstop gravity, downed dodge buffer, Overlord jab walk, dair landing feet, skid clip diagnosis (cloud, Opus high, 2026-11-17)
Branch claude/project-thread-77vkmg from 98dml5 72d1afaf (crash fix). NOTHING COMPILED (cloud). Owner playtest bugs of 23:20Z.

## Fixes (code)
1. **Hitstop keeps falling** (attacker and victim). Cause: Mover's mixer (MovementMixer.cpp CheckPriority) breaks a priority tie in favour of the
   move that STARTED FIRST; FGSTN_LayeredMove_Freeze ran at priority 0 (headers claimed 1), so any override move already running (aerial movement
   track, dash impulse, juggled victim's knockback) beat it and the falling mode kept gravity. Fix: freeze Priority = HitstopPriority 250, SDI drift
   freeze 251. Test GSTNCombat.NP.Hits.Hitstop.FreezeWinsOverEarlierMove (mixer, both orders, wire).
2. **Dodge not bufferable from downed**. Cause: the roll's dodge press buffer (RollPressBufferFrames 6) was checked only on free frames; the bounce +
   ground hit clips outlast it. Fix: FGSTN_DefenceFragment.DownedStartFrame/DownedFreeFrame (knockdown wire group); presses from 6 f before the downed
   start up to the first free frame count for 6 f after it (FGSTN_TumbleLanding::DownedRollCarry). A late stick never rolls on an old press.
   Tests GSTNDefence.NP.Landing.RollPressCarriedFromDowned; fragment tests updated (every group live 2 + 93 bytes, Differs downed start/free).
3. **Overlord jab walks under a full stick**. Cause: AM_OverlordJab{1,2}V3 have root motion OFF (setup_overlord_jab_kit.py:50, anim-jab-5), and the
   montage simulator queues a root motion move only when the montage has root motion (NetMontageSimulator.cpp:550), so nothing overrode the walking
   mode. Fix: a ground move sends no walk input (GSTN_CombatSimComponent.cpp, before the steer) unless its timeline sets the new
   FGSTN_CombatTimelineData::bWalkDuringMove (default off). Air drift unchanged. Test GSTNCombat.Core.Timeline.GroundMoveDoesNotWalk.
4. **Dair legs in the floor in the final landing lag**: the ground contact fix (27htqa, merged in F4) had rules only for reactions. ground_contact_setup.py
   now adds rule "Aerial landing lag montages" (names with AirLand / LandingLag: AM_OverlordDAirLand*). Poses untouched (rule 13e).

## Diagnosis only: skid / run start clips not visible (Overlord kit, run stage 1)
- Sim skid DOES fire at run stage 1 (FGSTN_RunSkidTuning MinStage 1, MinSpeed 350, ReverseStickCos -0.5).
- The clip never shows: the override slot activates only for a linked anim instance implementing IGSTN_OverrideLayerSource
  (GSTN_CharacterAnimInstance.cpp:40-55). Only the wall layer has one. ALI_GSTN_Skid / ALI_GSTN_RunStart are EMPTY interfaces from M0; M1 only
  copied Skid_Revised / SS_180_Turn into Ported57/Skid. The skid and run start layers are polish M4, never started. F4's "plays the same on both
  kits" = both show plain GASP locomotion + placeholder lean.
- Next: thread M4-skid (cloud code ~6 $: UGSTN_SkidAnimInstance reading UGSTN_RunPresentationComponent SkidPhase + Tools/GAS/setup_skid_anim_layer.py
  modelled on setup_wall_anim_layer.py, Skid_Revised for Enter/Active/Exit) + desktop build/capture/Fable (~5 $). Run start: clip source still
  "da confermare" (run-start-skid-import-piano.md §3: no start clip in GaspMoverProject -> GASP start?). Owner says he named a clip: ask which.

## Desktop steps (_ue59, Sonnet/Haiku low, one attempt per rule 5)
1. Merge claude/project-thread-77vkmg into the _ue59 branch (mark _ue59/IN-USO.txt).
2. `Tools/CI/Run-Timed.ps1 -Preset Build -Log Saved/Logs/ci/build_77vkmg.log` (header changes: TimelineTypes, DefenceFragment, Freeze).
3. `Tools/CI/Run-Timed.ps1 -Preset Test -Tests GSTN -Log Saved/Logs/ci/test_77vkmg.log`: expect 1768 + 3 new, 0 Fail
   (count: Select-String "Result={Success}").
4. Rules only, ABP untouched: `set GROUND_CONTACT_RULES_ONLY=1` then UnrealEditor-Cmd ... -run=pythonscript
   -script="<project>/Tools/GASNPPTest/ground_contact_setup.py" -unattended -nullrhi (log Saved/GroundContact/ground_contact_setup.log, montages.txt
   must list AM_OverlordDAirLand*). Then git restore the known dirtied assets (memory list), commit DA_GroundContact_Default.
5. Check in PIE (KIT_Test_Overlord): aerial hit freezes both in the air; dodge+stick held during the knockdown bounce rolls when the body lands;
   jab with stick full stays in place; dair landing feet above the floor (a.GSTN.GroundContactFix.Debug 1).
## Risks
- Freeze at 250 also beats LedgeGuard (1) and anything a future move sets above 250: keep hitstop the top priority.
- A ground move that relied on stick walking (none found: get-ups use bIgnoreMoveInput, dodge/dash use movement tracks) needs bWalkDuringMove.
- Timers of other layered moves still count during the freeze (unchanged behaviour).
Cost estimate ~11 $ (Opus high, cloud only).

## Addendum: skid layer (M4 skid part, coordinator go 23:29Z, ~5 $), NOT compiled
- GSTNAnimationCore: IGSTN_ClipTimeSource + leaf node FGSTN_AnimNode_ClipAtTime (clip and time copied from the instance in PreUpdate, no root
  motion); editor node "GSTN Clip At Time"; library FindOrAddPoseNode.
- GSTNCombatNP (Presentation/GSTN_RunLayerAnimInstances): UGSTN_SkidAnimInstance (slot Skid while the foot slide is a run skid; SkidClip stretched
  over [0, StopTime] up to the rest frame, Exit plays on at ExitPlayRate; turns the placeholder lean off when a clip is set) and
  UGSTN_RunStartAnimInstance (slot RunStart, one play of RunStartClip when ground speed crosses StartFromSpeed from still; EMPTY clip = off, the
  owner picks the clip). Tags Anim.Set.Skid / Anim.Set.RunStart. Tests GSTNCombat.NP.RunLayers.{SkidClipTime,RunStartTrigger}.
- Script Tools/GAS/setup_skid_anim_layer.py: ABP_GSTN_SkidLayer (Skid_Revised) + ABP_GSTN_RunStartLayer (no clip, env RUN_START_CLIP=<path> later),
  DA_AnimSet_Skid/RunStart, rules in DA_AnimSetCollection_SandboxCharacter. Re-runs setup_anim_layers.py first (idempotent).
- Desktop: after step 2-3 above, run it headless (editor closed): UnrealEditor-Cmd ... -run=pythonscript -script="<project>/Tools/GAS/setup_skid_anim_layer.py"
  -unattended -nullrhi, expect "ESITO: OK". Commit the new/changed assets in Content/Blueprints/Animation. Then capture a skid at run stage 1
  on KIT_Test_Overlord (side view, 4x slow) for the Fable review (rule 10): first frame changes pose, feet stop when the body stops.
- Tuning if the review says so: StopFraction / StopTime / ExitPlayRate on ABP_GSTN_SkidLayer class defaults (data, no code).
- Risk: Skid_Revised's own root/hips travel is not removed (root motion off); if the clip drifts forward of the capsule the fix is a pose-space
  offset, not a sim change.

## Addendum 2: skid clip = SS_180_Turn2 (owner 23:52-23:53Z; corrected: it is the SKID clip, not the run start)
Choice applied (rule 6): SS_180_Turn2 REPLACES Skid_Revised as the skid clip (owner's latest word); Skid_Revised stays as fallback
(SKID_CLIP env). The run start slot stays EMPTY until the owner names a clip.
Source: GaspMoverProject/Content/GiustinoMoves/Skid (5.7 project: copy files only, never open/save it). Its root may not rotate well; the owner
wrote a root-fix script there (subfolder RootCorrection seen; the cloud could not open the folder, nobody has read the script yet). M1 already copied
an SS_180_Turn2 into Ported57/Skid from another folder: keep it untouched, compare.
Desktop steps, before running setup_skid_anim_layer.py:
1. Read the owner's script in GiustinoMoves/Skid/RootCorrection first (what it changes, which asset it expects). Nothing writes into GaspMoverProject.
2. Copy GiustinoMoves/Skid/SS_180_Turn2.uasset to _ue59 Content/Blueprints/Animation/Ported57/SkidOwner/ (skeleton SK_UEFN_Mannequin as M1; broken
   reference = migrate with the port57_m1.py pattern).
3. Run the owner's root-fix script on that copy only (adapt the asset path, never the logic; any change is written in the handover).
4. Script default is now SKID_CLIP=/Game/Blueprints/Animation/Ported57/SkidOwner/SS_180_Turn2: run setup_skid_anim_layer.py (log line: skid_clip = that path).
5. Capture a skid at run stage 1, KIT_Test_Overlord (side + top, 4x slow, frame before the reverse included) -> Fable review (rule 10). Check FIRST
   the facing: the clip turns the root 180 while the sim turns the actor after the skid; a double turn (body ends facing back, or pops 180 at the
   exit) = root fix / clip question for the owner, not a sim change. Tuning: StopFraction / StopTime / ExitPlayRate on ABP_GSTN_SkidLayer.
