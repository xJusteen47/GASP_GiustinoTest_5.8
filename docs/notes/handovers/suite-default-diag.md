# SUITE_DEFAULT failures on L_Test_Combat: diagnosis (desktop, 2026-11-07)

Branch claude/project-thread-5t4k3a from qeaif9 (bf8562c). Build green. Runs: full SUITE_DEFAULT (50 scenarios), listen host + 2 clients,
60/15 ms 0 % loss, headless (driver = m3_suite_driver.py with NAMES = SUITE_DEFAULT, map passed on the command line). Reports: m7 worktree
Saved/SDD/report-{combat1,default1,combat_floor}.txt (untracked).

| Run | Map | Result |
|---|---|---|
| combat1 | L_Test_Combat | 43/50: spike_stats, reaction_tiers, di, loadout_change, hit_on_wall, hit_on_wall_hit, impact_wall_mode FAIL |
| default1 | DefaultLevel | **50/50 PASS**: same code |
| combat_floor | L_Test_Combat + temporary floor box (unsaved) | 48/50: only hit_on_wall, hit_on_wall_hit FAIL |

Cause: not the code and not a merge. SUITE_DEFAULT was written for DefaultLevel and has two map dependencies that L_Test_Combat breaks:
1. **Floor too small for the lanes.** `lane_offset` puts pawns LANE_SPACING = 1500 cm to -X / -Y of their spawn. On L_Test_Combat the starts are
   at x 0 / -300 and the floor ends at x -1500, so lanes 1 and 3 stand on the edge or past it (x -1800): pawns fall ("moved 1680 cm, Falling",
   apex -24 cm, owner/authority pos diff 258 cm). Which pawn gets which lane depends on the key sort, so the failing set changes from run to run
   and machine to machine (laptop 5-8 failures, different names). A pawn landing somewhere unexpected also explains the laptop's stray hit on
   pawn 3 (not reproduced here).
2. **hit_on_wall / hit_on_wall_hit use world coordinates of the DefaultLevel wall** (wall_x 1010, wall_y -880). L_Test_Combat has no wall
   there (its wall is at 2400, -900) and its flat trampoline (900, -700) sits on the climber's path. These two cannot pass on this map as written.

The M3 "green" (2jy4k8) ran only the first 29 names, on DefaultLevel: it never covered the hit scenarios on L_Test_Combat.

Fix (attempt 1 of 2, applied in the script only): `Tools/GAS/build_test_combat_map.py` floor -> centre (500, 0), size 8000 x 5000
(x -3500..4500, y -2500..2500). Verified with the equivalent temporary floor (run combat_floor). **Map NOT rebuilt or saved** (brief): rebuilding
L_Test_Combat with the builder needs an OK (it saves that map; LFS, binary).

Recommendation: gate SUITE_DEFAULT on DefaultLevel (green now), PARTS/DEFENCE/LINK on L_Test_Combat. If SUITE_DEFAULT must also run on
L_Test_Combat: rebuild the map with the new floor, and either skip hit_on_wall(_hit) there or make them spawn their own wall (a harness change, small
but not done: needs a choice of spot clear of the stations).

Not done: no map saved, nothing pushed to feature. PIE ini rewritten by setup_editor (listen, 3 players) in the m7 worktree, as in R3-1.
Cost: 1 build + 3 editor runs (~45 min machine); token cost est. ~$4.
