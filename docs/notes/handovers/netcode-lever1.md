# Netcode lever 1: players' replication traffic (desktop, 2026-11-09)

Brief: `docs/notes/reviews/netcode-nemici-confronto-2026-11-09.md` (L2 there). Scripts, logs, notes: `Saved/NL1/` of the `_m7` worktree.

**Result (listen host + 2 clients, real wire from the `MinionBench net` line):**
- Players idle, N=0: **50.2 -> 21.7 KB/s per client (-57 %)**; N=80 Chase adds ~0-2 KB/s as before.
- Players active (mount/pair-move suite, 400 s): NP bytes per update, Mover SP 243 -> 52 B, Mover AP 179 -> 26 B; all NP models
  together ~-47 % per update vs the idle baseline (no clean moving wire number before the change: moving base not measured).

**Where the bytes were (np.Debug.NetBytes, NP = 89 % of the wire):** Mover sync 200 B/update to SP, 164 to AP. Inside it:
`FSmoothWalkingState` 128 B (5 full doubles), input copy 31, BP user struct 18, mode FName as a string 16, wall state 16, modifiers 15.
Next heaviest after the fix: Combat sync 43 B (SP+AP), ASS AP sync 42-108 B, Mover SP input 31 B.

**Changes (fork code, comments in place, PATCHES.md and Mover/README.md rows):**
1. `SmoothWalkingState.cpp`: quantized (PackedVector<10,24> x4 + rotator short): 128 -> ~5 B. Far below the reconcile tolerances.
2. `FMoverSyncState::NetSerialize` + `FMoverDataCollection::NetSerializeDelta`: against the NP delta base (acked frame, both sides),
   mode name, modifiers and each collection element cost 1 bit when their wire bits equal the base. NOT for the input: AP acked
   frames keep no input, so after a role change (mount) the base differs (found by SUITE_MOUNT: "delta element 0 has no base").
3. Debug counter `np.Debug.NetBytes 1` / `np.Debug.NetBytes.Dump <label>` (off by default) -> `Saved/Logs/NPNetBytes.txt`;
   `minion_bytes` snaps dump it.

**Gate:** headless GSTN 1459/0/0, 0 ensures. SUITE_MOUNT listen/3 2 % 12/12 (incl. link_pair_move_delay(_throw)), 0 delta errors.
SUITE_MINION dedicated+2 0 %: 11/12 after rerun (player_hits_minion, reaction_tiers, lagcomp red on 1st run, PASS on rerun);
**minion_lod red twice and red on the base build too** (stash + rebuild: tier chain 0/12): pre-existing, not this change. minion_promote
(no pop) PASS. Reconcile log lines unchanged (28 vs 28 idle).

**Perf-debt / next (not started, context budget):**
- Step 3 approved by the owner: cvar `gstn.Net.SimProxySendEveryNTicks` (default 1) = host sends Mover state to SP every N ticks;
  (b) AP every 2 ticks only as a measured experiment. Sim rate and client input rate never change. No game-mode hookup.
- Delta for Combat and ASS sync (43 + 42 B/update), and for the SP input if AP acked frames start keeping the input.
- minion_lod red on feature (tier chain): needs its own look.

Builds: 7 (5-35 s each, incremental). Cost: ~150k context, Opus; estimated ~$4-5.
