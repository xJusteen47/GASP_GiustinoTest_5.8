# IO-1: per local player key bindings (desktop, 2026-11-11)
Branch `claude/desktop-io-1` from `614a0cff` (tip of `claude/desktop-m11-4`). Plan: `docs/architecture/plans/input-opzioni-per-giocatore.md`
(step 0, written from the laptop design thread's summary). Doc: `Plugins/GSTNInput/README.md` "Per local player key bindings (IO-1)".
**Done**: `UGSTN_KeyBindingsSubsystem` (GSTNInputCore, `ULocalPlayerSubsystem`): each local player owns its key -> Input Action overrides
(`FGSTN_KeyBindingSet`: action name + default key -> key; empty = today's mapping contexts). Menu API: `ListBindableMappings`, `Rebind`,
`ResetBinding`, `ResetAllToDefault`, `SetBindings`, `Save`, `Load`, `OnBindingsChanged`. Applied through the engine's Enhanced Input user
settings (on by default in 5.8; nothing added to Project Settings or ini): on each key-mapping rebuild a new context is made rebindable
(`FGSTN_KeyBindingsApply::EnsureMappable`: transient `PlayerMappableKeySettings` named after the action, never saved), registered, overrides applied.
Automatic for every local player; no Blueprint, no asset edited. Network/NPP untouched: the fork binds actions, not keys.
**Choices (rule 6)**: (a) own save slot `GSTNInputBindings_P<local player index>`, because the engine settings save to one slot for all
players and the PC save system ignores the user index (two split-screen players would share a file); the engine settings object is never
saved. (b) Identity injected at runtime instead of editing the Input Action assets (binary, versioned). (c) Reflection reads the protected
`UEnhancedPlayerInput::AppliedInputContextData` (no public getter in 5.8).
**Gate**: builds b1 failed (protected getter), b2 ok (4 s). Headless GSTN **1511/0** (+3: `GSTNInput.Core.Bindings.{Set, TwoLocalPlayers,
EnhancedInputProfiles}`: two players with different layouts, separate slots, save/load, engine profiles per player), 0 ensures.
SUITE_DEFAULT one PIE (`R31_ALL=1`): **50/50**, 855 s, 0 ensures, 0 Enhanced Input warnings. Driver `Saved/IO1/suite.ps1`, logs `Saved/Logs/io-1/`, `Saved/IO1/`.
**Unseen in PIE / not covered**: nobody has moved a key in a running game; the live path (subsystem picks up the ability and camera
contexts at possession, rebuild applies the player key) runs in every PIE of the suite but no test moves a key there and checks the action
fires. Two real split-screen players were never started. Shipping game target builds (20 s); not cooked or run.
Owner check, optional (PIE, Python console, host window): `import unreal; w=unreal.EditorLevelLibrary.get_game_world();
s=unreal.SubsystemBlueprintLibrary.get_local_player_subsystem(unreal.GameplayStatics.get_player_controller(w,0), unreal.GSTN_KeyBindingsSubsystem);
print([(str(r.action), r.default_key, r.current_key) for r in s.list_bindable_mappings()])`, then `s.rebind(<action>, <default key>, <new key>)`
and press the new key (untested snippet).
**Seen in passing**: `UGSTN_LoadoutSubsystem` (GSTNCombatGAS) saves to one slot name with the controller id as user index: on PC two
split-screen players would share the loadout file too (same cause as (a)). Not fixed here.
**Next** (plan order): GR-1 GameRules with the Held piece; IO-2 button-layout asset; IO-3 per-player behaviour options; the menu over this API.
Cost: ~120k context, Opus, estimated ~$4-6. Wall clock ~1 h 15; build 4-11 s, headless 55 s, Shipping 20 s, suite ~14 min.
