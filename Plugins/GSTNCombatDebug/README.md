# GSTNCombatDebug

Development tooling of GSTN Combat for Unreal Engine 5.8 that runs in PIE and development builds: the input display widget
(E14), later the Gameplay Debugger category (E15), debug draw (E16) and the combat state trace (E17). It reads combat state
through const accessors and a trace sink that the runtime modules expose; no runtime module depends on it.

Status: **experimental (0.1)**, `EnabledByDefault: false`. This is the skeleton (checkpoint E1 of
[m5-editor-plan.md](../../docs/architecture/plans/m5-editor-plan.md)); the design is
[combat-m5-editor.md](../../docs/architecture/combat-m5-editor.md) §10. Disabling it removes the tools and nothing else.

## Modules

| Module | State | What it is |
|---|---|---|
| `GSTNCombatDebug` | exists (Runtime, Default, not in Shipping) | `IGSTN_CombatDebugModule` (the interface other plugins use, `CreateInputDisplay()` returns null until E14) and `LogGSTNCombatDebug`. Covered by `GSTNCombat.Debug.*`. |

The descriptor excludes the module from Shipping with `TargetConfigurationDenyList: ["Shipping"]` (the spec's "bIsShippingExcluded"
rule: UnrealBuildTool has no such flag, the descriptor is where a module is kept out of a configuration).

## Dependencies

- Plugins: `GSTNCombat`.
- `GSTNCombatDebug` (`Build.cs`): public Core, CoreUObject, Engine, SlateCore. `GSTNCombatCore`, `GSTNInputCore`, `GSTNCombatNP`
  and `GameplayDebugger` are added by E14 and E15 when the code first reads them. Nothing links `GSTNCombatNP` yet.
- Nobody links this module. The editor plugin reaches it through `IGSTN_CombatDebugModule::Get()` (module name lookup), so each
  side builds and runs without the other.

## Gameplay tags

None declared, none read yet.

## Extending in Blueprint

Not applicable yet. The input display (E14) will be a Slate widget with a UMG wrapper so a designer can place it in a debug HUD.

## Replacing content

The plugin ships no content and references none by path.

## Tests

`Automation RunTests GSTNCombat.Debug`: `Module.LoadsAndExposesInterface`.

## Changelog

- 0.1 (2026-10-23): plugin and module skeleton, module interface stub (plan E1).
