# M11 chain on UE 5.9 + L_Test_Hub as startup map (desktop, 2026-11-13)
Branch `claude/project-thread-vevoas` from `8eab9ced` (5.9 cut, owner pushed it to `feature/gas-npp-5.8`), worktree `GASP_Giustino2_ue59`.
**Merged**: `origin/claude/project-thread-wmlass` (`fbf98070`, L7d tip). It already holds L7b, L10 (`60cg1f`), L7c (`360h11`), SWAP
(`claude/laptop-m11-swap`) and L9 (`emiydi`, `9e2ee9ce`): nothing left out. One conflict, `Config/Tags/GSTNCombatTest.ini`: both sides' tags kept
(M11 verify kit/mod/backpack tags + L10 Ranger/Power tags).
**Owner ask**: `EditorStartupMap` and `GameDefaultMap` = `/Game/Levels/Test/L_Test_Hub`. The harness relied on the startup map (the suite PIEs on
the open level), so `m3_suite_driver.py` now loads `/Game/Levels/DefaultLevel` first (`R31_MAP` overrides). Other headless drivers that PIE on
the startup level (defence/minion/mount batches, `Saved/*/run.ps1`) must do the same or open the map; not changed here.
**Hub**: `test_hub.spawn_backpacks()` (in the Play setup) puts `DA_Backpack_JetPack` at (800,-1050) and `DA_Backpack_ShieldDome` at (800,1050)
as backpack pickups, so the owner can try them by just pressing Play. Checked headless: both in all 3 worlds (`Saved/UE59/bpk_check.txt`).
Taking one was not driven here (m11-verify already took a backpack pickup by input).
**Gates (5.9)**: Editor build ok first try. Headless GSTN **1583/0**, 0 ensures (`Saved/UE59/headless_vevoas.log`). Suite `R31_ALL=1` on
DefaultLevel **49/50**, 0 ensures: `jump_squat` red (client kit not applied yet, known flaky), PASS alone (`Saved/R31/report-vevoas-*.txt`).
The editor started on L_Test_Hub, the hub auto-prepare ran, then the driver loaded DefaultLevel: harness still fine.
**Owner step**: push the tip to `feature/gas-npp-5.8` (fast-forward from `8eab9ced`), artifact "Cose da fare a mano" section 0.
The laptop must not pull until the coordinator says so (loadout screen in progress there on 5.8).
**Not done (by brief)**: backpack mesh not following the back; loadout screen. `PM_BoostJump`/`PM_ThrowRider` resaved by 5.9 stay uncommitted.
Nobody watched the backpacks, the dome or the Ranger in a window.
Cost: ~110k context, Opus, no subagents (build/tests were single background commands printing 2-5 lines); estimated ~$9-12. Wall clock ~50 min.
