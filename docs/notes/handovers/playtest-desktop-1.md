# First owner playtest on the desktop, L_Test_Hub (desktop, UE 5.9, 2026-11-13)
Branch `claude/project-thread-keexim` from `fde5585f` (worn backpacks). Folder `GASP_Giustino2_ue59` is now on this branch and built.
**Root cause of most reports**: the desktop folder was on `61086557` (before loadout screen, kit box on spine_05, backpacks). The
owner played an old build. Moved to fde5585f + fixes, built. **The owner always playtests on the desktop**: keep `_ue59` on the tip, built.
**Checked in PIE through UE 5.9's built-in MCP** (Epic ModelContextProtocol, `http://127.0.0.1:8000/mcp`, `.mcp.json` entry `unreal-mcp`;
toolsets `PythonScriptingTools.execute_python`, `InputToolset.ApplyInput` for real keys, `SceneCaptureToolset.CaptureGameViewport`).
The old ue-mcp plugin (`Plugins/UE_MCP_Bridge` 1.4.3, latest 1.4.13) is not used; owner: leave it. Probes: `Saved/Playtest/pt.py`.
1. **Kits light/medium/heavy, jump, attack, swap**: work on the new build (host, listen 3 players): injected and real keys (Y jump,
   LMB attack, O held = swap). Light jump 235 cm, swap Hits -> Light -> Medium accepted (revisions 2, 3).
2. **Hurtboxes "out of sync"**: the drawn hurtboxes are exactly the sim pose (snapshot = actor, 0 cm, same yaw on host). The body
   leaves them because hurtboxes are a static shape set: pelvis up to 31 cm off while running (GASP run lean), more for head and limbs.
   OffsetRootBone is not it (root gap <= 6 cm, translation radius 0). Not a net desync. **Owner decision**: hurtboxes that follow
   locomotion (per-pose sets or timeline hurtbox modifiers) vs keep static (fighting-game style) and tune the shapes.
3. **Kit box beside the back**: old build; on fde5585f it rides spine_05 (follows the pelvis in the probe).
4. **Crash found while probing (fixed)**: `GSTN_InputHistory.cpp:61` check "Samples must be appended in frame order (24381 after 24433)".
   NP logged "Gap in input stream" then `ReconcileFromInputOffset 191 -> 138`: frame numbers moved 52 back without a restore. Now the
   history clears (keeps held buttons) with a warning. The combat snapshot still raises its handled ensure on that path (not fixed).
5. **IMC_GSTN_AutoAim leak (fixed)**: every resolve of the shared input config made a new context, 17-20 per player after a few swaps.
   `UGSTN_InputConfig::ResolveActions` now reuses the previous auto context and actions when the keys are the same.
6. **Editor target V7 -> V8** (5.9 asked "Target Upgrade Required" at every launch; editor targets keep Tests on V8).
**Seen, not fixed**: loadout screen "SCEGLI IL TUO KIT" opens at spawn in L_Test_Hub and stays on screen while playing (game keys still
work); clients' local players show no mouse mappings for Attack (only host checked in depth); log spam "Lag Compensation Rewind Exceeded"
(43k lines in 10 min, listen 3 players).
**Gates**: build ok (4 builds). Headless GSTN **1584/0**, 0 ensures (`Saved/Playtest/headless.log`). Harness suite not run (owner's
editor time). Fixes 4-5 are not re-checked in PIE yet (no new crash path forced, IMC count not recounted): owner's next Play is the check.
**Owner**: push `claude/project-thread-keexim` to feature (command in the thread). Designer guide legend: separate thread.
Cost: ~150k context, Opus, 3 small subagents (Sonnet x2 image reads, Haiku headless); estimated ~$20-25.
