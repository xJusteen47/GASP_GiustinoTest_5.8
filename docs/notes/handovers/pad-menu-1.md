# Loadout screen on the pad, GASP input conflicts, loadout debug pad (laptop, UE 5.9, 2026-11-13)
Branch `claude/project-thread-vzctjx` from `3f615f49` (keexim). Worktree `GASP_Giustino2_padmenu` (laptop), Content junctions as `_m7`.
**Menu dead on the pad (fixed)**: the menu copied the player's keys into its context at the pawn's first frame, before Enhanced Input
had rebuilt the player's mappings: the context was EMPTY (seen with the new `GSTN.InputDump`), so no key reached it and the character kept
playing. Now the copy reads after an immediate rebuild and is redone whenever another action's key reaches the player (contexts added or
removed after opening). PIE check (pad keys injected with `InputToolset.ApplyInput`): stick down/right/up/right + A = rows and kit changed,
loadout sent; X swallowed; 0 cm moved while open; B closes; stick then moves 258 cm.
**GASP conflicts (fixed by data)**: GASP's `SandboxCharacter_Mover` adds `IMC_Sandbox` (prio 0) at BeginPlay. Live on the pad were DPad L =
next pawn (the likely source of the watcher's `KeyError('P264')`), DPad R, both stick clicks, Right2D twin-stick; on the keyboard Space =
GASP jump + traversal, R ragdoll, J teleport, N/M. New `UGSTN_InputConfig::MappingContexts` / `ReplacedMappingContexts`, applied each frame
by `UGSTN_PlayerInputSource` (`ApplyMappingContexts`). `IC_GASNPP_Test` replaces `IMC_Sandbox` with `IMC_GSTN_Locomotion` (copy keeping
only Move WASD/left stick, Look mouse/right stick, C = IA_Crouch). Combat buttons stay in `IMC_GASNPP_Test` (prio 10). No Blueprint edited.
Script: `Tools/GAS/setup_player_input_contexts.py` (idempotent). Note: Space no longer jumps (our jump is Y / pad A).
**"Menu never reopens after a kit"**: not reproduced: host and listen client both reopen after a kit change (toggle + server kit apply).
Likely the empty-context menu was already "open". `GSTN.LoadoutMenu` now prints why it cannot open (screen + log).
**Loadout debug pad (new)**: `AGSTN_LoadoutMenuPad` (C++, distance check, local players only), placed at Play from the rules' `Pads` list
(`DefaultGSTNLoadoutMenu.ini`: hub (1200, 1800), next to the hitbox pad); no map saved. Opens the menu as a bare text list (rows with
index/count, applied kit, verdict). Also `GSTN.LoadoutMenuDebug`. Screenshot checked once.
**Menu pick not reaching the simulation (fixed, 2nd commit)**: `ServerRequestLoadout` only ran `AcceptLoadout`: abilities granted, but the
combat sim kept the old revision's table (no `ServerWriteLoadoutRevision`), so X/Y/grab/pickup did nothing after a menu pick. The sim now
sets `UGSTN_LoadoutComponent::SetServerLoadoutApplier` -> `ServerApplyLoadoutChange` (also for special swaps). PIE: menu-path MEDIUM X 0 -> 7.
Light/Medium/Heavy have no RT binding (only KIT_Example, KIT_Test_Ranger with reticle): owner asked whether to add one. Hub "two rows" =
front kit row at x 800 (no pads) behind which the pad row at x 1200 shows through; not a duplicate.
**test_hub watcher**: `_pad_check` skips players without a snapshot entry (pawn switched, client not replicated) instead of raising each tick.
**Gates**: build ok; headless GSTN **1584/0**, 0 ensures (both commits) (`Saved/PadMenu/headless.log`). PIE checks above on standalone + listen 1 client. Harness suite not run (local input only).
**Owner**: push this branch to feature; desktop `_ue59` needs his editor closed to pull + build (ask first).
Not done: menu mouse/keyboard beyond the copied keys; `PM_BoostJump`/`PM_ThrowRider` show modified right after checkout (LFS), not committed.
Cost: ~180k context, Opus, no subagents; estimated ~$15-20.
