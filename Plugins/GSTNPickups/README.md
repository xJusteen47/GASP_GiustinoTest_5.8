# GSTNPickups

Equipment lying in the world (M11 slice L2, card gate G2; spec [combat-loadout-mods.md](../../docs/architecture/combat-loadout-mods.md) §4, §6, §12).
Optional, `EnabledByDefault: false`; the project enables it in `GASP_Giustino2.uproject`, and GSTNHeld lists it.

| Module | Type | What it holds |
|---|---|---|
| `GSTNPickups` | Runtime | `FGSTN_PickupPayload` (base) with `FGSTN_KitPickupPayload` and `FGSTN_SupportWeaponPickupPayload`; `AGSTN_EquipmentPickup` (replicated payload, placeholder look, `Take`); `AGSTN_KitPickup` (its kit child, moved from GSTNCombatGAS); `UGSTN_KitSwapSubsystem` (the TH-1b trigger, `FindPickupFor`, `SwapWithPickup`, `DropKit`, moved from GSTNCombatGAS); `FGSTN_PickupClaim` (the race rule). Tests `GSTNPickups.*`. |

Dependencies: GSTNCore, GSTNInput (core), GSTNCombat (core), GSTNCombatGAS, GameplayAbilities. Never Network Prediction, Mover, GSTNHeld or
GSTNLink: the plugin that brings the trigger with the framed focus (GSTNHeld's kit interact bridge) depends on this one.

## How it works

1. **Payload.** A pickup holds one `FInstancedStruct` whose base is `FGSTN_PickupPayload`. `MakeChange(Current, OutNew, OutLeftBehind)` is pure:
   it builds the loadout the taker would play and what stays on the ground. Another plugin adds a kind by deriving a struct (the backpack plugin
   of G6 will add `FGSTN_BackpackPickupPayload`); this plugin never names it.
2. **Kit** (`FGSTN_KitPickupPayload { Loadout }`): the taker plays the loadout on the ground; their previous loadout stays there. What
   carries over follows the mode's "Kit swap" settings in `UGSTN_LoadoutModeRules` (`MakeChangeUnder` + `FGSTN_KitSwapCarryOver`): the
   support weapon goes with the player by default (`CarrySupportWeapon`; Take falls back to leaving it with the old kit when the new kit's
   rules refuse it), mods follow the ground kit by default, a backpack always stays with the player; meters are `AcceptKitSwap`'s.
3. **Support weapon** (`FGSTN_SupportWeaponPickupPayload { Weapon }`): one at a time. With no additional weapon it is added and the pickup
   empties; otherwise the last additional weapon is swapped out and stays on the ground, and the player's slot customisations of that weapon are
   dropped. The kit's stats stay. The kit decides if it may carry it (`AllowedAdditionalWeapons`, `MaxAdditionalWeapons`).
4. **Take** (server): payload `MakeChange`, then `UGSTN_LoadoutComponent::AcceptLoadout` (validation and rules; a refusal changes nothing on
   either side), then the leftover goes into the pickup. The change reaches the simulation as a new `LoadoutRevision`, swapped at one frame
   boundary on every machine; nothing here is simulation state, nothing is predicted.
5. **Race** (`FGSTN_PickupClaim::SortForRace`): several requests for one pickup are served earliest request frame first, then lower player id
   (`APlayerState::GetPlayerId`, none last), never in arrival order; the loser is refused, it never takes what the winner left. GSTNHeld's
   bridge sorts its queued swaps with it.
6. **Triggers.** With GSTNHeld on, every equipment pickup gets the interact record of K1 (`Interact.Target.Kit`, hold B 0.5 s, the framed one
   wins). Without it, the TH-1b trigger of `UGSTN_KitSwapSubsystem` swaps the nearest pickup on the hold of interact.

## Designers

Make a Blueprint child of `AGSTN_EquipmentPickup` (or `AGSTN_KitPickup` for a kit), pick a mesh (no mesh = the coloured placeholder box) and set
the payload in the details panel. Nothing to wire: the interact record is added at runtime.

## Moved classes

`AGSTN_KitPickup` and `UGSTN_KitSwapSubsystem` were in `GSTNCombatGAS` until L2; `Config/DefaultGSTNPickups.ini` redirects
`/Script/GSTNCombatGAS.*` to `/Script/GSTNPickups.*`. The kit pickup's old `Loadout` property is now its kit payload (`GetLoadout`/`SetLoadout`
kept); no saved level held one.
