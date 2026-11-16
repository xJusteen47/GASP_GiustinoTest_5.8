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
