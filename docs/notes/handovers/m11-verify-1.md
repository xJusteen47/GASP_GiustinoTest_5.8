# M11 verify 1: laptop slices L1-L8 on the desktop (2026-11-12)
Branch `claude/desktop-m11-verify` from `claude/laptop-m11-l8` (`095da6f3`), own worktree `GASP_Giustino2_m11v` (sample content copied from the main folder).
**Gate**: Editor, Game Development and Shipping built first try (960 s / 371 s / 222 s, logs `Saved/M11Verify/build-*.log`). Headless GSTN **1554/0**, 0 ensures (same as laptop).
**setup_kit_visuals.py** (headless): 30 kits/weapons given the placeholder look, `SM_Implant_Placeholder` created. Its report argument breaks on a path with a space (the commandlet splits it): pass none, or a path without spaces.
**New content** (`Tools/GAS/setup_kit_mod_test.py`, data only, ini tags in `GSTNCombatTest.ini`): `KIT_Test_Modded` (KIT_Test_Hits + one `Mod.Slot.Blade`),
`MOD_Test_LongBlade` (all moves x1.5 time, x1.3 reach and hitbox), `BPK_Test_JetPack` (box on the connector, no ability: an ability needs a Blueprint).
**Seen in PIE** (L_Test_Hub, standalone, driver `Tools/GASNPPTest/m11_verify_pie.py`, images `docs/notes/reviews/m11-verify/`):
1. Kit pickup with the modded loadout shows "Tieni B: KIT_Test_Modded". Holding the Interact logical button 1 s swaps: implant turns from yellow to blue,
   applied loadout = KIT_Test_Modded + mods extension, the old KIT_Test_Hits stays on the ground with its label.
2. Backpack pickup shows "Tieni B: BPK_Test_JetPack"; the hold puts `GSTN_LoadoutBackpackExt` in the applied loadout (kit and mod kept).
   **Not seen**: the backpack box on the pawn. From the default camera the placeholder implant (big box, covers the head) hides the back; not checked from the side.
3. Attack after the swap: no visible motion in 3 frames (the test hit move has no animation), so the x1.5 time was not judged by eye.
4. Preview on kit: `GSTN.KitMods`/`OpenPreview` opens the derived timeline `GSTNModTL_..._W0` in the M5 editor as its own window (phases Startup 6,
   Active 6, 3, Active 6, 3, Active 12, Recovery 6); viewport says "No preview subject" until a kit/skin is picked. Opened through the utility, not the right-click menu.
5. Closing the editor after PIE asks to save `GSTN_CombatRules_Standard` and `IC_GASNPP_Test` (dirtied by PIE, not by the preview). Answered Don't Save; the driver now clears them.
**Validation**: MOD valid; KIT_Test_Modded valid (warning from KIT_Test_StrikeKinds: invalid tag on `KitTag`); BPK and the L6 weapons fail in the
commandlet: `AssetReferenceRestrictions` reports the `/Engine/BasicShapes` soft refs as missing packages. Pre-existing on the L6 assets; not checked in the GUI.
**Python notes**: `unreal.SubsystemBlueprintLibrary` does not exist in this build (the artifact's IO-1 snippet will fail); world subsystems via `unreal.ObjectIterator`.
`UGSTN_KitSwapSubsystem::TrySwap` is not reachable (Get() is not a UFUNCTION); the swap was done by input, which is the real path anyway.
**Decisions taken**: new test kit instead of adding a slot to KIT_Test_Hits (base kits used by the suite stay as they were); headless commandlets run while
the jab thread's editor was open (no PIE then), PIE only after it closed. **Not done**: suite R31 (not asked); listen+2 PIE; backpack seen on the pawn.
**Owner**: push command in the artifact (one fast-forward, replaces b00a916e). No new decisions.
Cost: ~170k context, Opus, estimated ~$8-11. Wall clock ~1 h 30 (Editor build 16 min).
