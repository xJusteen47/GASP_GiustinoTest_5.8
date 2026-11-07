# Handover: integration branch merge-feature-2026-11-07

Branch: `claude/merge-feature-2026-11-07` (worktree GASP_Giustino2_m7).

## What it contains
- Base: `origin/feature/gas-npp-5.8` at 60da6fa.
- Merge 1: `origin/claude/project-thread-kizkcl` (63f8450): TH-2 test hub, NP interpolate fix, Xbox IMC. It already contains 7autp9 (1fa9300, M7 G7-G12) and 8154056 (M7-15), so no separate merge was needed. No conflicts.
- Merge 2: commit 3312333 (owner's local feature tip: SD-16 handover, 3866040 bore tip no hitstop, a02c69e SD-15 handover; spin/drill lineage, on origin via pk0j51). No conflicts.
- origin/feature had not moved at the end of the work (still 60da6fa).

## Verification
- Editor build (GASP_Giustino2Editor Win64 Development) after each merge: Succeeded.
- Headless Automation `RunTests GSTN`: 1291 tests found, 1291 completed, 0 failed.
- Network (PIE) suites NOT run, as instructed.

## Owner action
Fast-forward/push this branch's final sha to feature/gas-npp-5.8 (push to feature was denied for the thread).

## Cost
Roughly 1-2 USD (Sonnet, a few dozen calls).

## M8 merge (laptop, 2026-11-07): feature + 5t4k3a + M8 chain (G0-G12a) on `claude/m8-merge-feature`, gate green, SUITE_DEFAULT 50/50

- Base: origin/feature/gas-npp-5.8 at 8d51747 (PRs #30/#31; 41 commits after 60da6fa). Merged in order:
  1. `claude/project-thread-wlggpb` (a97ef43 T6 on 90fcc9f E15/E16): already an ancestor of the M8 chain, SKIPPED as its own merge (comes with 3).
  2. `claude/project-thread-5t4k3a` (5555a23, builder floor + SUITE_DEFAULT diag; carries the early M8 G0-G2 commits): merge cbeb503. One conflict:
     docs/architecture/plans/README.md (feature's new plan rows vs the M8 row): kept both, dropped "M8 ... PR #26" from the PR sentence.
     My resolution first went through the ANSI codepage (mojibake on the section signs): fixed in ba9c8f8.
  3. `claude/m8-g12-laptop` (6a4a495, the whole M8 chain qeaif9 -> ... -> g11 -> g12a): merge 0eb0cda, no conflicts (auto-merged GSTN_KnockbackModel.cpp,
     GSTN_HitData.h, SpinDrill NP/Core files, spin_drill_scenarios.py, combat-m4-hits.md, spin-drill handover).
- Gate (UBT full build each, `Automation RunTests GSTN` headless, logs Saved/M8G12/headless-m*.log): all on 1398/0; GSTNMinion off 1307/0;
  GSTNPlatform+GSTNPlatformParts off 1376/0; 0 ensures. Counts equal G12a's: the 41 feature commits add no GSTN automation tests.
- SUITE_DEFAULT (50 scenarios) on DefaultLevel, listen host + 2 clients, 60/15, 0 %: 50/50 PASS (Saved/M8G12/report-sd0.txt, driver = a copy of
  m3_suite_driver.py with NAMES = SUITE_DEFAULT). Minion scenarios not rerun on the merge (no minion code changed by feature; G12a run stands).
- Red / not run: nothing red. Not run here: SUITE_PARTS/DEFENCE/LINK on L_Test_Combat, minion suites on the merge (desktop G12b sweep).
- Owner action: push the tip of `claude/m8-merge-feature` to feature after review. The exact sha is in the thread report (this file is part of it):
  `git push origin <tip sha>:refs/heads/feature/gas-npp-5.8` (fast-forward from 8d51747). Not pushed to feature by me.
- Second commit on the same branch: docs/notes/reviews/perf-debt-review-2026-11-07.md (43 items: 17 fix before PvP, 12 toaster pass, 14 accept).
- PERF-DEBT: none new from the merge.
- Cost: ~$5 (merges, 4 builds, 3 gate test runs, 1 SUITE_DEFAULT run, Haiku review ~88k tokens).
