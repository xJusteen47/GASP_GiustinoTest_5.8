# Testing the GAS + NPP + Mover integration

Two layers: Automation tests for the pure combat logic (`GSTNCombatCore`), and a PIE network harness for everything simulated.

## Automation tests (`GSTNCombat.Core.*`)

Pure, deterministic tests of `Plugins/GSTNCombat/Source/GSTNCombatCore` (input history, classifier, matcher, slots). Run them in the editor (Session Frontend > Automation, filter `GSTNCombat`) or headless with the editor closed:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<project>\GASP_Giustino2.uproject" -ExecCmds="Automation RunTests GSTNCombat.Core;Quit" -unattended -nullrhi -nopause -log
```

The result is in `Saved/Logs/GASP_Giustino2.log` (search `Test Completed`). Test names are `GSTNCombat.Core.<Area>.<Case>`; sources are in `Private/Tests/`.

## C++ side (`GASNPPTest/`)

- `UGSTN_GASNPPTestComponent` (on `BP_GASNPP_TestCharacter`): registers the test mapping context on the local pawn, counts Mover rollbacks and simulation ticks (and how many were re-simulations), injects input (`StartHoldingInput`, `StopHoldingInput`, single-frame press) and exposes `GetStateSnapshot()`: a JSON of role, counters, transform/velocity/mode, attributes, tags, active effects, cooldowns. Server and client copies of the same pawn are compared through it.
- `GSTN_TestAbilities`, `GSTN_TestAttributeSet`, `GSTN_TestEffects` (Dash, Buff, stamina regen): throwaway content that exercises prediction, cost, cooldown, attribute modification.
- Game mode `GM_GASNPPTest`, input `IMC_GASNPP_Test`, `IA_GASNPP_Dash`, `IA_GASNPP_Buff`.

## Python side (`Tools/GASNPPTest/gasnpp_test.py`, run in the editor Python console)

PIE setup: dedicated server + 2 clients, "Run Under One Process".

```python
import sys; sys.path.insert(0, r"<project>/Tools/GASNPPTest"); import gasnpp_test as m
m.setup_editor(True)          # before Play: test GameMode on the open level (not saved), no CPU throttling
m.net(60, 15, 2)              # lag ms, jitter ms, packet loss %
m.reset_all(); m.scenario_abilities((1, 2))   # or scenario_forced_rollback / scenario_server_correction / scenario_accounting
print(m.report())             # counters + server vs client state comparison
m.setup_editor(False)         # after Stop: restore GM_Sandbox and throttling
```

- `force_reconcile_on_mode`: forces a reconcile while the client pawn is in a given mode (e.g. on the wall with `GA_WallSlide` active). `np.ForceReconcile` is one global cvar under "Run Under One Process", so it is set again every frame until the target client's rollback counter increases.
- A clean run = server and client snapshots equal after the scenario and **no reconciles that were not forced**, with one known exception (below). Unexpected reconciles almost always mean simulation reads state that is not rolled back (see the rules in gas-npp.md).
- **Known exception (frame-offset resyncs).** Network Prediction re-maps the client's frames to the server's (log: `ReconcileFromInputOffset Old Offset X, New Offset Y`) whenever the server simulated ticks without that client's input, and the client's simulation time jumps with it. Expected causes: turning network emulation on (the added round trip), packet loss, and a hitch. In PIE "Run Under One Process" a hitch stalls server and clients together and costs one offset change per client. With nothing of that kind going on, the offset stays put: 0 changes in 60 s idle at lag 0. Keep the editor's Project Settings tab closed during tests: its image pickers stall the whole process for ~300 ms after editor Python calls. An offset change can make an idle pawn mismatch on the period timer of a periodic effect (`GSTN_GE_StaminaRegen`: "Current period Time MS"), a moving one on Mover. So judge by `mark_log()` before the scenario and `report_reconciles(mark)` after it: `unexplained` (state-mismatch reconciles **not** on the frame of an offset change) must be 0, `forced` counts the `np.ForceReconcile` ones. How the fork recovers from a hitch: `UNetworkPredictionPlayerControllerComponent::AdvanceLastConsumedFrame` and `UNetworkPredictionWorldManager::SendFixedTickInputsToServer`. A client input lost in transit can only be recovered by a redundant copy sent before the server consumes that frame (the gap is filled with the previous input, logged as `Gap in input stream`). Accepting late frames does not help: UE drops reordered packets, and time dilation keeps the server's buffer shallower than the one-way latency, so a late copy always arrives after the frame was consumed (tried and measured, 0 repairs in 30 hitches).

## Asset setup script

`Tools/GAS/setup_gas_assets.py` (editor Python console or headless `UnrealEditor-Cmd -run=pythonscript`): idempotent. Step 1: `IMC_GAS_Abilities` with IA_Jump; step 2: abilities and `GSTN_AbilityInputComponent` on the test character; step 3 (opt-in `--wall-transition`): replaces Falling's `GSTN_WallEntryTransition` by `GSTN_AbilityWallEntryTransition`. Backs Blueprints up to `Saved/Backups/GAS` first.

## When to run what

- Changed an ability, tag, effect or anything in the ability tick: run `scenario_abilities` and `scenario_forced_rollback` with lag/jitter/loss on.
- Changed a wall mode or transition, or Mover sync/aux state: run `scenario_jump_wall` (ground jump, grab the wall, wall slide), `scenario_server_correction` on the wall, and `force_reconcile_on_mode` while in the wall mode.
- Changed the combat model (`Plugins/GSTNCombat`): run `scenario_combat_spike` (combat button 0 -> test dash, button 1 -> hitstop window, forced reconciles across both) with lag/jitter/loss; `combat_tap(client, mask)` sends a one-frame press. Turn on `np.PrintReconciles 1` in every world: the harness rollback counter counts each rollback twice, the log shows which ones were forced.
- Other helpers: `start_mode_log` / `stop_mode_log` (mode transitions), `measure_smoothness` (visual smoothing of the simulated pawn), `server_apply` / `server_remove` (apply or remove an effect on the server), `scenario_server_dash`.
