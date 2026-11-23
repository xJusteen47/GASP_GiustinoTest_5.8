# IO-2: single button layout (desktop, 2026-11-11)
Branch `claude/desktop-io-2` from `b934f808` (tip of `claude/desktop-gr-1`). Plan: `docs/architecture/plans/input-opzioni-per-giocatore.md` (IO-2).
**Done**: `UGSTN_ButtonLayout` (GSTNInputCore `Layout/GSTN_ButtonLayout.h`), a GameRules piece "Button layout": list function name -> logical
button (`Input.Button.*`), `GetButton(Function, Default)`. Defaults = class defaults from `DefaultGSTNInput.ini` section
`[/Script/GSTNInputCore.GSTN_ButtonLayout]` (config class, not in Project Settings). GSTNInput now depends on GSTNCore (uplugin + Build.cs).
Moved: Held `PickupDropButton`, `ThrowButton` (out of `UGSTN_HeldSettings`) and `KitSwap.Button` (out of `UGSTN_HeldRules`) -> functions
`Held.PickupDrop`, `Held.Throw`, `Held.KitSwap`. C++ points: mount interact + crouch (`Mount.Interact`, `Mount.Crouch`), spin crouch
(`Spin.Drill`), drill exit jump (`Drill.Exit`), surface launch jump hold (`Surface.LaunchBoost`). Same buttons by default. README section added.
**Choice (rule 6)**: the layout is a piece of the GameRules asset (per mode/level, chosen by the server like GR-1), not a separate asset
reference. IO-3 per-player options should layer per local player on top of it (never edit the piece). Function ids are FNames so a plugin
adds functions without declaring tags.
**Gate**: builds b1/b2 ok (62 s, 13 s). Headless GSTN **1513/0** (+1 `GSTNInput.Core.Layout.Defaults`), 0 ensures. Shipping game target ok
(38 s). SUITE (`R31_ALL=1`, listen): **50/50**, 859 s, 0 ensures. Logs `Saved/IO2/`, `Saved/Logs/io-2/`.
**Not done (next thread)**: (a) shield: `GSTN_DefenceLibrary.cpp:166` (binding press) and `GSTN_ShieldAbilityData.cpp:10` (HoldButton
default, set in a constructor: read the layout where the spec is used, not in the ctor) plus `GSTN_TumbleLanding.cpp:178`; change all three
together or the press and the hold disagree. (b) jump in other places and the "two right-stick buttons": not found by grep on
`Button_*` in .cpp (look for the right-stick buttons in the movement set / camera input). (c) Link: no button field in its Project Settings;
`GSTN_LinkSlots.cpp:105-205` still hardcodes Interact/Attack/Strong/Crouch/Special/Jump; `GSTN_MovementSlots.cpp`, `GSTN_StandardSlots.cpp`
(GSTNCombat) too. Same pattern: `UGSTN_ButtonLayout::GetButton(TEXT("Link.X"), default)` + a line in the ini + a test row.
**GR-1 limit (1) not fixed, now also for the layout**: kit bindings compile when the kit loads (`GSTN_HeldRuntimeModule.cpp:85`
ContributeCompileInput, Mount/Spin slots), maybe before the server picks the GameRules asset. Needed: a `FGSTN_ActiveGameRules::OnChanged`
multicast in GSTNCore, fired by `UGSTN_GameRulesSubsystem` (server pick) and `AGSTN_GameRulesInfo` OnRep (client); the loadout subsystem
(GSTNCombatGAS) listens and recompiles the bound kits' tables. Only defaults are safe today.
**Unseen**: nobody made a GameRules asset with a Button layout piece or moved a function to another button in PIE.
Cost: ~70k context, Opus, estimated ~$3-4. Wall clock ~40 min. Code tip `19bda380`; the branch tip is the handover commit after it.
