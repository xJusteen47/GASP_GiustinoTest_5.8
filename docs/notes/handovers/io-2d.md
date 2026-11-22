# IO-2d: editor exit crash fixed, final gate (desktop, 2026-11-12)
Branch `claude/desktop-io-2d` from `9f9589b1` (tip of `claude/laptop-io-2c`). Handover before: `io-2c.md`. Worktree `GASP_Giustino2_m7`.
**Cause**: `UGSTN_HeldSettings::RemoveFromInputSettings()` (`GSTNHeldCore`) called `GetMutableDefault<UGSTN_InputSettings>()` from
`FGSTNHeldRuntimeModule::ShutdownModule()`, which at editor exit runs after the UObject system is torn down: dead class default object,
access violation 0x0 at every quit. GSTNCombat and GSTNLink already guard the same call with `UObjectInitialized()` at their call sites;
GSTNHeld did not. Reproduced on the desktop on the first try (commandlet start + `QUIT_EDITOR`, exit 3, same stack as the laptop).
**Fix** (`70ff3344`): the guard inside `RemoveFromInputSettings()` itself, so any caller is safe. One attempt.
**Verified**: 3/3 clean quits (exit 0, no new crash folder), plus the headless run and both suite editors quit with exit 0 and no crash.
**Gate on the final code**: build ok; headless GSTN **1514/0**, 0 ensures (`Saved/IO2d/headless-t1.log`); suite `R31_ALL=1` **49/50**,
0 ensures (`Saved/R31/report-io2d.txt`): `combat_spike` red once (hitstop: owner pawn moved 6.3 cm in the window), PASS when rerun alone
(`report-io2d-spike.txt`), so a flake, not this change (shutdown only). Shipping `GASP_Giustino2` build ok (`Saved/IO2d/build-ship.log`).
**Project Settings note (not changed, architecture question)**: what this function undoes is `UGSTN_HeldSettings::CommandTiers`, the held
command priority tiers, read from the plugin's config (`DefaultGSTNHeld.ini`, shown in Project Settings > Plugins > GSTN Held) and pushed
into the global `UGSTN_InputSettings` contributions at startup, like GSTNCombat's and GSTNLink's tiers. Tiers are input data in a
project-wide settings object, not per player / mode / kit. The fix keeps that as is; moving tiers into the button layout / GameRules or
the kit is a design decision for the owner and coordinator.
**For the owner**: push tip of this branch (handover commit) to feature: fast-forward from `747ce8b9`.
Scripts: `Saved/IO2d/{build,quit,headless,suite,suite-spike}.ps1` (not versioned).
Cost: ~95k context, Opus, estimated ~$4-6. Wall clock ~45 min (2 editor builds, 1 Shipping, 7 quit runs, headless, suite 16 min + rerun).
