# Netcode step 3: proxy send dividers + gates + hover soak (desktop, 2026-11-09)

Branch `claude/desktop-netcode-step3` (from 0ceec02b, merged with the merge-branch tip 0853277f, not rebased: never force-push).
Scripts and logs: `Saved/NS3/` of the `_m7` worktree (`bytes.ps1` + `bytes_driver.py` = idle/moving per-client bytes, `launchat.ps1` = hover soak,
`run.ps1` takes `-Env GSTN_NS3_CMDS=<cmd;cmd>` to set cvars before a batch).

**Step A (0ceec02b):** build 34 s, headless 1460/0/0, SUITE_MINION ded+2 0 % 11/12 (only the known minion_lod red).
**Merged 2f54b58e/0853277f (levers 3-4):** build ok, headless 1462/0/0, SUITE_MINION 11/12 first run (player_hits_minion red on the kit-swing
check, PASS on rerun); lagcomp PASS, **minion_lod PASS** (red before lever 3).

**Step B, commit f0c7f995 (NP fork, PATCHES.md row):** `gstn.Net.SimProxySendEveryNTicks N` (default 1, console + ini) holds the replication
proxy's cached frame in `FReplicationProxy::OnPreReplication`, so `Identical()` skips the SP send until N ticks passed. Applies to every NP model
on that component (Mover, Combat, ASS). Experiment `gstn.Net.AutoProxySendEveryNTicks` does the same for AP (default 1).
Listen + 2 clients, 15 s windows, per client pre-handler KB/s (Oodle has no .udic on the desktop, so post ~= pre):
| | idle | moving (all 3 walk circles) |
|---|---|---|
| N=1 (stock) | 25.1 | 34.7 |
| SP N=2 | 18.7 (-25 %) | 28.9 (-17 %) |
| AP N=2 only | 21.4 | 32.5 |
- SP N=2: link_pair_move_delay(_throw) 12/12 each, mount_basic, minion_promote (no pop) and minion_replicate PASS.
- AP N=2: pair-move 12/12 each, mount_basic, dismount_predicted, mount_forced_reconcile PASS (5 of 12 mount scenarios, not the full suite).
- NOT checked: proxies seen in a rendered client. SP interpolation buffer is 50 ms (FixedTickInterpolationBufferedMS) and N=2 spaces SP
  updates ~32 ms + net jitter: possible starve/pop. Look at it rendered before turning N=2 on in a game mode. Defaults stay 1.

**Step C, hover soak (fallback 0, DefaultLevel -game -nullrhi):** NOT reproduced. 10 minions h500/v1200 every 30 f at (165,-845,91), 60 s + 290 s
(21.9k frames); 30 minions h250/v600 every 20 f at the lip (100,-850,240), 290 s (18.1k frames). AIR 0, trapped 0, stuck 0, fell 0.

**Next:** rendered look at SP N=2 (and decide on N=2 for PvE); delta for Combat/ASS sync; Oodle dictionaries for the desktop if needed.
Builds 3 (34 s incremental). Cost: ~120k context, Opus, estimated ~$4.
