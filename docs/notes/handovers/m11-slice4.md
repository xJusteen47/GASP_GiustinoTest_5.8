# M11 slice K4: pre-playtest network check of the kit work (desktop, 2026-11-11)
Branch `claude/desktop-m11-4` from `253e11d2` (tip of `claude/desktop-m11-3b`). Plan: `docs/architecture/plans/m11-kit-piano-a-fette.md` K4.
**Checked**: K1 kit swap, K2 kit throws, K3 three test kits, 3b buttons as data. No gameplay code changed.
**Found and fixed (in slice, 2 attempts)**: the Game target (Development, Shipping) did not build. Not kit code: tests in 9 plugins called
editor-only API (`GetTagEditorData`, `RerunConstructionScripts`, `IsDataValid`, struct metadata, `EditorGetAssetTags`) and Shipping compiled
reflected test stand-ins whose bodies need test-only tags (`GSTN_InputTests_Blueprint.cpp`) or sat inside the test guard (two weapon stats
constructors in `GSTN_CombatNPTests_Stats.cpp`). Guarded with `WITH_EDITOR` / `WITH_METADATA` / `WITH_DEV_AUTOMATION_TESTS`; editor checks
unchanged (commit `037e4ad8`). Nobody had built the game target since these tests landed: keep one Shipping build in each closing check.
**Measured** (all reports in `Saved/Logs/m11-k4/`, driver `Saved/M11k4/harness.ps1`, `batch_driver.py`, fresh PIE per batch):
| Run | listen 0 % | listen 2 % | dedicated 0 % | dedicated 2 % (new) |
|---|---|---|---|---|
| `SUITE_KIT` + `SUITE_ARCHETYPES` (8: swap hold, stick throws fwd/back/lifted, stats, light/medium throws, heavy lift) | 8/8 | 8/8 | 8/8 | 8/8 |
| `SUITE_HELD` (8) | 8/8 | 8/8 | 8/8 | 8/8 |
SUITE_DEFAULT one PIE (`R31_ALL=1`, run_suite) **50/50**, 854 s. Headless GSTN **1508/0**. 0 ensures in every run.
Builds: editor 9-13 s (incremental); Game Development 79 s first, 26 s after fix; Shipping 66 s, then 19 s, 16 s. Harness batch ~5.5 min, suite ~14 min.
The plan's `scenario_kit_pickup` / `scenario_kit_throw` are `kit_swap_hold` and `kit_throw_*` (names from K1-K2).
**Unseen in PIE**: nobody has looked at any of it with eyes or a pad: the "Tieni B" prompt, the swap, the throw arcs and the three kits' feel,
the kit dropped at death, a moved button. Every run is headless (`-nullrhi`, scripted input). Owner steps: artifact "Cose da fare a mano"
point 2, "Playtest dei kit col pad" (one list now, replaces the three slice items; mirror in `docs/notes/owner-manual-todo-artifact.md`).
**Not covered**: kit death drop and focus-vs-cube priority have unit tests only (no harness scenario); the game executables were compiled, not cooked or run.
**Open**: script slip of this thread: a held batch first ran with an empty name list (0/0), rerun with the names, fine.
Still-fixed buttons in other plugins: list in `m11-slice3b.md` (separate slice, ~$3-5 each or one ~$6-10 sweep).
Cost: ~110k context, Opus, estimated ~$5-7. Wall clock ~2 h 30, mostly machine time.
