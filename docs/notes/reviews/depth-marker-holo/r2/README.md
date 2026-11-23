# Depth marker look, round 2 (Fable FAIL round 1)
Same clip and camera as round 1 (PIE L_Test_Hub, dm_circle_jump, game view v20..v300 step 20): `marker_game_grid.png`, `marker_circle_jump.mp4`.
Changes (data only, no logic): M_DM_Holo is now translucent unlit, Intensity 4, Whiten 0.55, scanline strength 0.4, thin dark outline (Fresnel rim,
OutlineStart 0.62, OutlineDark 0.9); GSTN_DepthPresenter_Ring.ThicknessCm 3 -> 7.5 (x2.5), Line.WidthCm 2.5 -> 5 (x2) (header defaults);
decal DiscAlpha 0.3 -> 0.45, RingWidth 0.05 -> 0.09, Intensity 3.
ROOT CAUSE of the round-1 "dark ring": the material lacked the `used_with_instanced_static_meshes` usage flag (ring/dashes are ISM cubes), the editor log said
"Material M_DM_Holo missing usage flag InstancedStaticMeshes! Default Material will be used in game", so the holo look never rendered. Fixed in
setup_depth_marker_holo.py; the warning is gone in Saved/DepthMarkerM1/cap4/editor.log. In round 2 the ring and dashes show bright (whitish pink: the player colour
lerped 55% to white). Check the dark outline at full resolution in the mp4.
NOT verified: the ground blob decal is not visible in these frames (no load warning for it, but I could not see it under the character on the blue floor;
the 2-attempt limit is used). Needs a top-down look or a decal debug. The sideways projection volume is still not captured: no PIE-world spawn function
exists in Python (GameplayStatics has no begin_spawning/deferred spawn; log lists only spawn_decal/sound/...); it needs a Blueprint/placed volume in the map.
