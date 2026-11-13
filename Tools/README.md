# Tools

Editor Python for the test content and the PIE harness. Nothing here runs in a shipped game. Run the editor scripts from the editor's Python console, through
`mcp__rider__ue_execute_python`, or headless with `UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<script> <args>"`.

## Tools/GAS

| Script | What it does | Command line (editor Python) |
|---|---|---|
| `setup_gas_assets.py` | Idempotent test content: mapping contexts, kits, hit content, extension Blueprints, physical materials. Backs Blueprints up to `Saved/Backups/GAS` first | `sys.argv = ['x', '--combat-input', '--hits-content', '--h13-content']; runpy.run_path('Tools/GAS/setup_gas_assets.py', run_name='__main__')` |
| `build_test_combat_map.py` | Builds `L_Test_Combat` (wall, glass, KO zone, trampolines) | `import build_test_combat_map as b; b.run()` |
| `validate_combat_content.py` | Runs every validator of the combat content and prints the matrices | headless `-run=pythonscript` |
| `bake_sockets.py` | Samples a montage at the NPP tick rate into `BakedSockets` of a timeline asset | `import bake_sockets as b; b.bake_into_timeline('/Game/GASNPPTest/Combat/TL_Test_HitBaked', '/Game/Characters/UEFN_Mannequin/Animations/Traversal/Hurdle/AM_M_Relaxed_Traversal_Hurdle_1_0_run_F_Lfoot', ['hand_r'])` |
| `make_hurtbox_set.py` | Physics Asset bodies to a `UGSTN_HurtboxSetDefinition` (needs `UGSTN_PhysicsAssetUtils`, Python cannot read the body setups) | `import make_hurtbox_set as m; m.from_physics_asset('/Game/Characters/UE5_Mannequins/Rigs/PA_Mannequin', '/Game/GASNPPTest/Combat/HBS_FromPhysics', tag_map=m.STANDARD_TAG_MAP, default_tag='Combat.Hurtbox.Torso')` |

Both tools were run on real sample content in H13b: the bake of the UEFN hurdle montage gave 32 frames of `hand_r` (the montage carries root motion, so the
hand travels about 2 m: pick a montage without it for a real attack), the Physics Asset gave 22 hurtboxes (placed at their bone-space offsets, which the
tool warns about: pass `bone_transforms` or fix the offsets by hand).

## Tools/GASNPPTest

`gasnpp_test.py`, the PIE harness (dedicated server + 2 clients, or listen server + 2 clients). See `docs/architecture/testing.md`.
