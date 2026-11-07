# Forward prediction (FP) handovers

## FP G0 (laptop, 2026-11-08): baseline measured, buffer sweep invalid (override not applied); 2 fix attempts used, stopped

- Branch `claude/fp-g0-laptop` from 6c875b6 (the plan). Sonnet runner (`Saved/FPG0/brief.md`, `results.md`, `driver.py`, `harness.ps1`).
  No repo code, no ini changed.
- Ran 8 cells (100/66/50/33 ms x 0/2 %, dedicated + 2, 60/15): throw_rider and boost_jump PASS in every cell (first 100/0 throw flaked
  once, rerun PASS); walk probe for stalls, jumps and proxy offset.
- Delays did not move with the buffer, so I added a 400 ms control and a proxy-offset metric: 45-56 cm at "400" vs 50 cm at 100,
  so the override never applied. Attempt 1: `-ini:` command-line override (the struct commas split it). Attempt 2: `set` exec command on
  the settings CDO before PIE. Both had no effect. Stopped (rule 5).
- Valid result: the S4 baseline. The boost partner sees it ~290-355 ms after the initiator (430-490 ms from the tap). A throw shows on both screens
  together but 550-630 ms after the tap (likely the throw timeline's wind-up: check in G1).
- Verdict and value: not decided. The plan file has a "G0 results" section with the table and what closes G0.
- Needed to close G0: an inert fork cvar `np.FixedTickInterpolationBufferedMSOverride` (-1 = off) read in
  `SyncNetworkPredictionSettings`, logged, listed in PATCHES.md; owner/coordinator OK (fork edit, not GSTNCombatNP). Then rerun the sweep
  (~$2-3) with the proxy offset as the "applied" check.
- PERF-DEBT: none (measurement only).
- Cost: ~$4 (runner ~147k tokens / 79 tool calls, 11 headless harness runs; my context ~+25k).
