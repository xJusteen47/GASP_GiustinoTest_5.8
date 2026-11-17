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
