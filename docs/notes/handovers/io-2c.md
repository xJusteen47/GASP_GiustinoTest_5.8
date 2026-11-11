# IO-2c: slot sets follow the button layout (laptop, 2026-11-11)
Branch `claude/laptop-io-2c` from `6c7947e0` (tip of `claude/laptop-io-2b`). Handover before: `io-2b.md`. Main laptop folder, editor closed.
**Done** (code `b7c44afa`, asset `52545325`; the branch tip is the commit of this file):
1. `FGSTN_InputSlotSpec::LayoutButtons` (default button -> layout function). Builders write the defaults and record the functions:
   standard (`Combat.*`), movement (`Movement.*`), link (`Link.*`, also throw/special-grab slots). `FGSTN_BindingTableCompiler::Compile`
   calls `ApplyActiveButtonLayout()` on its slot copies (new `FGSTN_InputCondition::RemapButtons`: Press, Release, Hold, Chord, Mash, AnyOf;
   one pass, a swap does not chain), then empties the list. Assets never bake a layout; defaults unchanged. Debug display applies it too.
2. Migration: `UGSTN_StandardSlots::AddLayoutFunctionsToSlotSet` (same command + same pattern as today's builder; edited slots left alone)
   and `Tools/GAS/migrate_slot_layout.py` (headless, saves .uasset only). Run: `SS_GASNPP_Standard` 20/24 slots migrated (4 harness press
   slots stay fixed), second run = nothing. `setup_gas_assets.py` refills give the same result. GSTNCombatCore now lists GSTNCore.
3. Test `GSTNCombat.Core.Table.ButtonLayout`: layout swapping Attack/Special, Crouch->Jump, Dash->Dodge moves the compiled slots, set asset
   untouched, no rules = defaults back, migration count and idempotence.
**Not covered (decide later)**: `UGSTN_CommandBindingSet` (M2 harness table, raw pointers into set assets) keeps defaults; Blueprint rule
conditions keep their buttons; kits are not migrated, the script only reports them: `KIT_Test_Delta` has `Command.Special.Ex` in its
added slots (kit-only slot, no builder template: fixed buttons).
**Gate (laptop)**: build ok after 2 fixes (C4459 shadowing in the test; name clash in StandardSlots). Headless GSTN **1514/0**, 0 ensures
(`Saved/IO2c/headless-t1.log`). Suite `R31_ALL=1`: **50/50 PASS**, 0 ensures (`Saved/R31/report-io2c-all.txt`). Note: `Saved/R31/driver_m3.py`
was a stale copy without R31_ALL (ran 29/29 first); now replaced by `Tools/GASNPPTest/m3_suite_driver.py`.
**Exit crash**: every editor run this thread (commandlet and suite) crashed at quit, access violation 0x0. Unlike IO-2b it HAS a project
frame: `UGSTN_HeldSettings::RemoveFromInputSettings()` (`GSTN_HeldSettings.cpp:19`) <- `FGSTNHeldRuntimeModule::ShutdownModule()`
(`GSTN_HeldRuntimeModule.cpp:129`). Crashes `Saved/Crashes/UECC-Windows-C7F40DC1...`, `...A9AE198B...`, `...DA67674D...`. Not touched.
**Unseen**: nobody pressed buttons in PIE with a GameRules asset that moves a combat function; the suite runs default layout only.
No manual step for the owner. Artifact merge tip not moved (desktop verifies first).
Cost: ~110k context, Opus, estimated ~$5-7. Wall clock ~1h (3 builds, 2 suite runs ~10 + 15 min).
