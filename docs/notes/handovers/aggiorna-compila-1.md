# Aggiorna e compila (one-click update + build), 2026-11-28

Owner ask: a script that brings his project to the latest main and builds the editor, so updates stop causing problems.

## Done
- `Aggiorna e compila.bat` (project root, next to `Apri editor 5.9.bat`) runs `Tools/CI/Aggiorna-Progetto.ps1`.
- Steps: editor for this project closed (never closes it; `_ue59` lock checked in that worktree), local changes into
  `git stash push -u -m aggiorna-<date>` (never applied, dropped or committed), owner-local files (`owner_local_patterns` of
  `protected-paths.json`, e.g. `DA_Locomotion_Default`) copied to `Saved/Backups/Aggiorna/<date>/` and put back after the pull,
  `fetch` + switch to main + `merge --ff-only` + `git lfs pull`, engine from `-Engine` / `UE_ENGINE_DIR` / registry / launcher list,
  build through `Run-Timed.ps1 -Preset Build` (log `Saved/Logs/ci/aggiorna-build.log`). Switches `-SenzaBuild`, `-SoloBuild`, `-Progetto`.

## Tests (desktop, Remote Control)
- First version recursed (helper named `Git` shadowed git.exe): fixed in a652b3e7 (`Invoke-Git`, `git.exe`).
- Temp clone: A1 local changes + DA file -> stash, ff to origin/main, DA restored; A2 other branch -> main; A3 local commit on main -> refused, exit 1. All PASS.
- Owner project: 82dfd88d -> 02392e6e, build 74/74 actions, `ESITO: OK` in 202 s, `REGRESS: OK` (`Saved/Logs/ci/aggiorna-build.log.esito`).
- Leftover on the desktop: `%TEMP%\agg-test` (throwaway clone, delete by hand).
- Noted, not fixed: C4996 `FCoreDelegates::OnPostEngineInit` deprecated in `Plugins/GSTNSpinDrill/.../GSTNSpinDrillGASModule.cpp` lines 31, 37.

ESITO: PASS script aggiorna+build scritto e provato sul fisso (clone di prova 3/3, progetto vero aggiornato e compilato)
SUBAGENTI: 1 sessione RC sul fisso, test, Opus
COSTO: ~2 $ | 10 min desktop | 0
