# Handover: integration night 2026-11-16 (desktop, Opus medium)
Branch `claude/integ-night-1116` = `claude/pushbox-p5` c28a5777 + merge 118afe21 (timeline nodes CP1) + merge 50a91a0c (native hitbox editor) + fix 07b2e0bc. Both merges clean, no conflicts.
_ue59 left checked out on this branch, built, ready for the playtest.

## Build (Tools\CI\Run-Timed.ps1 -Preset Build)
- Attempt 1: ESITO FAIL, 4 compile errors (Saved\Logs\ci\integ1116-build.log, `Select-String "error "`). Never compiled before (cloud work).
  1. `GSTN_CombatNPTests_TimelinePlay.cpp`: anonymous-namespace `MakeTimeline(int32)` clashed in the unity build with another test file's helper -> renamed `TLPlayMakeTimeline`.
  2. `GSTN_GA_BaseCombat.cpp` PlayTimeline: param `Timeline` hid a class member (C4458 as error) -> `PlayedTimeline` in the definition.
  3. `GSTN_TimelineSelectionProxy.cpp` MakeSpecStructData: ternary `MakeShared : nullptr` -> `TSharedPtr<FStructOnScope>()`.
  4. `SGSTN_TimelineDetails.cpp` SectionHeader: `Text_Lambda(Label)` needs an rvalue -> lambda `mutable` + `MoveTemp(Label)`.
- Attempt 2: ESITO OK (integ1116-build2.log, "Result: Succeeded").

## Tests (-Preset Test -Tests GSTN, integ1116-test.log)
- ESITO OK, 63 s. 1655 Success, 0 Fail/Error (`Select-String "Test Completed. Result=\{Success\}"` count / `Result=\{(Fail|Error)`).
- New ones pass: GSTNCombat.NP.TimelinePlay.{Rules,Registry,StartInFrame}, GSTNCombat.Editor.HitboxNative.{DetailsShowOneHitboxAndKey,ParentingToGroupAndSocket,RenameAddDuplicateDelete,RulerSecondsZoomAndIcons}.
- One missing icon warning: CoreStyle brush `Icons.Target` (the toolbar uses its fallback). Cosmetic.

## NOT done / open
- Rollback check of the same-frame start (StartTimelineFromAbility writes combat state from the ability sim step) under the PIE lag harness: not run, needs a test host BP (timeline CP2). Unit test StartInFrame passes only.
- Dirty asset not touched and not committed: `Content/GASNPPTest/Combat/Overlord/AM_OverlordDAirV9.uasset` (was already modified before this thread; owner decides). PM_BoostJump/PM_ThrowRider/IC_GASNPP_Test/KIT_Test_Hits were clean.
- Editor not opened, no maps saved.

## Checkpoint 2 (same thread): hurtbox follow bones + Wait Command
- Merged 6bfba81b (hurtbox, clean) -> build FAIL, 3 errors (integ1116-hb-build.log): GSTN_StepSnapshot.h loop var `Extension` hid the member (C4458, renamed `PosedExt`, 2 sites); GSTN_EditorTests_HurtboxFollow.cpp TestEqual on UScriptStruct* -> TestTrue(==).
- Merged 466fc1ae (Wait Command / Wait Combat Frames / validator hints, clean), one build: ESITO OK (integ1116-hb-build2.log "Result: Succeeded").
- Tests -Tests GSTN (integ1116-hb-test.log): ESITO OK, 1665 Success, 0 Fail/Error. New pass: HurtboxPose.{DetailLevels,FollowsBone,HurtboxTrack,KitMovement,LegacySetUnchanged}, Editor.HurtboxFollow.{SimpleBody,TrackEditor}, NP.WaitTasks.{CombatFrames,Command}, Editor.SimGraph.TaskHints.
- Pushed 1684b510.

## Not done (budget reached, stable point)
- PIE leg check (low hit between the legs on an idle dummy): needs first the asset step of hurtbox-follow-bones.md §3 (prepare_hurtbox_set on the playtest set + save + re-bake all timelines = versioned asset changes), then a scenario with a low poke placed between the legs. Est. one desktop thread, Sonnet/Opus medium, ~6-8 $.
- Rollback check of the same-frame start AND of Wait Command's consume (both write sync state from the ability step): needs a test host BP (3-phase move) + gasnpp_test.py lag run. Timeline CP2 work, ~8-10 $. Until then: unit tests only.

## Checkpoint 3 (same thread): stopped at budget after the probe
- Editor probe (Saved/ci_scripts/hb_probe.py, read only, Run-Timed Capture, Saved\Logs\ci\hb_probe.txt): the playtest pawn BP_GASNPP_TestCharacter uses HBS_Test_Pawn = torso(spine_03) / head / feet(foot_l), no PreviewMesh, no follow, detail Both. There is no Overlord-specific set. HBS_FromPhysics has 23 per-bone capsules.
- The probe run ended in TIMEOUT: QUIT_EDITOR from -ExecCmds at frame 1 hangs on shutdown (log stops after Python cleanup). Next scripts should quit later (a tick callback) or the timeout eats 10 min each time.
- Nothing saved, no assets changed. Not pushed again (no code change).
- RISK before preparing (rule 12): prepare_hurtbox_set(HBS_Test_Pawn, mesh, True) adds a Simple "body" and makes torso/head/feet DetailedOnly, so in idle and locomotion every hit lands on "body" (head reactions, parts and gasnpp_test HIT_HURTBOXES expectations change). The leg check passes either way: the current feet capsule (r 30, z -62 on the root axis) already covers between the legs. Recommended: prepare a COPY (HBS_Test_PawnFollow) on a test pawn, or HBS_FromPhysics + simple body, not the shared set.
- Plan for the next thread (desktop, Opus medium, ~10-12 $): (1) copy set, prepare with the character mesh, save, gstn.Combat.BakeAll save; (2) PIE scenario: idle dummy, low poke aimed between the feet, debug hurtboxes on, check the inbox hurtbox index + 8-frame clip; (3) test host BP (GA_BaseCombat flag off, Play Combat Timeline x2 + Wait Command) and gasnpp_test.py lag run comparing server/client combat state.
