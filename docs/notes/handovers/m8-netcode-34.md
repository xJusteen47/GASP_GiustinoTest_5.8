# M8 netcode levers 3 + 4 — laptop, 2026-11-09, branch claude/laptop-netcode-34 from 8cb155f7

## Lever 3: pack-relative minion positions (lossless)
- `FGSTN_MinionNetAnchor` (Core codec): per pack stream, per-axis min of the 19-bit PositionQ + per-axis width (5 bits) covering the pack
  extent; items send `PositionQ - Min` at that width. The receiver rebuilds the same PositionQ, so decoded positions, interpolation, pops are
  unchanged by construction. `FGSTN_MinionNetArray::NetDeltaSerialize` writes 1 flag bit, then serializes the FastArray twice (relative and
  absolute) and keeps the smaller (header 72 bits), so it never costs bits. Sender-only cvar `gstn.Minion.Net.RelativePositions` (default 1);
  `gstn.Minion.Net.AnchorStats` prints/resets streams, bits sent vs all-absolute. Core test `GSTNMinion.Core.Net.PackRelativePositions`.
- Attempt 1 (header always) gave ~0 at N=80; attempt 2 = two-pass. Counter: 99 % of streams relative, 29 bits/item vs 57, **-10.5 % minion bits**.
- Measured, minion_bytes Massacre N=80/N=40, T0 10 Hz, window 15 s, 2 clients, minion part per client post-handler (KB/s), noise ~±5 %:
  Oodle off: before 8.4 / 5.6, after 7.6 / 5.6. Oodle on: before 4.9 / 3.3; after 5.1 / 3.4 (old dict), 5.1 / 2.9 (dict retrained on new layout).
- Reading: with Oodle the gain is gone (Oodle already squeezed the redundant high bits of absolute positions). It only pays where Oodle does
  not compress: Shipping today (no packaged .udic) or a machine without dictionaries. Default left ON (never more raw bits); set the cvar to 0
  if a later mixed-capture dictionary shows on > off. Server cost: one extra FastArray serialize per pack send (<=16 items).
- Content/Oodle dictionaries are now the retrained ones (unversioned); old pair in Saved/NC34/udic-moved. PERF-DEBT: two-pass serialize per pack/conn.

## Lever 4: host suitability
- `FGSTN_HostSuitability` (GSTNCore/Net): cores, CPU brand, 3 ms single-thread benchmark (150 minion-like bodies x 2000 steps, best of 5),
  optional upload KB/s -> Score 0-100 (0.7 CPU + 0.3 cores, x upload factor) and bEligible (>= 4 physical cores, upload >= 100 KB/s if known).
  Console `gstn.Net.HostScore [UploadKBps]`. Laptop i7-10870H: bench 2.77 ms, score 66 (reference machine = 50). Test `GSTNCore.Net.HostSuitability.ScoreOrder`.
- Lobby later: each member runs Measure() at lobby join (plus an upload probe to the session backend if available), publishes Score+bEligible
  in its session member data; the lobby leader picks the highest eligible score, ties to the current leader; re-run on member change, not mid-match.
## Oodle CPU per packet: NOT measured
- CsvProfile started on the command line only covers editor startup (31 s, no Oodle column) even with CsvProfile Stop before quit. Next try:
  `CsvProfile Start` from the driver after PIE starts, column Exclusive/GameThread/Oodle_OutCompressTime / server OutTotalPackets.
## Gate
- Build ok, headless GSTN 1462/0/0. SUITE_MINION 0 %: 9/12 first pass; reaction_tiers PASS on rerun; player_hits_minion and lagcomp failed twice on
  the kit-swing check ("kit swing came out on the server" empty), and lagcomp failed the same way with RelativePositions=0: pre-existing flake,
  not lever 3. Desktop rerun advised. Laptop build ~2.5 min; harness runs ~3 min each.
- Cost: ~$3 (lead only, no subagents), ~1 h 45 min laptop time, 3 builds, 8 harness runs, 1 dictionary training. Scripts in Saved/NC34.
