# Combat test content: final names (for the harness update)

Written 2026-10-22. C++ not built yet, setup script not run.

## Abilities (project module, `Source/GASP_Giustino2/{Public,Private}/GASNPPTest/GSTN_TestCombatAbilities.*`)
Both derive from `UGSTN_GameplayAbility`, LocalPredicted, instanced per actor. In Python: `unreal.GSTN_GA_TestAttack`, `unreal.GSTN_GA_TestStance`.

| Class | Bound to | Owns while active | Ends |
|---|---|---|---|
| `GSTN_GA_TestAttack` | `Command.Attack.{Neutral,Directional,CrouchNeutral,CrouchDirectional}` x `Input.Context.{Grounded,Airborne}` | `Test.State.Attacking` | after `ActiveFrames` (default 20) simulation frames |
| `GSTN_GA_TestStance` | `Command.Special.{Neutral,Directional,CrouchNeutral,CrouchDirectional}` x `Input.Context.{Grounded,Airborne}` | `Test.State.Stance` + `Input.Lock.Command.Attack` | when the trigger button (read from the activation context) is no longer held (`IsCommandButtonHeld`) |

No separate test Special attack: the Special button is the stance button. Existing: `GSTN_GA_TestDash` (`Command.Dash`), `GSTN_GA_TestBuff` (`Command.Buff`).
Test.* tags are private to the .cpp (not exported); the harness reads them by name (`UGSTN_GASNPPTestComponent` already logs every `Test.*` tag).

## Tags
- Gameplay tags (native): `Test.State.Attacking`, `Test.State.Stance`; lock tag `Input.Lock.Command.Attack` (GSTNCombatLockTags::Attack, combat plugin).
- Button tags (`Config/Tags/GSTNCommands.ini` for Dash/Buff; standard ones native): `Input.Button.Attack`, `Input.Button.Special`, `Input.Button.Crouch`, `Input.Button.Dash`, `Input.Button.Buff`.
- Command tags: `Command.Attack.*`, `Command.Special.*` (native, 4 shapes each), `Command.Dash`, `Command.Buff` (ini).

## Button indices in IC_GASNPP_Test (bit order = spec order)
0 Attack, 1 Special (= stance), 2 Crouch, 3 Dash, 4 Buff. Matches the defaults of `scenario_combat_lock(attack_button=0, stance_button=1, dash_button=3)`.

## Assets
`/Game/GASNPPTest/Input/`: `IA_GASNPP_Attack` (LMB), `IA_GASNPP_Special` (MMB), `IC_GASNPP_Test`, `SS_GASNPP_Standard` (16 slots via `GSTN_StandardSlots.fill_slot_set`), `BS_GASNPP_Test`.
`setup_gas_assets.run(combat_input=True)` also adds the four test abilities to the NpAbilitySystemComponent DefaultAbilities of `BP_GASNPP_TestCharacter` (a binding only starts a granted ability).

## Dash/Buff slots
`UGSTN_TestSlotUtils::AddButtonPressSlot(Set, Command, Button)` (project, `GASNPPTest/GSTN_TestSlotUtils.*`; Python `unreal.GSTN_TestSlotUtils.add_button_press_slot`) adds a one-step plain press slot, skipped if the set already has a slot for that command. The setup script applies `press_slots`: `Command.Dash` <- `Input.Button.Dash`, `Command.Buff` <- `Input.Button.Buff`, so `SS_GASNPP_Standard` has 18 slots (16 standard + 2).

## Open
- `FillSlotSet` signature changed from `UGSTN_InputSlotSet&` to `UGSTN_InputSlotSet*` (UFUNCTION); the one test caller was adjusted. Any test counting slots on a set that also got the 2 press slots would see 18, not 16 (only the transient set in the automation test is used, unaffected).
