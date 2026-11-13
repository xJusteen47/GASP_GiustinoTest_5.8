# GSTNBackpack

Backpacks (M11 slice L7, card gate G6, spec [combat-loadout-mods.md](../../docs/architecture/combat-loadout-mods.md) §10).
Optional plugin, `EnabledByDefault` false. Disabled = no backpacks; a saved loadout's backpack entry is dropped with a warning.

## Modules

| Module | What | Depends on |
|---|---|---|
| `GSTNBackpackCore` | Tags (`Backpack` root, `Command.Backpack.Use`), the loadout extension `FGSTN_LoadoutBackpackExt` (wire id `Backpack`), the pure rule `FGSTN_BackpackLoadoutRules` (equip, one at a time, remove) | `GSTNCombatCore` |
| `GSTNBackpackGAS` | The asset `UGSTN_BackpackDefinition` (+ the starters `_JetPack`, `_ShieldDome`), the movement effect `UGSTN_GE_BackpackMovement`, the loadout extension `GSTN.Backpack` (priority 200, after the mods) and the attachment provider `GSTN.Backpack` | `GSTNCombatCore`, `GSTNCombatGAS` |
| `GSTNBackpackNP` | The combat tick extension that runs the backpack's memory and dome (feature id `Backpack` = 21), the dome move `UGSTN_GA_BackpackDome` | `GSTNCombatNP`, `GSTNBackpackCore` |

`GSTNBackpackCore` also holds the sim data (M11 L7b): the fragment `FGSTN_BackpackFragment`, the tracks `FGSTN_BackpackChargeTrack` and
`FGSTN_BackpackDomeTrack`, the pure step `FGSTN_BackpackStep`, the movement struct `FGSTN_BackpackMovement`.

The pickup payload lives in the bridge plugin [GSTNBackpackPickups](../GSTNBackpackPickups) (`FGSTN_BackpackPickupPayload`), so this plugin
never names `GSTNPickups` and the reverse.

## Designer flow (no Blueprint wiring)

1. Make a `GSTN_BackpackDefinition` data asset: one identity tag under `Backpack` (ini leaf, e.g. `Backpack.JetPack`).
2. `Abilities`: the verbs as bindings, usually one on `Command.Backpack.Use` to a `UGSTN_GameplayAbility` child (a jet-pack burst moves
   the character through a Mover queued move, like any ability). The button is data: the binding's `SlotOverride` (a logical button of the
   GameRules layout) or the kit's command set. `Passives` and `Effects` while carried.
3. `Visual.Parts`: a part with neither `Socket` nor `ParentSlot` goes on the kit implant's `Connector_Backpack`, so any backpack fits any kit.
4. Give it to a player: the loadout's extension `Backpack` entry, or a pickup with a `Backpack` payload (bridge plugin enabled).

## Move slots, memory, movement (M11 L7b, owner 2026-11-12)

- **No backpack button.** A backpack's move slots are its `Abilities` bindings, each with its own trigger (`SlotOverride`), e.g. the dome's
  `Press(dodge)` + stick neutral on `Command.Backpack.Dome` (one condition more than the dodge, so it wins only with the stick at rest; the
  button follows the layout's `Movement.Dodge`).
- **Memory**: 4 slots of charges per entity, combat sync state (`FGSTN_BackpackFragment`, rolled back and reconciled with the combat state).
  A backpack move spends through a `Backpack Charge` track on its timeline (slot, cost, capacity, refill `Never` / `OnLanding` / `OverTime`);
  its binding gates on `Backpack.State.Empty.Slot<N>` (IgnoreTags), projected each frame. An "EX charges" backpack is a `Never` slot.
- **Movement** (`FGSTN_BackpackMovement`): extra air jumps and a glide added to the carrier's combat stats while carried
  (`UGSTN_GE_BackpackMovement`). The glide itself is the jump state's (`FGSTN_JumpState::bGliding`): falling with the last jump's button held.
- **Dome**: a `Backpack Dome` track raises a dome (centre = the carrier at the end of the previous frame, radius, `WhileHeld` or `Deploy`),
  sync state and the tag `Backpack.State.Dome`. **It does not block hits yet** (next slice).
- **Starters** (make a data asset of the class, nothing to wire): `GSTN_BackpackDefinition_JetPack` (+1 air jump, glide 0.25 g, 300 cm/s
  cap) and `GSTN_BackpackDefinition_ShieldDome` (the dome move: 400 cm, held, at most 10 s, 2 charges, one back every 20 s). First-guess
  numbers for the playtest.

## Rules

- One backpack at a time; taking another drops the current one (the payload leaves it on the ground).
- A whole-kit swap keeps the backpack (`bKeepOnKitSwap`, default true, through the generic
  `FGSTN_LoadoutExtensionData::StaysWithPlayerOnKitSwap`); false sends it down with the old kit.
- The backpack's bindings join the kit's at compile: bound whatever weapon is active, never reshaped by mods.
- `CompatibleSupportWeapons` without such a weapon carried is a warning, never an error. `bDropOnDeath` is data only until a death drop
  hook is called.

Tests: `GSTNBackpack.*` (Core, GAS, NP, Pickups); the glide is `GSTNCombat.Core.Jump.Glide`.
