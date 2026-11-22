# Depth marker holo (M1 extra item), branch claude/project-thread-kup1kk
Merge of origin/claude/project-thread-rwaob2 (56c87963) = 635a70b5; holo materials 6fe4e6ea (setup_depth_marker_holo.py needed
`-AllowCommandletRendering` instead of `-nullrhi`, log Saved/Logs/ci/holo2.ue.log: "GSTN_HOLO FINISHED OK").
Build: Run-Timed Build, ESITO OK (Saved/Logs/ci/m1_build.log, Result: Succeeded).
Tests (Run-Timed Test, ESITO OK, log Saved/Logs/ci/m1_test.log.ue.log, 251 Success, 0 Fail/Skipped; count command:
`Select-String "Result=\{Success\}.*Path=\{<prefix>"`): GSTNDepthMarker.Mover 3, GSTNDepthMarker.Runtime 33 (includes
Runtime.ProjectionPrecedence 1), GSTNDepthMarker.Core.ProjectionDirection 1, GSTNCombat.NP 209, GSTN.Visual.DepthMarker 5.
No Visual snapshot diff was reported (the run is headless; the new look may only differ in a rendered run).
Capture (PIE L_Test_Hub, Tools/GASNPPTest/dm_cap_m1.ps1 + pie_capture_move.py sequences dm_circle_jump / dm_projection, log
Saved/DepthMarkerM1/cap2/capture.log): `marker_circle_jump.mp4` (octagon run + jump, 320 frames, 60 fps, game + az90 views),
`marker_game_grid.png` (game camera, v20..v300 step 20; the ring and dashes show in the jump v140/v160, dark ring on the blue floor).
NOT captured: the sideways projection volume. dm_projection ran but the volume spawn failed
(`GameplayStatics.begin_deferred_actor_spawn_from_class` does not exist in Python), so that clip shows no volume; nothing to review there.
