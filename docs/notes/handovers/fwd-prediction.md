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

## FP G0 close-out (laptop, 2026-11-08): sweep valid, verdict "lightweight path cannot reach 150 ms"

- Coordinator OK for the inert fork knob `np.FixedTickInterpolationBufferedMSOverride` (-1 = off, dev cvar, Shipping const): read in
  `UNetworkPredictionWorldManager::SyncNetworkPredictionSettings`, logged, PATCHES.md §4 (a test knob, does not pre-empt decision 3).
  Built (editor target). Not gated with the GSTN headless run: the knob is off by default and the path only reads it at world init.
- Sweep 100/66/50/33 x 0/2 % + 400 ms control, all applied (15 log lines per run), all scenarios PASS. Table and reading in the plan,
  "G0 results" (replaces the invalid first sweep).
- Result: the buffer moves the proxy offset by ~10-15 cm (100 -> 33 ms) and not the boost delay (~320 ms, noise 259-366). 33 ms triples the stalls.
  Recommended 50 ms (decision 1). Verdict: G1-G2 cannot reach 150 ms; remaining gap ~150-170 ms. The throw's 540-620 ms from the tap
  is most likely its timeline wind-up (G1 note).
- Next: decompose the boost delay by hop before G3-G5 (~$2-3).
- PERF-DEBT: none (test knob, one cvar read per world init).
- Cost: ~$3 for the close-out (1 fork build, 10 harness runs, my context ~+20k); FP G0 total ~$7.

## FP decisions + G0b (laptop, 2026-11-08): owner answers in the plan, buffer 50 ms set, hop breakdown done

- Owner answers (D1-D5) written in plan §6; gate table §4 revised: G1 shares a per-link "partner window" with G4 (no redo), snap on
  misprediction accepted, new G3b (client hit inbox for link partners; the only `GSTNCombatNP` change of FP), all before M10.
- D1 applied: `FixedTickInterpolationBufferedMS=50` in `Config/DefaultNetworkPrediction.ini` (the NP settings file; the brief said
  DefaultEngine.ini, which does not hold this setting). Not gated with the headless GSTN run (config value only; the scenarios below ran with it).
- G0b (3 runs at 0 % and 2 %, dedicated + 2, 60/15): boost = hop 1 rider screen -> server ~180 ms (0 %) / ~218 ms (2 %), the client lead
  (input travel + server input buffer); hop 2 server -> carrier screen 70-128 ms. Throw: server releases at 470-484 ms (input path +
  wind-up). Table in the plan, "G0b hop breakdown".
- Re-estimate: G1 reaches the 150 ms initiator target by itself. G3-G5 cut the partner gap only ~30-60 ms (to ~200-260 ms, about one round
  trip); hop 1 is the bigger lever (server input buffer settings), not measured yet and not FP.
- Tooling (not versioned): `Saved/FPG0/hops.ps1`, driver hook `_spike4_hops`, `FP_NAMES` env.
- PERF-DEBT: none new (the plan's §7 lists the expected debt; 50 ms buffer stalls to recheck on the desktop).
- Cost: ~$2 (6 harness runs, my context ~+20k). FP total so far ~$9.
