# Designer examples: four moves made only of data (laptop, UE 5.9, 2026-11-13)
Branch `claude/project-thread-vc4k9v` from `3d684fba` (`claude/project-thread-tixkmv`, the 5.9 line with the loadout screen).
Owner plan steps 3-4: an example setup a designer can open, copy and retune, plus an Italian guide (artifact
https://claude.ai/artifact/DyiBrsycoRfXNSMoZUBxun, "Mosse di esempio GSTN").
**Done**:
1. `Tools/GAS/setup_designer_examples.py` (headless, idempotent: creates only what is missing, `run(force=True)` rewrites) made 7 assets in
   `Content/GASNPPTest/Examples` (kits/weapons must live under /Game/GASNPPTest: the Asset Manager scan, DefaultGame.ini):
   `GA_Example_Light` (X, dup of GA_Medium_Jab), `GA_Example_Strong` (RT, dup of GA_Heavy_Jab), `GA_Example_Special` (Y, dup of
   GA_Medium_Special), frame data written with `GSTN_TestKitContentLibrary.write_strike_move`; `GA_Example_BackpackDome` (BP child of
   GSTN_GA_BackpackDome, default dome timeline); `WPN_Example` (copy of WPN_Test_Medium, light+strong on Neutral and Directional);
   `KIT_Example` (copy of KIT_Test_Medium, label ESEMPIO, special on Neutral+Directional); `DA_Example_Backpack_Dome` (copy of the dome backpack).
2. Tags `Kit.Example`, `Weapon.Example`, `Backpack.Example.Dome` in new `Config/Tags/GSTNExamples.ini`.
3. Hub, Python only (no map saved): kit pad `ESEMPIO DESIGNER` at (800,-1400) in `build_test_hub_map.KIT_PADS`, the example backpack at
   (800,1400) in `test_hub.BACKPACK_PADS`. Both also show in the loadout screen (kit "ESEMPIO", backpack "Example Backpack Dome").
4. Scenario `example_scenarios.designer_examples` (registers itself on gasnpp_test); `m3_suite_driver.py` gained `R31_IMPORT`;
   `batch_driver.py` gained `BATCH_MAP`.
**Gates (5.9)**: no C++ change, no build. Headless GSTN **1584/0**, 0 ensures. `designer_examples` (suite driver, DefaultLevel, host+2 clients)
**PASS 3/3**: damage +4 / +14 / +8 on the authority and the victim's client, 0 ensures (`Saved/R31/report-examples2.txt`).
**Open, flag**: the dome of the example backpack is NOT tried in game. The scenario's backpack step (Python `accept_loadout` on the running
authority pawn with a `GSTN_LoadoutBackpackExt`) re-made the pawn's table at revision 0 and crashed the editor twice (NP ensure
ClientID/ServerID, then a BitArray check in `UNpAbilitySystemComponent::EndPlay`), also with the fresh-PIE batch driver. Not investigated (2
attempts): could be the test's misuse of AcceptLoadout or a real 5.9 bug in a loadout change that adds a backpack. Next: pick the backpack in
the loadout screen with the pad in L_Test_Hub; if the editor crashes there, it is a real bug. The step stays in the scenario, `with_backpack=True`.
**Decisions (recommended)**: examples under GASNPPTest (no scan change); moves duplicated from the archetypes (the designer's own workflow); parent
stays `GSTN_GA_TestKitStrike` (a BaseCombat child with nothing extra; the guide never names it).
Cost: ~150k context, Opus, one Haiku subagent (first scenario run + headless tests); estimated ~$20-25. Wall clock ~1.3 h.
