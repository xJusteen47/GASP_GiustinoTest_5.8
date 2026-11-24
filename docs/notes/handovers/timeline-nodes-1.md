# Handover: timeline nodes, checkpoint 1 (1A Play Combat Timeline And Wait + 1B Start Combat Move)
Thread cloud, Opus high, 2026-11-16. Base `claude/integ-cp3` 069287f7. Branch `claude/project-thread-wptsyy`. Plan: `handoffs/archivio/2026-11/move-timeline-decoupling.md`.
**State: code + unit tests written, NOT compiled (no UE in the cloud). Desktop build + tests are the next step.**

## Done
- **Play Combat Timeline And Wait** = `UGSTN_PlayCombatTimelineTask` (fork `UBasePredictionTask`: K2 node made by the fork editor, active bit synced, no rollback duplicates). Pins On Key / On Hit Confirmed / On Completed / On Interrupted(reason) / On Cancelled.
- `UGSTN_GA_BaseCombat`: `bPlayTimelineOnActivation` (default ON = standard moves unchanged), `PlayableTimelines`, `bEndWhenTimelineInterrupted`, `PlayTimeline()`, `IsPlayingTimeline`. EndAbility split into `FinishTimelinePlay` (same montage/recoil/tag rules). Host = flag off: plays 0..N, survives completed plays.
- **Start Combat Move(class)** on `UGSTN_GA_Base` (1B): other move starts + activates in the same frame; running move cancelled; frame-0 commit done.
- Same-frame start: `UGSTN_CombatSimComponent::StartTimelineFromAbility` writes the frame's combat state from the ability sim step (+ Tags-track diff, state tags). Refused outside the sim step / restoring / gate closed / unregistered.
- Registry: `AppendPlayableTimelines` (entry per class+timeline), `FindPlayEntry`, `FindMoveEntry`, `IsHostClass`; host bindings = plain ability (both table paths). Pre-tick commit skips hosts.
- Fork: `UNpAbilitySystemComponent::IsInSimulationTick()` getter (1 line). Docs: combat.md §11.3.
- Tests `GSTNCombat.NP.TimelinePlay.{Rules,Registry,StartInFrame}`.

## Choices applied (rule 6, to list to owner)
1. Host = a `GA_BaseCombat` with the flag off, not any `GA_Base` (reuses all playback code; plan said any GA_Base). 2. Plays start with source Forced (no new enum value).
3. Host ends on an interrupted play by default (no wiring needed for hits). 4. Played timelines never commit the ability (Commit Ability in the graph). 5. Frame 0 of an in-frame start runs no attacker step (same as a command start today).

## Risks to verify on the desktop
- The in-frame write into the combat pending state from the ability tick (`WriteSyncState`): determinism under rollback → PIE harness with lag (gasnpp_test.py) on a test host BP.
- The fork K2 node: menu category is "Prediction Task" (fork), title "Play Combat Timeline And Wait"; delegate with enum param must compile in the node.

## Desktop steps (fisso, editor CLOSED, ~10 min, Haiku)
1. `git fetch origin claude/project-thread-wptsyy && git checkout claude/project-thread-wptsyy` in `GASP_Giustino2_ue59` (mark `_ue59/IN-USO.txt`).
2. `powershell -File Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\tl1-build.log` → ESITO OK. On compile errors: max 2 fix attempts (rule 5), push fixes on this branch.
3. `powershell -File Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\tl1-test.log -Tests GSTNCombat.NP` → all pass (new: `GSTNCombat.NP.TimelinePlay.*`); regressions in other NP tests = FAIL.
4. Free `_ue59/IN-USO.txt`. Report ESITO lines + failing test names in the next thread.

## Next checkpoints
CP2: test host BP (3-phase move with choice) + PIE harness rollback check + rule-10 video. CP3: Wait Command (2A). CP4: Wait Combat Frames task + validator hints (3A). Cost this CP: ~$22 est.
