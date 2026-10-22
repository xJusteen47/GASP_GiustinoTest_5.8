# Testing the GAS + NPP + Mover integration

Two layers: Automation tests for the pure input and combat logic (`GSTNInputCore`, `GSTNCombatCore`), and a PIE network harness for everything simulated.

## Automation tests (`GSTNInput.Core.*`, `GSTNCombat.Core.*`)

Pure, deterministic tests. `GSTNInput.Core.*` (input history, classifier, matcher, slots, resolver) live in `Plugins/GSTNInput/Source/GSTNInputCore/Private/Tests/`; `GSTNCombat.Core.*` (the standard command set, the combat tags and the project's combat settings) in `Plugins/GSTNCombat/Source/GSTNCombatCore`. 63 tests today: 56 input, 7 combat. Run them in the editor (Session Frontend > Automation, filter `GSTNInput` or `GSTNCombat`) or headless with the editor closed (the command runs both prefixes; a second `RunTests` in the same `-ExecCmds` may be dropped, so use `+`):

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<project>\GASP_Giustino2.uproject" -ExecCmds="Automation RunTests GSTNInput+GSTNCombat;Quit" -unattended -nullrhi -nopause -log
```

The result is in `Saved/Logs/GASP_Giustino2.log` (search `Test Completed`). Test names are `GSTNInput.Core.<Area>.<Case>` and `GSTNCombat.Core.<Area>.<Case>`; sources are in `Private/Tests/`.

## C++ side (`GASNPPTest/`)

- `UGSTN_GASNPPTestComponent` (on `BP_GASNPP_TestCharacter`): registers the test mapping context on the local pawn, counts Mover rollbacks and simulation ticks (and how many were re-simulations), injects input (`StartHoldingInput`, `StopHoldingInput`, single-frame press) and exposes `GetStateSnapshot()`: a JSON of role, counters, transform/velocity/mode, attributes, tags, active effects, cooldowns. Server and client copies of the same pawn are compared through it.
- `GSTN_TestAbilities`, `GSTN_TestAttributeSet`, `GSTN_TestEffects` (Dash, Buff, stamina regen): throwaway content that exercises prediction, cost, cooldown, attribute modification.
- Game mode `GM_GASNPPTest`, input `IMC_GASNPP_Test`, `IA_GASNPP_Dash`, `IA_GASNPP_Buff`.

## Python side (`Tools/GASNPPTest/gasnpp_test.py`, run in the editor Python console)

**Primary configuration: listen server.** One player hosts and plays, the others are clients; "Run Under One Process". World 0 is the host (its pawn is authority **and** locally controlled), worlds 1..N are clients. The dedicated server (world 0 with no local player, clients 1..N) stays as a secondary check.

```python
import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import gasnpp_test as m
m.setup_editor(True)          # before Play: test GameMode + PIE settings = listen, host + 2 clients (not saved), no CPU throttling
                              # secondary check: m.setup_editor(True, 'dedicated', 2)
m.net(60, 15, 2)              # lag ms, jitter ms, packet loss %
m.check_topology()            # after Play: (mode, problems); [] = roles are what the mode promises
m.reset_all(); m.scenario_abilities()   # every player, the host included; or scenario_forced_rollback / scenario_server_correction / scenario_accounting
print(m.report())             # counters + authoritative copy vs every other world's copy of each pawn
m.setup_editor(False)         # after Stop: restore GM_Sandbox, throttling and the PIE settings
```

- `LevelEditorPlaySettings` is **not exposed to Python** in 5.8 (checked live), so `setup_editor` only does the GameMode and throttling part and returns what to start PIE with. Start PIE with the play tool (Rider MCP `ue_play`: `netMode=listen, players=3, runUnderOneProcess=true`; dedicated: `netMode=client, dedicatedServer=true, players=2`) or by hand in Play > Advanced settings (listen: players = host + clients). `ue_play` persists these into the editor's PIE settings; restore them afterwards (ConfigSettingsToolset, section `LevelEditor/PlayIn`: `PlayNetMode`, `PlayNumberOfClients`, `RunUnderOneProcess`, `bLaunchSeparateServer`). Do not `time.sleep` inside an editor Python call: it blocks the game thread and the scenario timeline with it. Start the scenario, let the call return, wait ~15 s, then call `report()`: a report taken while the scenario runs shows transient position/stamina differences.
- Input injection is the same path for every player: `hold` / `release` / `press(index, action)` go through the Enhanced Input subsystem of the world's local player. In listen `index 0` drives the host's own pawn; on a dedicated server it raises (no local player). `force_reconcile*` refuse index 0 (the authority never reconciles). Scenarios take a world `index`; targets (`server_apply`, `server_correction`, `accounting`, `server_dash`) default to the pawn key of that world's player (`key_of_player`), since player ids depend on join order.
- What each `CMP` line compares (the authoritative copy is always the one in world 0; position, mode, attributes, tags, combat history as before):
  - `host-local(w0) vs simulated(wN)`: the host's own pawn against a client's view of it (listen only).
  - `server-copy(w0) vs autonomous(wN)`: a client's own pawn against its authoritative copy on the host/server.
  - `server-copy(w0) vs simulated(wM)`: a client's pawn as another client sees it.
  - `CMP RESULT: k/n OK` closes the report; `report(pos_tol, proxy_pos_tol)` can loosen the position tolerance for simulated (interpolated) copies. The first line of `report()` carries the `check_topology()` problems, if any.

- `force_reconcile_on_mode`: forces a reconcile while the client pawn is in a given mode (e.g. on the wall with `GA_WallSlide` active). `np.ForceReconcile` is one global cvar under "Run Under One Process", so it is set again every frame until the target client's rollback counter increases.
- With network emulation on, packet loss (`m.net(60, 15, 2)`) alone causes a few Sync/Aux mismatch reconciles around dashes (seen live in listen: 3 in one `scenario_abilities` run, none at `m.net(60, 15, 0)`), so `unexplained` can be non-zero at 2% loss; judge the code at 0% loss. A clean run = host/server and client snapshots equal after the scenario and **no reconciles that were not forced**, with one known exception (below). Unexpected reconciles almost always mean simulation reads state that is not rolled back (see the rules in gas-npp.md).
- **Known exception (frame-offset resyncs).** Network Prediction re-maps the client's frames to the server's (log: `ReconcileFromInputOffset Old Offset X, New Offset Y`) whenever the server simulated ticks without that client's input, and the client's simulation time jumps with it. Expected causes: turning network emulation on (the added round trip), packet loss, and a hitch. In PIE "Run Under One Process" a hitch stalls server and clients together and costs one offset change per client. With nothing of that kind going on, the offset stays put: 0 changes in 60 s idle at lag 0. Keep the editor's Project Settings tab closed during tests: its image pickers stall the whole process for ~300 ms after editor Python calls. An offset change can make an idle pawn mismatch on the period timer of a periodic effect (`GSTN_GE_StaminaRegen`: "Current period Time MS"), a moving one on Mover. So judge by `mark_log()` before the scenario and `report_reconciles(mark)` after it: `unexplained` (state-mismatch reconciles **not** on the frame of an offset change) must be 0, `forced` counts the `np.ForceReconcile` ones. How the fork recovers from a hitch: `UNetworkPredictionPlayerControllerComponent::AdvanceLastConsumedFrame` and `UNetworkPredictionWorldManager::SendFixedTickInputsToServer`. A client input lost in transit can only be recovered by a redundant copy sent before the server consumes that frame (the gap is filled with the previous input, logged as `Gap in input stream`). Accepting late frames does not help: UE drops reordered packets, and time dilation keeps the server's buffer shallower than the one-way latency, so a late copy always arrives after the frame was consumed (tried and measured, 0 repairs in 30 hitches).

## Asset setup script

`Tools/GAS/setup_gas_assets.py` (editor Python console or headless `UnrealEditor-Cmd -run=pythonscript`): idempotent. Step 1: `IMC_GAS_Abilities` with IA_Jump; step 2: abilities and `GSTN_AbilityInputComponent` on the test character; step 3 (opt-in `--wall-transition`): replaces Falling's `GSTN_WallEntryTransition` by `GSTN_AbilityWallEntryTransition`. Backs Blueprints up to `Saved/Backups/GAS` first.

## When to run what

- Changed an ability, tag, effect or anything in the ability tick: run `scenario_abilities` and `scenario_forced_rollback` with lag/jitter/loss on.
- Changed a wall mode or transition, or Mover sync/aux state: run `scenario_jump_wall` (ground jump, grab the wall, wall slide), `scenario_server_correction` on the wall, and `force_reconcile_on_mode` while in the wall mode.
- Changed the combat model (`Plugins/GSTNCombat`): run `scenario_combat_spike` (combat button 0 -> test dash, button 1 -> hitstop window, forced reconciles across both) with lag/jitter/loss; `combat_tap(client, mask)` sends a one-frame press. Turn on `np.PrintReconciles 1` in every world: the harness rollback counter counts each rollback twice, the log shows which ones were forced.
- Other helpers: `start_mode_log` / `stop_mode_log` (mode transitions), `measure_smoothness` (visual smoothing of the simulated pawn), `server_apply` / `server_remove` (apply or remove an effect on the server), `scenario_server_dash`.
