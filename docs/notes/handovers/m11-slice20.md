# M11 slice 20: pre-mission loadout screen (laptop, 2026-11-13)
Branch `claude/project-thread-ulc64s` from `fbf98070` (L7d tip, `claude/project-thread-wmlass`). Plan had no slice for it: built as below.
Owner design: mods/specials/backpack change BEFORE the match; in the match only kit pickups (`bAllowSpecialSwapInMatch` stays off).
Owner steer 16:20Z: playtest + designer interface first, "something judgeable": minimal but usable with the pad, everything from data.
**Done**:
1. GSTNCombatGAS: `FGSTN_LoadoutChoiceRegistry` (`Loadout/GSTN_LoadoutChoices.h`): screen rows from data. Built-in kit row (every
   `GSTN_PrimaryKit`, label = kit `DisplayName` else asset name) and one row per special a weapon allows alternatives for. Plugins add rows:
   GSTNKitModsGAS `FGSTN_KitModChoices` (one row per kit mod slot, mods that fit), GSTNBackpackGAS `FGSTN_BackpackChoices` (none + every
   backpack). `UGSTN_LoadoutComponent::GatherChoices` validates every option (`ValidateLoadout`); `FGSTN_LoadoutDraft` = cursor + cycle that
   skips refused options. Mods/backpacks listed via the asset registry (their types are not in the Asset Manager scan).
2. GSTNInputCore: `IGSTN_InputConfigSource` (combat sim component implements it) so UI finds the player's input config without NP.
3. New plugin `GSTNLoadoutMenu` (enabled in the .uproject; README): per-local-player subsystem + Slate panel (toy cards, no Blueprint).
   Stick = Move action, A/B = layout functions `Menu.Confirm`/`Menu.Back` (DefaultGSTNInput.ini, Jump/Interact). While open a transient IMC
   copies all the player's keys at priority 100000 and consumes them (character still). Confirm = `ServerRequestLoadout` + save preset `Last`.
   Opens by itself once per level only in `AutoOpenLevels` (L_Test_Hub, `DefaultGSTNLoadoutMenu.ini`, GameRules piece "Loadout screen");
   console `GSTN.LoadoutMenu` anywhere; `-GSTNNoLoadoutMenu`/automation keep it shut. Server refusal reopens it with the finding.
**Gate**: build ok first try. Headless GSTN **1581/0**, 0 ensures (+1 `GSTNCombat.GAS.Loadout.Screen`). Hub standalone smoke: opens by
itself, `Kit(24) Backpack(3)` (`Saved/M11Loadout/hub-smoke2.log`). Suite `R31_ALL=1` **49/50**, 0 ensures: `hit_on_wall_hit` red (wall-grab count, the timing flake of slice19), **PASS on its rerun**
(`Saved/R31/report-m11load-all.txt`, `report-m11load-wall.txt`). Second time in 2 days: consider adding it to the flaky list.
**Decisions (recommended, flag)**: Slate in C++ (no CommonUI in the project); kit change keeps look+extensions, clears support weapon,
supers, specials; mod config keyed by the kit's first `KitTags` tag; auto-open only in listed levels (harness safe); B leaves without sending.
**Not built**: nobody pressed the pad on it (no PIE with a pad here); mod row not seen live (KIT_Test_Alpha has no slots); "all ready"
match start (M12 lobby); support-weapon/super rows; mod-unlocked alternatives; mod/backpack display names (asset names, prefix stripped).
**Manual**: artifact "Cose da fare a mano": new entry "Prova la schermata del carico" (how to try it with the pad).
Cost: ~170k context main thread, no subagents; estimated ~$15-20. Wall clock ~1.5 h.
