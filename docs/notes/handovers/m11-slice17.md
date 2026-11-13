# M11 slice 17: L10 first mod, first aimed move, kit swap scenarios (laptop, 2026-11-13)
Branch `claude/laptop-m11-l10` from `732a64c0` (L7b; the L7b handover commit `a8b9c08e` sits under it), pushed as `claude/project-thread-60cg1f`.
Owner rule (2026-11-12): mods and specials change before the mission; in the mission only a kit swap (the ground kit's mods come with it).
**Done** (no Blueprint wiring, no Project Settings, sim untouched):
1. `UGSTN_KitModContentLibrary` (GSTNKitModsGAS): `SetKitModSlots`, `WriteAllMovesMod`, `WithKitMods` / `GetAnyKitMods` (mod paths as strings,
   a wildcard config on a loadout or a pickup). Scripts never build an instanced struct.
2. Starter aimed move `UGSTN_GA_AimedHitscanShot` (GSTNProjectileNP): Hitscan, aim Reticle (Facing with it off), 5 frames, 6 damage, 5000 cm.
3. `Tools/GAS/setup_m11_ranger_content.py` (run headless, assets committed): `KIT_Test_Ranger` = copy of KIT_Test_Medium, display TIRATORE,
   one `Mod.Slot.Grip`, the shot on Strong.Neutral/Directional with `Input.Context.PrecisionAim` required. Medium has no plain strong move, so
   strong without the reticle does nothing. `Mods/MOD_Test_Power`: every move 1.4x hitbox, 1.5x reach, 1.25x damage, 1.15x kb, 1.2x slower.
   Tags in `Config/Tags/GSTNCombatTest.ini`.
4. Hub: `build_test_hub_map.KIT_PADS` + TIRATORE at (800,-700); new `MOD_PADS` TIRATORE + MOD POTENZA at (800,700); `test_hub.spawn_kits`
   lays both at Play (`_spawn_modded_kit`). The map itself is not rebuilt (pads and signs missing until the owner reruns the map script).
5. Harness (`kit_scenarios`, SUITE_KIT): `kit_swap_support` (support weapon stays with the player, ground kit without it, Ex meter kept),
   `kit_mod_swap` (mods reach authority and owner tables, same crc), `aimed_shot` (reticle off: nothing; on: victim damaged on both copies).
   Test snapshot gives `loadout.derivedTimelines`.
**Gate (laptop)**: build ok x2. Headless GSTN **1570/0**, 0 ensures (+1 `GSTNKitMods.GAS.ContentLibrary`; `Saved/M11L10/headless-b2.log`).
Kit batch listen: kit_swap_support PASS, kit_swap_hold PASS, kit_mod_swap PASS (fix 1: Python SoftObjectPath built empty, paths now strings),
aimed_shot PASS (`Saved/M11L10/report-kit1/3, aim1.txt`). Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l10-all.txt`).
**Not run**: the new scenarios on a dedicated server or with loss; Game/Shipping not built.
**Known gaps**: the shot has no recovery (press rate = fire rate); the reticle is held on by the harness through the hold flag, the pad
player toggles it with the right stick click; mods do not touch the shot (no timeline). Backpack pad and `scenario_backpack_use` still open.
**Decisions (recommended, flag)**: aimed shot on a copy of the medium kit rather than a support weapon (kit bindings are always active);
mod changes whole-move scales via HitboxScale too, since the test strikes have no ReachAxis.
**Manual steps**: artifact item `ed-m11-l10-ranger` (try the two TIRATORE pads in PIE; rerun the hub map script for pads/signs if wanted).
Cost: ~170k context, Opus, estimated ~$12-15. Wall clock ~1.5 h (2 builds, 2 headless runs, 4 PIE batches, suite).
