# Traversal leftovers: T2 close-out (laptop, 2026-11-06)

Branch claude/project-thread-wlggpb from be6ff01 (R3-1). No code change.

T2 was listed as "paused" in resto-m3-m4b-m5-t.md, but that audit read the commit 4041401 ("T2, paused"). The follow-up 447fc2d
(already on this branch) ran every check the pause had left open: surface scenario at 2 % loss, `wall_ab.py` moving lanes, the v2 wall
scenarios at 0 % and 2 %; design and numbers in wall-slide-v2.md 6.15 and traversal-doc-deltas.md "T2". The plan's class
`UGSTN_SurfaceWallEntryTransition` and tests `GSTNMovement.Wall.Surface.*` were replaced by `UGSTN_WallReentryUtils::Judge` and its tests (documented).

Re-check today on the current code (build up to date):
- `Automation RunTests GSTN.Wall+GSTNMovement.Wall`: 90/90 Success, 0 Fail (Saved/T2/wall-tests.log, untracked).
- `python Tools/GASNPPTest/surface_scenarios.py` (60/15 ms, 0 % loss, listen host + 1 client, both subjects, 2 reps, memory kept):
  4/4 pass; client `launch_to_grab` 1 unexplained per rep (the harness's own teleport, as in 6.15), host 0 (Saved/T2/surface.txt).

T2 is closed. Next traversal gates in plan order: T4 (needs M8 G1/G5 merged: GSTNCombatCore), T6/T7 (GSTNPlatform; not while M8 G10 runs), T9.
Cost: ~$1 of tokens (reading + two headless runs).
