# Polish M4a: run rules from the owner (L3 middle stage, resume after skid). Cloud, Opus, NOT compiled
Branch `claude/project-thread-jprs4t`, base = merge of `claude/qa-movement-1117` 180164f3 + `claude/playtest-fixes-1117c` 23fb94a4
(conflict only on `KIT_Test_Playtest.uasset`: took 1117c, so **rerun `Tools/GAS/setup_qa_movement_1117.py`** on desktop, as its handover says).
Source: owner 2026-11-17 12:52Z (polish-movimento-vfx-piano.md §5), brief handoffs/polish-m4a-brief.md.
## Done
1. L3 boost -> `FGSTN_RunStagesTuning::BoostTargetStage` (Run Stages|Boost), counted FROM 1 like the owner's names (1 run, 2 sprint 1,
   3 sprint 2), default 2, clamped into the table; a boost never lowers the stage. Replaces "L3 = last stage". No constant, no asset change
   needed (the new property takes the default in DA_Locomotion_Default).
2. `FGSTN_RunSkidTuning::bResumeStageAfterSkid` (Run Skid, per kit via the locomotion profile), default ON: the stage is kept at skid start
   and frozen while the skid layered move is live (new `FGSTN_RunFrameInput::bSkidActive`, filled in `FGSTN_RunStageMover::PostMovement`
   from `FindActive`), so the push off continues from it. Off = old rule (`bResetsStage`). No new state: the stage already lives in
   the Mover sync state and serializes; the input comes from the frame's sync state (rollback safe).
3. Verified by reading + tests: a released stick never skids (`ShouldStart` needs stick >= MinStickMagnitude 0.5 and cos <= ReverseStickCos
   -0.5, both data); a release only takes the release path (ReleaseBrake, carry StickRelease). No fix needed.
## Tests (pure logic, NOT run: the cloud has no UE)
- Updated: Core.RunStages.BoostWindow (target = index 1, + target 3/99/1, no lowering), Core.RunSkid.RunState (resume off for the old rule).
- New: Core.RunSkid.Resume (kept, frozen 20 frames, push off, replay identical, off = stage 1), Core.RunSkid.ReleaseVsReverse.
- NP: no NP test reads the boost/skid stage rule; NP.RunStages.* unchanged.
## Next (desktop, Haiku low)
Build + `Run-Timed -Preset Test -Tests "GSTNCombat.Core.RunStages+GSTNCombat.Core.RunSkid+GSTNCombat.NP.RunStages"`; then PIE feel check.
## Choices applied (rule 6) / to confirm with owner
- Resume default ON (brief: "da confermare"). Resume overrides bResetsStage.
- BoostTargetStage is 1-based (owner-facing); Skid.MinStage stays 0-based (old field, untouched).
- During the skid the stage counters wait (no release drop, no boost window); they restart after the skid.
Cost estimate ~3 $.
