# Dodge long jump, downed roll, move fallback (cloud + desktop build, Opus, 2026-11-17). Branch `claude/project-thread-v94blx` from `claude/playtest-fixes-1117c` 23fb94a4
`claude/qa-movement-1117` NOT merged: KIT_Test_Playtest.uasset conflicts (binary), merge on the desktop when both are wanted.
## Done (owner requests 2026-11-17)
1. Ground dodge = Mario long jump: no startup, no landing lag, no montage, blocking until landing (unchanged), feet intangible only first 40 % (vulnerable end).
   New: run speed carried in (`FGSTN_DashMoveData::CarriedSpeedScale` 1, `MaxForwardSpeed` 1400; never under 800) and full air control while it flies
   (`FGSTN_TimelineTrack_Movement::AirControlOverride` 1, read by `FGSTN_RunStageMover::ApplyAirValues` via `UGSTN_CombatSimComponent::GetRunningMoveAirControl`;
   needs the kit's run stages, which the Mario preset has).
2. Air dodge = the ground one (`UGSTN_GA_AirDodge` → `AirDodgeFromGroundParams`), blocking up to 120 frames (no chaining it to fly). Dive roll data kept (`AirDodgeMove/Params`).
3. Downed roll: `FGSTN_GroundBounceRules::RollInput` DodgeAndStick (default) / StickOnly; `RollPressBufferFrames` 6 (counted from 1). Dodge button = layout `Movement.Dodge`.
4. Auto get-up: `FGSTN_GroundBounceRules::AutoGetupFor` Everyone (default, owner 1A) / DummiesOnly (player = pawn possessed by a player controller).
5. Missing-move fallback: `FGSTN_CommandSet::bFillMissingMoves` (on) + `FallbackOverrides`; defaults `UGSTN_StandardSlots::BuildDefaultFallbacks`
   (same button; stance first, then direction). Compiler adds borrowed rows (`FillMissingMoves` in GSTN_CompiledBindingTable.cpp): never over a row that covers
   the context or would tie with it (validator conflict), weapons first then kit.
## Choice 2 (Terry down tilt) — research, no code
Terry's d-tilt launches at 80-86° (ssbwiki): at 0 % a small near-vertical pop with non-tumble hitstun, then grab/jab combos. That is what we do now
(launch tier between LaunchThreshold and TumbleThreshold → HitStand hop). Option B "stays grounded, pushed" already exists as data: raise
`FGSTN_ReactionTuning::LaunchThreshold` (combat rules); below it a staggered hit only pushes back. No per-move flag yet.
## Applied choices (rule 6)
Defaults above; the air dodge's 120-frame cap; fallback borrows in the air too (crouch + X in the air with no crouch aerial = neutral air).
## To confirm (owner)
- Long jump: carry run speed (cap 1400) and full air control: ok, or base speed only?
## Tests: see the end of this file (desktop build).
Desktop _ue59 (branch checked out there, AM_OverlordDAirV9.uasset local change left untouched): build OK (Saved\Logs\ci\v94blx-build1.log);
`Run-Timed.ps1 -Preset Test -Tests GSTN -Log Saved\Logs\ci\v94blx-test1.log`: 1722 Success / 1 Fail (Select-String "Test Completed. Result=\{(\w+)\}").
Fail = GSTNCombat.GAS.Table.AssembleFromDefinitions counted rows (12 vs 3: the borrowed rows); fixed in the test (fallback off there), re-run below.
Cost ~12 $ estimated (Opus, 1 build, 2 test runs).
Re-run after 7d7b4f20: build OK (v94blx-build2.log), `-Tests GSTNCombat.GAS.Table` 1 Success / 0 Fail (v94blx-test2.log). Total 1723/1723. Branch ready for playtest in _ue59.
