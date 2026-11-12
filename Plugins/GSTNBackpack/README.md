# GSTNBackpack

Backpacks (M11 slice L7, card gate G6, spec [combat-loadout-mods.md](../../docs/architecture/combat-loadout-mods.md) §10).
Optional plugin, `EnabledByDefault` false. Disabled = no backpacks; a saved loadout's backpack entry is dropped with a warning.

## Modules

| Module | What | Depends on |
|---|---|---|
| `GSTNBackpackCore` | Tags (`Backpack` root, `Command.Backpack.Use`), the loadout extension `FGSTN_LoadoutBackpackExt` (wire id `Backpack`), the pure rule `FGSTN_BackpackLoadoutRules` (equip, one at a time, remove) | `GSTNCombatCore` |
| `GSTNBackpackGAS` | The asset `UGSTN_BackpackDefinition`, the loadout extension `GSTN.Backpack` (priority 200, after the mods) and the attachment provider `GSTN.Backpack` | `GSTNCombatCore`, `GSTNCombatGAS` |

The pickup payload lives in the bridge plugin [GSTNBackpackPickups](../GSTNBackpackPickups) (`FGSTN_BackpackPickupPayload`), so this plugin
never names `GSTNPickups` and the reverse.

## Designer flow (no Blueprint wiring)

1. Make a `GSTN_BackpackDefinition` data asset: one identity tag under `Backpack` (ini leaf, e.g. `Backpack.JetPack`).
2. `Abilities`: the verbs as bindings, usually one on `Command.Backpack.Use` to a `UGSTN_GameplayAbility` child (a jet-pack burst moves
   the character through a Mover queued move, like any ability). The button is data: the binding's `SlotOverride` (a logical button of the
   GameRules layout) or the kit's command set. `Passives` and `Effects` while carried.
3. `Visual.Parts`: a part with neither `Socket` nor `ParentSlot` goes on the kit implant's `Connector_Backpack`, so any backpack fits any kit.
4. Give it to a player: the loadout's extension `Backpack` entry, or a pickup with a `Backpack` payload (bridge plugin enabled).

## Rules

- One backpack at a time; taking another drops the current one (the payload leaves it on the ground).
- A whole-kit swap keeps the backpack (`bKeepOnKitSwap`, default true, through the generic
  `FGSTN_LoadoutExtensionData::StaysWithPlayerOnKitSwap`); false sends it down with the old kit.
- The backpack's bindings join the kit's at compile: bound whatever weapon is active, never reshaped by mods.
- `CompatibleSupportWeapons` without such a weapon carried is a warning, never an error. `bDropOnDeath` is data only until a death drop
  hook is called.

Tests: `GSTNBackpack.*` (Core, GAS, Pickups).
