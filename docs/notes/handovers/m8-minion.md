# Handover M8 minion

## G0a (laptop, 2026-11-06): DONE, gate green

- Start: local feature 3312333 (SD-16 + 5 SD commits, unpushed; M7 G7-G12/M7-15 not merged, step 0 not done). Branch `claude/project-thread-xhi73b`.
- Pre-gate (SD-16's skipped one): headless on 980/0, plugin-off build + suites 933/0, rebuild on OK (`Saved/SD17/gate.txt`). Recorded in spin-drill.md.
- S0: X1..X14 re-checked, none worse than written. X2 has no kind field: the kind rides the EntityId bit of X5, as planned.
- Spike 2: hook point found (after the lag-comp capture in the fixed-tick loop, and after `StepRollback`); no delegate exists, so G1 applies
  the inert `OnPostFixedTick(ServerFrame, EndTimeMS, bResimulating)` patch (text in the report). Time map: frame F ends at `F*FixedStepMS`;
  after the tick the authority "now" is 1 frame behind the newest history stamp. Measured error 0 frames (round trip 0.47). Report:
  `docs/notes/m8-spike-report.md`, combat.md §17e.
- Side finding for FP: `GetSyncStateAtTime` ignores `Offset` (fine on server, wrong by Offset frames on a client).
- Leftover: scratch test `Source/GASP_Giustino2/Private/ScratchSpike/GSTN_SpikeFixedTickTest.cpp` is untracked and must be deleted by hand
  (delete was blocked by permissions); the next build drops it from the binaries.
- Not done (per owner): nothing beyond G0a. **G1 waits for G0b** (desktop, spike 1 baseline on a low-end budget, needs L_Test_Hub).
- Cost: one Opus thread, ~45 tool calls, 2 gate builds + 1 scratch build (~1 h machine), ~$5.
