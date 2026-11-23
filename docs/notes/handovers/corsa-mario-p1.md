# Handover: Mario run P1 (2026-11-16, cloud, Opus, branch claude/project-thread-dru892 from feature d258c41c)

**State: written, NOT compiled, NOT tested** (cloud has no UE). Every number below is a design default, none comes from a log.
**What**: 3 run stages + L3 boost, all kit data. Plan `docs/notes/corsa-mario-piano.md` (also updated: retreat method D, skid momentum memory, +1.5 $).
- Core `Movement/GSTN_RunStageTypes.h` + `GSTN_RunStageLogic.cpp`: `FGSTN_RunStage` (MaxSpeed, Acceleration, TurningStrength, ReleaseBrake, FramesToNextStage, NextStageSpeedFraction, CarryFrames = K of 2A), `FGSTN_RunStagesTuning` (rules, `BoostCommand`, `BoostMode` InitialWindow|Anytime|Disabled, `BoostWindowFrames`), `FGSTN_RunStageState` (Stage, FramesInStage, FramesSinceMoveStart, FramesSinceStickRelease, CarriedSpeed + FramesSinceCarry + CarrySource Release|Skid for P2/P3, PrevSpeed, LastFrame), pure `FGSTN_RunStageLogic::Step`.
- GAS `Kits/GSTN_LocomotionProfile.h`: `UGSTN_LocomotionProfile` (stages + names of the walking-mode properties they drive: RunSpeed/SprintSpeed, Run/SprintAcceleration, TurningStrength/SprintTurnStrength, Deceleration), kit extension `FGSTN_LocomotionKitExtension`, attach + loadout extension (registered in the GSTNCombatGAS module, same pattern as SpinDrill). Kit without the entry = old behaviour.
- NP `Movement/GSTN_RunStageSyncState.h`: `FGSTN_RunStageSyncState` in Mover's sync state collection (persistent, NetSerialize, ShouldReconcile). Driven by the existing `FGSTN_StatsMovementModifier`: OnPreMovement assigns the stage values after the stats factor (stage read from the tracker = start of frame); new OnPostMovement steps the state in the output sync state.
**Why the modifier and not a C++ walking mode**: the walking mode is the GASP Blueprint (SmoothWalkingMode parent); a C++ mode means reparenting a shared binary BP. The modifier already assigns speeds every substep and is in every combat pawn. Cost: OnPostMovement writes through `const_cast` on the state machine's own (non-const) output state, commented.
**Input (data only)**: tags `Input.Button.RunBoost`, `Command.Movement.RunBoost` in `Config/Tags/GSTNCommands.ini`; pad L3 -> `IA_GASNPP_RunBoost` in `setup_player_input_contexts.py`; button + press slot in `setup_gas_assets.py`; `Tools/GAS/setup_run_stages.py` makes `DA_Locomotion_Default` and adds the entry to every kit in /Game/GASNPPTest/Combat.
**Tests**: `GSTNCombat.Core.RunStages.{Climb,BoostWindow,Drop,Rollback}`, `GSTNCombat.NP.RunStages.{SyncState,StageValues}`.

## Desktop steps (Haiku, timed scripts; one attempt = one build)
1. Editor closed. Pull `claude/project-thread-dru892` in a worktree or the playtest copy (not _ue59 unless the coordinator says so).
2. Full build (header changes): Build.bat GASP_Giustino2Editor Win64 Development, 45 min cap. On error, max 2 fixes.
3. Headless tests, 30 min cap: `-ExecCmds="Automation RunTests GSTNCombat.Core.RunStages+GSTNCombat.NP.RunStages+GSTNCombat.NP.Stats;Quit"`; then the whole `GSTNCombat` set for regressions. Cite the log and the grep count in the next handover.
4. Data, headless, in order: `setup_player_input_contexts.py`, `setup_gas_assets.py`, `setup_run_stages.py`. Revert the known dirtied assets; never save a map.
5. PIE on pad: stage climbs while running straight, L3 at run start = stage 3, L3 late = nothing, release > 8 frames or a 180 = stage drop. `mover.debug` / ToString shows `RunStage`.

## Open / to check
- The GASP BP must read RunSpeed/SprintSpeed etc. each frame (the stats modifier already relies on it). If the stage does not change the speed in PIE, that is the first thing to check.
- Substeps: the stage steps on the first substep of a frame (frame guard). Kit swap to a kit without the entry leaves acceleration/turn/brake at the last stage's values until the BP resets them (P2: restore archetype).
- Stage defaults 450/650/850 cm/s, 40 and 60 frames: placeholders for the owner to tune in DA_Locomotion_Default.
