# TH-1 audit: what the test hub can use (2026-11-06, worktree m7, branch `claude/project-thread-f6jper` from M7-15 `8154056`)

**Base.** `feature` does not have M7 yet (step 0 of sequenza-2026-11-06 not done), so the hub is built in the m7 worktree, where the
`.uproject` already enables GSTNDefence, GSTNLink, GSTNSpinDrill, GSTNPiggyback, GSTNPlatform, GSTNLevelObjects. `Content/AnimPipeline` is not
versioned: m7 gets a junction to the main checkout's folder (same as the `_feature` worktree), so Z6 sees the pilot sequences.

**Level actors available (all placed by `Tools/GAS/build_test_hub_map.py`).** `AGSTN_Trampoline`, `AGSTN_KOZone`, `AGSTN_TestSurface` (glass),
`StaticMeshActor` with `PM_Test_Wall` / `PM_Test_Glass`, wall slide plates and start markers from `build_wall_slide_ab_map.build_plan()`.
`AGSTN_PlatformTestFighter` is spawned at run time only (`gstn.Platform.SpawnTestFighter`, `h.fighter()`); `AGSTN_WallTestMover` exists but the
two hub lanes (flat, 90 deg convex) do not use it.

**Kits.** `KIT_Test_Hits` (+ spin bindings and drill extension in memory, `spin_drill_scenarios.prepare_editor`), `KIT_Test_Link`,
`KIT_Test_Mount` / `KIT_Test_MountHeavy` (GSTNPiggyback Content/Test, created and saved by `mount_scenarios.prepare_editor` when missing),
`KIT_Test_Defence*`, `KIT_Test_Alpha/Beta/...`. Shield button added in memory by `defence_scenarios.prepare_editor`.

**Animations (Z6).** Jab v12 `AS_Pilot_JabV12` (no `AS_JabV12_Build` in Rig), ftilt v31 `Rig/AS_GanonFTiltV31_Build`, dair v4 VarM
`Rig/AS_OverlordDAirV4_Build_VarM` (V5/V6 builds also exist: "current" taken as VarM, the last brief). None of them is referenced by a kit,
so Z6 shows loops only: no live version on a target.

**Missing (not blocking, not built).**
- No dummy actor: targets are the PIE pawns parked by `test_hub.park()`, moving ones by `walker()` and the platform test fighter.
- No non-drillable static geometry: "not drillable" is the test immunity effect on a player (`h.drill_immune(i)`); the Z4 block is only a marker.
- No `Role.Rider` attack binding on any kit (M7-06): an attack from a mounted rider does nothing yet.
- Rider pose: AnimBP IK nodes still TODO (M7 G10), the rider pose is expected to look wrong.
- Pilot moves are not in any kit (see above).
