# M11 slice 14: kit swap rules, owner answers M11.2/3/4/6 (laptop, 2026-11-12)
Branch `claude/laptop-m11-swap` from `095da6f3` (L8). Code `0bff9227`. Spec `combat-loadout-mods.md` §4 (*Built (M11 swap)* note).
**Done** (all settings in `UGSTN_LoadoutModeRules`, GameRules piece "Combat loadout", category **Kit swap**; per mode, no Project Settings):
1. `KitSwapSupportWeapon` = **StaysWithPlayer** (default, M11.2) / StaysWithKit. The player's last additional weapon and its slot customisations
   go to the new kit (`FGSTN_KitPickupPayload::CarrySupportWeapon`); a support weapon on the ground kit trades places with it. If the new kit's
   loadout rules refuse it, `AGSTN_EquipmentPickup::Take` falls back: the weapon stays on the ground with the old kit, the swap still happens.
2. `KitSwapMeters` = **Keep** (default, value capped at the new max) / Reset (M11.3, owner: playtest decides). `UGSTN_LoadoutComponent::AcceptKitSwap`
   sets the Ex/Super base values after the grant, out of band like `ApplyStartingHealth`. Keep chosen: trying a kit from the ground costs nothing.
3. `KitSwapMods` = **GroundKit** (default, M11.4) / PlayerSetup. Core `FGSTN_KitSwapCarryOver` + virtual `StaysWithPlayerOnKitSwapUnder`;
   the mods entry answers with the rule. The ground kit's slot customisations (specials) come with it too.
4. M11.6: `SupportWeaponStats` default was already **WeaponOwn** (owner: a heavy weapon may slow you). Only its comment changed.
**Gate (laptop)**: build ok. Headless GSTN **1559/0**, 0 ensures (+5: `GSTNPickups.Payload.KitSupportCarry`, `.Payload.KitModsRule`,
`.Take.KitSwapRules`, `GSTNKitMods.Core.KitSwapRule`, `GSTNCombat.GAS.Loadout.KitSwapMeters`; `Payload.Kit` updated to the new default;
`Saved/M11Swap/headless-t2.log`). 1 test fix (fallback skipped when the old and new kit carried the same weapon list).
Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11swap-all.txt`).
**Nobody has seen in PIE**: a kit swap with a support weapon or meters in a match; no harness scenario covers either (kit_swap_hold has no
support weapon). Health progress and broken state untouched by a swap, as before. Game/Shipping not built.
**Decisions (recommended option)**: "support weapon" = the last additional weapon (as the support pickup); the fallback uses the same
`ValidateLoadout` as the swap, never a separate kit check; Reset on a swap to an identical kit still empties the meters.
**Owner questions**: none new. Still open: M11.10, M11.11, special swap input, L9 design, jet-pack feel.
**Manual steps for the owner**: none (artifact unchanged). To try Reset while playtesting: GameRules asset > Combat loadout > Kit swap > Kit Swap Meters.
**Next**: L9 (when designed) or L10 sweep; L10 could add a hub support-weapon pad and a `kit_swap_support` scenario.
Cost: ~95k context, Opus, estimated ~$6-8. Wall clock ~1 h (2 builds, 2 headless runs, suite).
