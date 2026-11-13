# M11 slice 1 (K1): kit swap on the grab button (desktop, 2026-11-11)
Branch `claude/desktop-m11-1` from `c3ffe3a0` (tip of `claude/desktop-m10-10`; the M11 plan docs were already there). Doc: `Plugins/GSTNHeld/README.md` step 17, `combat.md` §10.6.

**Done** (`GSTNHeld`, `GSTN_KitInteract.h`): every `AGSTN_KitPickup` gets a `UGSTN_KitInteractableComponent` (server adds it replicated to spawned kits, every
machine to level-placed ones) = one GSTNInteract record `Interact.Target.Kit`, stamped in combat frames (override of GSTNInteract's server-time clock, which
runs ahead of the NP frame). GSTNLink's probe frames one target per frame, so the framed thing wins (decision 6). The loadout extension gives every **kit
asset** `Command.Kit.Swap` = Release(B) after `HoldSeconds` 0.5 s (31 frames), tier `Command.Kit` 360, offset -3, requires the tag, not carrying.
`UGSTN_GA_KitSwap` asks the host; after the frame the host swaps with the kit the probe framed (else nearest), one swap per kit per frame, through
`UGSTN_KitSwapSubsystem::SwapWithPickup`. The TH-1b hold-near-a-kit trigger is switched off (`bSwapOnInteractHoldEvent`). Prompt "Tieni B: <kit>".
**G0 audit / mid-ability swap**: nothing new needed, `FGSTN_LoadoutSwapLogic` already ends the running move at the revision boundary on every machine.
**Death (decision 5)**: host `Combat.Event.KO` of a human player -> `DeathDrop` = DropAndLose (default: kit dropped under the player, player back to
`RespawnKit` = KIT_Test_Hits), DropCopy (player keeps it) or Off; dropped kit lives `DroppedKitLifetimeSeconds` 30. All in `KitSwap` of `DefaultGSTNHeld.ini`.

**Choices (rule 6)**: code in GSTNHeld (it already joins CombatGAS pickup, Interact records and the Link probe; plan L2 moves it to GSTNPickups); release-based
hold (swap on release after 0.5 s, like the movement set's hold) instead of firing while held; both death behaviours as a setting (owner rule), default
DropAndLose; kits built in memory by code get no swap binding (the GSTNCombat tests count table rows); a second player swapping the same kit on the same
frame gets nothing (not the first player's old kit); priority 90 = pickup, framing decides.

**Gate**: build b1 ok first try, b2 ok. Headless GSTN **1500/0** (+2 `GSTNHeld.Core.KitSwapSlot/KitSwapSettings`), 0 ensures. SUITE_DEFAULT one PIE 49/50:
`super_code` counted 4 bindings on KIT_Test_Sigma, now 5 with the swap -> expectation updated, rerun PASS (with `loadout_change`); `interact_tap_hold` PASS alone
(it fails only after a kit scenario in the same PIE, known run_suite rule). New `kit_scenarios.scenario_kit_swap_hold` (real B input through the combat
input, short hold = no swap, long hold = swap of the framed kit, both machines on the new kit): listen 0 %, listen 2 %, dedicated 0 % **PASS**.
Exit crash at QUIT_EDITOR exists since M10 (same in M10j logs), not from this slice.
**Not verified**: the prompt text and the KO drop (no scenario; KO of a player not scripted), level-placed kits (hub kits are spawned). **Nobody has seen it in PIE with a pad.**

**Next essential slice K2** (Smash kit throws: stick picks the throw, 4 per kit in `FGSTN_LinkKitExtension` + fallback set, grab with special+crouch or
interact tap, liftable -> M10 object flow): start from this branch (touches GSTNHeld/GSTNLink too). K3 kits on hub pads; K4 adds `kit_throw` + a KO-drop scenario.

Cost: ~150k context, Opus, estimated ~$10-13. Build ~1-3 min each, headless ~2 min, suite ~13 min, kit scenario ~1 min per batch.
