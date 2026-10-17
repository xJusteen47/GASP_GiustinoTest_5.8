# Testing the GAS + NPP + Mover integration

There are no automation tests; verification is a PIE network harness.

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
- A clean run = server and client snapshots equal after the scenario and **no reconciles that were not forced**. Unexpected reconciles almost always mean simulation reads state that is not rolled back (see the rules in gas-npp.md).

## Asset setup script

`Tools/GAS/setup_gas_assets.py` (editor Python console or headless `UnrealEditor-Cmd -run=pythonscript`): idempotent. Step 1: `IMC_GAS_Abilities` with IA_Jump; step 2: abilities and `GSTN_AbilityInputComponent` on the test character; step 3 (opt-in `--wall-transition`): replaces Falling's `GSTN_WallEntryTransition` by `GSTN_AbilityWallEntryTransition`. Backs Blueprints up to `Saved/Backups/GAS` first.

## When to run what

- Changed an ability, tag, effect or anything in the ability tick: run `scenario_abilities` and `scenario_forced_rollback` with lag/jitter/loss on.
- Changed a wall mode or transition, or Mover sync/aux state: run `scenario_jump_wall` (ground jump, grab the wall, wall slide), `scenario_server_correction` on the wall, and `force_reconcile_on_mode` while in the wall mode.
- Other helpers: `start_mode_log` / `stop_mode_log` (mode transitions), `measure_smoothness` (visual smoothing of the simulated pawn), `server_apply` / `server_remove` (apply or remove an effect on the server), `scenario_server_dash`.
