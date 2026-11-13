# Backpack on the back follows the body (laptop, UE 5.9, 2026-11-13)
Branch `claude/project-thread-tor00h` from `dc3a7602` (designer examples).
**What the owner saw**: the box on the back is NOT a backpack look. Backpacks (`DA_Backpack_JetPack`, `_ShieldDome`,
`DA_Example_Backpack_Dome`) have empty `Visual.Parts`, and no pawn carries a `UGSTN_SkinComponent` (no asset or code adds one), so
nothing of the equipment look is drawn in game. The box is `UGSTN_KitPlaceholderLookSubsystem`'s kit stand-in, attached to the capsule
(root), so it never followed the animation.
**Done** (C++ only, no asset, no map):
1. `GSTN_KitPlaceholderLook.cpp`: the box rides bone `spine_05` of the mesh that draws the body. The offset is computed from the reference
   pose so the box sits exactly where the approved capsule offset put it at rest (no bone-axis guess); it re-attaches if the drawn mesh
   changes (GASP's `VisualOverride` child actor). Other skeleton without the bone: stays on the root as before.
2. `FGSTN_EquipmentVisualUtils::FindShownBodyMesh` (CombatGAS): preferred mesh if drawn, else first drawn skeletal mesh of the pawn, else of
   its child actors.
3. `UGSTN_SkinComponent`: pieces go on that mesh (`bFollowVisibleMesh`, `VisibleMeshCheckInterval`, re-check timer); new socket fallback
   `Connector_Backpack -> spine_05` (a backpack with no back implant used to land at the mesh origin, the feet).
**Gates (5.9)**: editor build ok, headless GSTN **1584/0**, 0 ensures. **Not seen in PIE** (no capture taken): owner checks in L_Test_Hub.
**Not done, flag**: real backpack meshes on the back need (a) a look component on the pawn: decision, e.g. the loadout component adds
`UGSTN_SkinComponent` on every drawing machine (prototyped then reverted: it would also start applying skins to the GASP mesh), and
(b) a mesh + offset in each backpack's `Visual`. Owner/coordinator to decide.
**Crash of the previous thread (not fixed)**: `example_scenarios._server_loadout` takes the PAWN's `GSTN_Loadout` component first (the test
pawn has one, revision 0 = never used) and calls `AcceptLoadout` on it from Python; the loadout screen and pickups go through the
PlayerState's component / the server flow. Likely a second loadout component granting onto the same ASC. Next: swap the order to
(player_state, pawn) or use the pickup path as `m11_verify_pie.py` does; owner test via the loadout screen stays the real check.
Logs: `Saved/UE59Load/{build,headless}-backpack.*`, probes `Saved/Backpack/`.
Cost: ~130k context, Opus, no subagent; estimated ~$12-15. Wall clock ~1.5 h (3 headless editor probes + 2 builds).
