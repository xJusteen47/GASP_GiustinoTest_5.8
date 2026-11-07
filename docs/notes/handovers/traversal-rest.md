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

## T6 parts bridge (laptop, 2026-11-06, branch claude/project-thread-wlggpb)

New plugin `Plugins/GSTNPlatformParts` (README there), enabled in the `.uproject` (plan §6 lists this edit; done without asking, D-rule 6).
`UGSTN_PlatformPartsComponent` feeds `SetShapeMask` from the confirmed broken mask at the pose point; part names resolve to part + ancestor
bits (`FGSTN_PlatformPartGates`, GSTNPlatformCore, shared with T8). GSTNPlatform got three small seams: `OnPrePose` (forward and
presentation poses only), `SetShapeMaskAtFrame`, and the settings treat part names as resolved when GSTNPlatformParts is loaded.
Deviation from plan T6 (also in traversal-doc-deltas.md "T6"): clients do not read their own copy of the fragment. Run 1 did that and
the client reconciled 4-7 times per break (it stamped the change a round trip later than the host). Now the host replicates
{mask, frame} on the bridge and clients stamp the host's frame retroactively; inside the Breaking grace nothing reconciles.
Checks: build green (3 builds). `RunTests GSTNPlatform+GSTNParts+GSTNSurface` 176/176, `GSTNPlatformParts.*` 5/5 (Saved/T6/headless-3.log).
`surface_scenarios.py --phases part` 4/4 pass, client corrections [0,0] (Saved/T6/summary-part-run2.md); wall+stand+part at 2 % loss 2/2
pass (Saved/T6/summary-all-loss2.md). Limit: the scenario breaks the part with `gstn.PlatformParts.SetBrokenMask`, not with a hit (the harness
fighter has no combat state); hit-driven break and the healer restore (P7b) need a combat fighter with a flank: T9 content.
Next: T7 (M, plan $6-10, realistic $10-15: needs a combat fighter carrying a player and may touch the lag comp history in GSTNCombatNP,
so after M8 G1/G5 merge; not while M8 G10 edits GSTNPlatform). Not started here. Cost of this thread: ~$7-10 (Opus, ~120k context, 3 builds, 3 PIE runs).
