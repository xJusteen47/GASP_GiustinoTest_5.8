# GSTN Loadout Menu

Pre-mission loadout screen (M11). Each local player picks kit, specials, mods and backpack with the pad before the match; on a listen
server every player does it on their own machine. Slate only, built in C++: no widget Blueprint, nothing to wire.

## How it works

- **Rows from data.** `UGSTN_LoadoutComponent::GatherChoices` (GSTNCombatGAS) asks `FGSTN_LoadoutChoiceRegistry` for the rows of the
  draft: the kit row (every `GSTN_PrimaryKit` the Asset Manager knows) and one row per special that the kit's weapons allow
  alternatives for are built in; GSTNKitModsGAS adds one row per mod slot of the kit (`FGSTN_KitModChoices`, mods that fit the kit),
  GSTNBackpackGAS a backpack row (`FGSTN_BackpackChoices`). A disabled plugin has no row. Every option carries the whole loadout it gives
  and is judged with `ValidateLoadout`; the ones the rules refuse are skipped.
- **The draft.** `FGSTN_LoadoutDraft` (GSTNCombatGAS, no widget): cursor up/down over rows, left/right picks the next accepted option.
  It starts from the applied loadout, else the player's last confirmed one (`UGSTN_LoadoutSubsystem` preset `Last`), else the first kit.
- **Sending.** Confirm calls `ServerRequestLoadout` on the PlayerState's loadout component: the server validates, it replicates, and it
  is granted on every pawn the player spawns (same path as before). A refusal reopens the screen with the finding.
- **Input, no key named in code.** Navigation is the player's own Move action (input config, through `IGSTN_InputConfigSource`);
  confirm and back are the button layout functions `Menu.Confirm` (A, `Input.Button.Jump`) and `Menu.Back` (B, `Input.Button.Interact`)
  in `Plugins/GSTNInput/Config/DefaultGSTNInput.ini`. While open, a transient mapping context copies every key the player has mapped
  onto the menu's actions at priority 100000 and consumes them: the character stays still, the simulation gets a neutral command.

## When it opens

`UGSTN_LoadoutMenuRules` (GameRules piece "Loadout screen", defaults in `Config/DefaultGSTNLoadoutMenu.ini`): by itself once per level
when the player's pawn first appears, only in `AutoOpenLevels` (the playtest hub `L_Test_Hub`) or everywhere with `bAutoOpenEverywhere`.
Mods and specials change before the match only (`bAllowSpecialSwapInMatch` stays off); during the match only kit pickups change the
kit. Console `GSTN.LoadoutMenu` opens or closes it anywhere (testing). `-GSTNNoLoadoutMenu` and automation keep it shut.

## Not built

Match start gated on "everyone ready" (M12 lobby flow), support weapon and super rows, the alternatives a mod unlocks (shown only when
the weapon allows them), mouse/keyboard navigation beyond the copied Move keys, a preview of the character.
