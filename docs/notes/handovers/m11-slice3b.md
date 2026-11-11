# M11 slice 3b: held and kit swap buttons as data (desktop, 2026-11-11)
Branch `claude/desktop-m11-3b` from `48fe6528`. Doc: `Plugins/GSTNHeld/README.md` step 19.
**Done**: the M10 held buttons and the K1 kit swap button are settings, not C++: `UGSTN_HeldSettings::PickupDropButton`, `ThrowButton`,
`KitSwap.Button` (+ `SlotOverrides` = whole slot replaced for any `Command.Held.*` or `Command.Kit.Swap`, K2 pattern). Read through
`FGSTN_HeldSlotInputs::FromSettings` by the loadout extension (every machine, same settings). `DefaultGSTNHeld.ini` names today's buttons, and an
empty value falls back to the same, so nothing changes for the player. Test `GSTNHeld.Core.SlotInputs` (defaults, ini, moved buttons, overrides).
**Choice (rule 6)**: simple button fields + full override array (the owner changes one tag; a designer can still change the pattern).
The logical tags are `Input.Button.*`, not `Combat.Button.*` as the K2 handover and `GSTN_LinkSettings.h` comments say (doc slip, not fixed there).

| Input | Where it is defined |
|---|---|
| Key / pad button -> logical `Input.Button.*` | Input Actions + mapping contexts (`Content/Input`), `AbilitySystemMappingContexts` |
| Held pickup / drop (B press) | GSTN Held > Input\|Buttons `PickupDropButton` |
| Held throw, enemy throw (RT press, tap/hold) | GSTN Held > Input\|Buttons `ThrowButton` (hold = the activating button) |
| Kit swap (B release after 0.5 s) | GSTN Held > Kits `KitSwap.Button`, `HoldSeconds` |
| Any held/swap slot shape | GSTN Held > Input\|Buttons `SlotOverrides` |
| Link rows (grab, pummel, mash) | slot set asset (`UGSTN_LinkSlots::Build`, setup script) |
| Kit throws | GSTN Link > Throws `ThrowInputMode`, `ThrowSlotOverrides` |

**Still fixed in code (other files, listed only)**: wall/surface launch jump-held (`GSTN_CombatSimSurfaceLaunch.cpp`, Jump); tech on landing
(`GSTN_TumbleLanding.cpp`, Shield); shield press slot (`GSTN_DefenceLibrary.cpp`, Shield; the ability's `HoldButton` is a data default); mount
dismount/drop (`GSTN_MountLoadout.cpp`, Interact + Crouch); spin drill slot (`GSTN_SpinSlots.cpp`, Crouch) and drill track exit
(`GSTN_DrillTrackHook.cpp`, Jump; `Bore.HoldButton` is a data default); default movement/standard slot sets (`GSTN_MovementSlots/StandardSlots.cpp`,
written into editable slot set assets, so data after setup). Each one is a small K2-style slice.
**Gate**: builds b1, b2 ok (17 s, 11 s). Headless GSTN **1508/0** (+1), 0 ensures (first run caught the wrong tag namespace, fixed).
Harness via `batch_driver.py`: `SUITE_HELD` 8/8 and `SUITE_KIT`+`SUITE_ARCHETYPES` 8/8 at listen 0 %, listen 2 %, dedicated 0 % (~5.5 min each).
SUITE_DEFAULT one PIE (`R31_ALL=1`) **50/50**, 861 s, 0 ensures.
**Not verified / unseen**: nobody has pressed the buttons in PIE with a pad; no button was actually changed in play (moved buttons are unit-tested only).
Cost: ~60k context, Opus, estimated ~$3-4. Build 11-17 s, headless ~1 min, each harness batch ~5.5 min, suite ~14 min.
