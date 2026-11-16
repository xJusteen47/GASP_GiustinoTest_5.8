# Handover: timeline nodes, checkpoint 2 (2A Wait Command + 3A Wait Combat Frames task + validator hints)
Thread cloud, Opus high, 2026-11-16. Base `claude/project-thread-wptsyy` 118afe21 (CP1, not compiled). Branch `claude/project-thread-0kjg39`.
**State: code + unit tests written, NOT compiled (no UE in the cloud). Build it together with CP1 (same branch history).**

## Done
- `GSTN_CombatWaitTasks.h/.cpp` (GSTNCombatNP): fork prediction tasks, pins in their own sync data (rollback restores them), combat clock.
  - **Wait Combat Frames**(Frames) -> On Finish. Old `GA_Base::WaitCombatFrames` kept (display name now "Wait Combat Frames (Event)").
  - **Wait Command**(Command, Window Frames; advanced: Buffer Frames -1 = slot's, Consume on, Priority Before Bindings, Condition Always,
    Only Trigger Once on) -> On Command(FramesWaited) / On Window Expired. Matches with the slot's rules on the buffer; not in hitstop
    (fires the frame after), not in a reaction, not on a locked command; a kept press fires once (LastFiredInputFrame).
- Component: `FindCommandIndex`, `CanTakeCommandForWait`, `IsCommandEnteredForWait`, `ConsumeCommandForWait` (WriteSyncState from the ability
  step, same path as CP1's StartTimelineFromAbility). `CanExecuteBinding` refuses bindings of a command an active Wait Command claims (slot keeps
  the press). `FGSTN_CombatTimelineLogic::IsConditionMet` exposed. `AbilityStepCancelConditions` keeps the tick's extension conditions.
- Validator `FGSTN_SimGraphRules::FindTaskHint` + `IsActorSpawn`: GAS tasks -> error with the combat equivalent; fork raw-input/montage/
  seconds tasks -> warning `Ability.SimTaskHint`; Spawn Actor -> `Ability.SimSpawn`; our 3 tasks pass. Docs combat.md §11.3.
- Tests: `GSTNCombat.NP.WaitTasks.{CombatFrames,Command}`, `GSTNCombat.Editor.SimGraph.TaskHints`.

## Choices applied (rule 6, to list to owner)
1. Priority default Before Bindings (a stance/follow-up owns its button). 2. Wait Command ignores cancel windows (the graph decides), but
never fires in hitstun or on a locked command. 3. No separate "Wait Command Release" node: hint points to End On Trigger Release / Get Button
Held Frames. 4. GAS ability tasks in a sim graph are now an ERROR (were warning).

## Risks to verify on the desktop
- Consume written from the ability step (WriteSyncState) under rollback: PIE harness with lag on a host BP (same check as CP1).
- Claim read from ASC tasks inside the combat tick (end of previous frame): a press on the frame the node starts can still fire a binding.
- UHT: int16 UPROPERTYs in task data, enum default params on the fork K2 node; Command index goes stale across a kit swap (moves end there).

## Desktop steps (fisso, editor CLOSED, ~10 min, Haiku)
1. `git fetch origin claude/project-thread-0kjg39 && git checkout claude/project-thread-0kjg39` in `GASP_Giustino2_ue59` (mark `_ue59/IN-USO.txt`).
2. `powershell -File Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\tl2-build.log` -> ESITO OK. Max 2 fix attempts, push here.
3. `... -Preset Test -Log Saved\Logs\ci\tl2-test.log -Tests GSTNCombat.NP` then `-Tests GSTNCombat.Editor.SimGraph` -> all pass.
4. Free `_ue59/IN-USO.txt`; report ESITO lines + failing tests. Next: CP2 test host BP (3-phase move with Wait Command choice) + PIE rollback + rule-10 video.
Cost this CP: ~$15 est.
