# Handover debug-wall-switch-1: wall slide debug behind a Debug Options switch (2026-11-26)

Branch `claude/project-thread-3dyy2t`, base `main` 152d25e6. Cloud thread, NOT compiled (desktop off until Friday).
Owner 2026-11-26: the wall slide visualizer must not be always on.

## Why it was always on
The wall modes drew their shapes (capsule green/red, normal, up, velocity, probes) when the per-mode UPROPERTY `bDrawDebug` was true.
The C++ default is false, so a Mover mode instance in a Blueprint was saved with it checked: no console variable, nothing in the window.

## What
- New console variable `gstn.Wall.Debug.Draw` (0 default, 1 on) in `GSTN_WallMovementDebug.cpp`, `GSTNWallDebug::IsDrawOn()`.
  The name contains `Debug`, so the GSTN Debug Options window lists it by itself (Wall group, check box).
- `UGSTN_WallMovementModeBase::bDrawDebug` became `bDrawDebug_DEPRECATED` (no Blueprint access): the old saved value loads and is ignored.
  `DrawWallDebug` and the query settings (`FGSTN_WallQuerySettings::bDrawDebug`, probes) read the console variable.

## Two hidden switches moved into the window (owner choice A, 2026-11-26)
They were off by default but named outside the window's filter (`gstn.` prefix + `Debug` in the name), so only the console reached them:
- `a.GSTN.GroundContactFix.Debug` -> `gstn.Anim.Debug.GroundContactFix` (floor, contact spheres, lift of the ground contact anim node).
- `gstn.Platform.Draw` -> `gstn.Platform.Debug.Draw` (enemy platform shapes); README, test fighter comment and `surface_scenarios.py` updated.
Old handovers keep the old names (history). `a.GSTN.GroundContactFix` (the node on/off) is not a debug draw and stays as it is.

## Desktop steps
1. Editor closed, merge the branch, `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log` (UPROPERTY change: full build).
2. The window lists `gstn.Anim.Debug.GroundContactFix` (Anim group) and `gstn.Platform.Debug.Draw` (Platform group), both off.
3. PIE on a wall: no shapes. Tools > Debug > GSTN Debug Options, Wall group, tick `gstn.Wall.Debug.Draw`: shapes appear; untick: gone.
4. If a Blueprint graph set "Draw Debug" on a wall mode, it fails to compile: delete that node (none found in Python tooling).

ESITO: PARZIALE codice pronto, non compilato e non provato (fisso spento)
SUBAGENTI: 0
COSTO: ~2 $ | 0 | 0
