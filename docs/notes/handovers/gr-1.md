# GR-1: GameRules with the Held piece (desktop, 2026-11-11)
Branch `claude/desktop-gr-1` from `769b1fe8` (tip of `claude/desktop-io-1`). Plan: `docs/architecture/plans/input-opzioni-per-giocatore.md` (GR-1).
**Done**: GSTNCore `GameRules/GSTN_GameRules.h`: `UGSTN_GameRules` (primary data asset, one per mode/level, `Instanced` pieces, `Levels`,
`GameModes`), `UGSTN_GameRulesPiece` (abstract, EditInlineNew; a plugin derives its own), `FGSTN_ActiveGameRules::GetPiece<T>()` (active asset's
piece, else T's class defaults), `UGSTN_GameRulesSubsystem` (server at world begin play: URL `?GameRules=/Game/...`, else an asset listing the
level, else one listing the game mode, else none = defaults) and `AGSTN_GameRulesInfo` (always relevant, `COND_InitialOnly`, clients apply on rep).
GSTNHeld: `UGSTN_HeldRules` piece ("Held objects") takes DefaultBreakRules, ThrowInput, PickupMeaningPriority, bPickupTakesFocusedObject,
bFocusMissFallsBackToNearest, SizeClasses, DefaultCarryCapacityKg, DefaultThrowWindup, ThrownEnemies, KitSwap. Same names; ini lines moved to
section `[/Script/GSTNHeldCore.GSTN_HeldRules]` of `DefaultGSTNHeld.ini` (config class, not a DeveloperSettings: not in Project Settings), so
defaults and behaviour are unchanged. Call sites read `UGSTN_HeldRules::Get()`; `gstn.Held.Windup` and `kit_scenarios.py` write the piece.
**Choices (rule 6)**: (a) generic classes in GSTNCore (every plugin may depend on it; Build.cs now Core+CoreUObject+Engine+AssetRegistry).
(b) Defaults stay in the plugin ini as the piece's class defaults (no default asset to create, no Blueprint). (c) One active rules pointer per
process, not per world: the readers have no world (same as the old settings); in one-process PIE all worlds play the same level.
**Buttons (IO-2)**: PickupDropButton, ThrowButton, SlotOverrides, CommandTiers, KitsWithDefaultBindings and the abilities stay in
`UGSTN_HeldSettings` (Project Settings). Exception: `KitSwap.Button` moved with the KitSwap struct (splitting it was not needed now; IO-2 takes it).
**Gate**: builds b1 failed (link, GSTNCore missing in HeldRuntime deps), b2/b3 ok (11 s). Headless GSTN **1512/0** (+1 `GSTNHeld.Core.GameRules`),
0 ensures. Shipping game target ok (28 s). SUITE_DEFAULT one PIE (`R31_ALL=1`, listen): **50/50**, 854 s, 0 ensures. Logs `Saved/GR1/`, `Saved/Logs/gr-1/`. `check_modules.py`: 23 failures,
all pre-existing (Minion, PartsMinion, HeldRuntime->Link), none from GSTNCore.
**Unseen / not covered**: no GameRules asset exists yet; nobody created one, picked it by URL/level/mode or saw a client receive it in PIE.
Known limits: (1) kits compile their bindings (KitSwap hold, size bans) when loaded, possibly before the server's choice: an asset that changes
KitSwap or SizeClasses may not reach kit bindings (defaults do). (2) No gate holds the match until the client has the rules; in practice the
info actor arrives with the first replication. (3) GameRules assets are found via the asset registry: for cooked builds they must be cooked
(Asset Manager type `GSTN_GameRules` or a reference); not configured. (4) Old `Saved/Config` overrides of the moved keys under the settings
section are ignored.
**Owner, optional**: Content Browser > Data Asset > GSTN_GameRules, add piece "Held objects", list the level, PIE and compare.
**Next**: IO-2 button-layout piece; fix limit (1) by recompiling kit bindings on rules change; Link and Projectile pieces when touched.
Cost: ~95k context, Opus, estimated ~$3-5. Wall clock ~25 min; builds 11 s, headless ~1 min, Shipping 28 s, suite ~14 min. Code commit `81df6718`; the tip is the handover commit after it.
