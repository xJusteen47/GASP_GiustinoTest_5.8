# M11 slice L7: backpacks (laptop, 2026-11-12)
Branch `claude/laptop-m11-l7` from `ff4eac17` (L5b). Plan `m11-kit-piano-a-fette.md` L7, spec `combat-loadout-mods.md` §10 (*Built (M11 L7)*).
**Done**:
1. New plugin `GSTNBackpack` (Core, GAS; off by default, on in the .uproject). Core: tags `Backpack` root and `Command.Backpack.Use`
   (native command only, the button is data), loadout extension `FGSTN_LoadoutBackpackExt` (wire id `Backpack`, soft path, `bKeepOnKitSwap`),
   pure `FGSTN_BackpackLoadoutRules` (equip one at a time, hands back the replaced one, remove). GAS: `UGSTN_BackpackDefinition` (identity tag,
   Visual, Abilities bindings, Passives, Effects, CompatibleSupportWeapons, bDropOnDeath), extension `GSTN.Backpack` (priority 200, after mods:
   bindings and passives join the kit's, effects on grant), attachment provider (a part without socket/parent goes on Implant.Back/Connector_Backpack).
2. Bridge plugin `GSTNBackpackPickups`: `FGSTN_BackpackPickupPayload` (swap leaves the old backpack on the ground). Neither plugin names the other.
3. GSTNCombatCore: `FGSTN_LoadoutExtensionData::StaysWithPlayerOnKitSwap` + `GSTNLoadoutExtension::CarryOverOnKitSwap`; the kit pickup payload
   uses it, so a kit swap keeps the backpack (spec §10) and still swaps everything else (mods unchanged: travel with the kit).
4. **Bug fixed** (latent since L1): the extension payload used a plain memory archive, which is fatal on any `FSoftObjectPath`, so sending or
   saving a loadout with mods (or a backpack) crashed. Now a string proxy archive (names and soft paths as strings, same bytes for FNames).
5. Build fix: unity clash `ResolveClass` in `GSTN_CombatNPTests_ModTimeline.cpp` renamed.
**Gate (laptop)**: build ok; headless GSTN **1549/0**, 0 ensures (+7: `GSTNBackpack.Core.{Equip,WireRoundTrip}`, `GSTNBackpack.GAS.{Compile,
VisualOnConnector,RulesView}`, `GSTNBackpack.Pickups.Payload`, `GSTNPickups.Payload.KitCarryOver`; `Saved/M11L7/headless-t5.log`).
Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l7-all.txt`). 2 build fixes (include, unity clash), 1 test fix (the crash above).
**Nobody has seen in PIE**: no backpack content exists; no backpack ability ran in a match; no harness scenario (`scenario_backpack_use` is L10).
Desktop: build, author `BP_Backpack_JetPack` (identity `Backpack.JetPack` ini leaf, one binding on `Command.Backpack.Use` with a slot on a
GameRules logical button, a placeholder burst ability via Mover queued move, a cube on the connector), put it on a hub pickup, PIE listen+2.
Without an implant the backpack goes on the pawn mesh at `Connector_Backpack`: check the skin's SocketFallbacks maps it to the back.
**Decisions taken (recommended)**: backpack verbs as kit-source entries (no new source kind, as L1); priority 200 so mods never reshape them;
kit swap keeps the backpack as data (`bKeepOnKitSwap` default true); feeding without the weapon = warning only; payload in a bridge plugin.
**Owner questions**: M11.11 (which button for `Command.Backpack.Use`, and dome-shield backpacks in PvE); jet-pack feel (burst height, air
uses, fuel/cooldown) is move design: needed before the sample ability. Still open: M11.2-M11.4, M11.6, special swap input, L9 design.
**Not built**: death drop (`bDropOnDeath` data only, like the kit's `DropKit`), feeding a support weapon, sample jet-pack ability.
**Manual steps for the owner**: none (artifact unchanged). **Next**: L8 (mod editor), L10 sweep (backpack pad + `scenario_backpack_use`).
Cost: ~110k context, Opus, estimated ~$7-10. Wall clock ~1 h (3 builds, 2 headless runs, suite).
