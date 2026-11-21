# M10 + M11 code leftovers (cloud, no editor, 2026-11-21)
Branch `claude/project-thread-476gv2` from `claude/runstart-generic-desktop` 84cc0057. Source: `milestones-stato-2026-11-21.md` (claude/project-thread-yepa62).
**NOT COMPILED, NOT RUN** (cloud has no Unreal). Every number below is a design value, none measured.

**Done (code)**
1. M10 G12 (`Plugins/GSTNHeld/README.md` step 20): held object on `hand_r` of the drawn body mesh (`gstn.Held.HandAttach` 1, 0 = old root offset),
   pickup/release ease `gstn.Held.PlacementBlend` 0.1 s, throw arc preview for the local holder `gstn.Held.ArcPreview` 1 (debug lines, facing aim,
   not Shipping; replaces the planned editor module), prompts `gstn.Held.Prompt` 2 (pickup "B: prendi" + "RT: lancia" in hand, texts in held settings),
   wind-up from the debug window `gstn.Held.WindupFrames` (-1 = data; PIE only). Test `GSTNHeld.Core.PresentationMath`.
2. M11 loadout screen: rows "Arma di supporto" (priority 50) and "Super" (60) with "Nessuna", `DisplayName` labels (new field on mods and backpacks).
   Test `GSTNCombat.GAS.Loadout.ScreenSupportAndSuper`.
3. M11 backpack: `gstn.Equipment.AutoLook` (draws worn backpacks, default on), `gstn.Equipment.HideKitBoxWhenWorn` (kit box hidden under a backpack).
4. All new cvars + `gstn.Combat.KitPlaceholderLook` listed in GSTN Debug Options (`SGSTN_DebugOptionsPanel.cpp`, prefixes `gstn.Held.`, `gstn.Equipment.`).
5. Flaky tests: `run_suite` reruns `jump_squat` / `hit_on_wall_hit` once in place when red (`SUITE_FLAKY`), report says "PASS ... on rerun (first run ...)";
   `jump_squat` jumps 2.4 s after the kit request (was 1.4 s).

**Not done**: HoldFrames validator (no spec found), G12 video, real backpack meshes (asset), KIT_Test implant offsets (asset, see `backpack-look.md`),
"tutti pronti" start (M12).

**Desktop/laptop steps (one build for this + the M12 thread)**
1. Pull this branch into the editor worktree; real incremental build (build script, 45 min max). Fix compile errors here first (Live Coding is not enough).
2. Headless `Automation RunTests GSTN` (30 min max): expect previous 1584 + 2 new tests, 0 fails.
3. Harness `R31_ALL=1` suite; check `jump_squat`/`hit_on_wall_hit` lines; then `SUITE_HELD` (held_scenarios) listen + dedicated.
4. PIE L_Test_Hub: `gstn.Held.Spawn`, pick up: object in the right hand? If it sits wrong, tune `HandOffset` on the test definition (not code).
5. Throw: yellow arc before, smooth exit from the hand. `gstn.Held.WindupFrames 12` in GSTN Debug Options, throw again (first PIE look at the wind-up).
6. Loadout pad: support weapon and super rows appear only for kits with `MaxAdditionalWeapons`/`SuperSlotCount` > 0 and matching assets.
7. Pick a backpack: drawn on the back, kit box hidden; toggle both `gstn.Equipment.*` in the window.
8. Capture video (behind + side views) of 4-5 for the G12 review; only then to the owner.
9. Asset (owner/designer): real backpack meshes in each backpack's `Visual.Parts`; `DisplayName` on kits/weapons/supers/mods/backpacks.

COSTO: ~6-7 $ (Opus, ~120k context, no subagent) | 0 min desktop | 0 token review
