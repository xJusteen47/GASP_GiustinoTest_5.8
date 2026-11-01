# GSTNSpinDrill

Spin and drill for the GSTN combat on Unreal Engine 5.8: a **spin** started by a stick rotation (ground spin, and one air spin per
airtime with a small hop), the **opposite-spin cancel**, and the **drill**: hold crouch during a spin to dive along gravity, latch
onto what the tip hits when it is drillable, bore with repeated hits, then hop out on release or jump out high with a buffered jump.
The design is [spin-drill.md](../../docs/architecture/spin-drill.md); the checkpoints are
[spin-drill-plan.md](../../docs/architecture/plans/spin-drill-plan.md).

Status: **skeleton (0.1)**, `EnabledByDefault: false`. Checkpoint D1 is built: the plugin, its five modules, the native tags of spec
§10 and their test. No data, math, fragment, ability or presentation code yet (D2 onward). The D0 input findings (Spin condition,
keyboard shortcut) are in [spin-drill-spike-report.md](../../docs/notes/spin-drill-spike-report.md).

## Modules

| Module | Type | State | What it will hold |
|---|---|---|---|
| `GSTNSpinDrillCore` | Runtime | native tags, tag ini registration, log category | Data structs, pure math (`UGSTN_DrillMath`), drill target response, native tags. No GAS, NP or Mover. Tests `GSTNSpinDrill.Core.*`. |
| `GSTNSpinDrillNP` | Runtime | empty | Drill sync fragment, combat tick extension, snapshot extension, latched movement mode. Needs the GAS-NPP fork. |
| `GSTNSpinDrillGAS` | Runtime | empty | The five abilities (spin, spin cancel, dive, bore, launch), tuning data asset, kit extension. |
| `GSTNSpinDrillPresentation` | Runtime | empty | Procedural spin and wobble component; clients and listen hosts only. |
| `GSTNSpinDrillTests` | Runtime, not in Shipping | empty | Harness abilities and test content names. Unit tests stay in each module's `Private/Tests`. |

The Core module class registers the plugin's `Config/Tags` folder with the tag manager: the engine reads `Config/Tags` only for the
project and for Game Feature plugins, so the `GameplayCue.SpinDrill.*` leaves (ini only, none yet) would otherwise be missing.

## Dependencies

- Plugins: `GSTNInput`, `GSTNCombat`, `GSTNCombatGAS`, `GSTNCombatNP`, `GameplayAbilities`, `NetworkPrediction`, `Mover`.
- `GSTNSpinDrillCore`: Core, CoreUObject, Engine, GameplayTags, GSTNInputCore, GSTNCombatCore; private Projects (to find the plugin's
  own folder). No GAS, NP or Mover module.
- `GSTNSpinDrillNP`: Core, CoreUObject, Engine, GameplayTags, NetworkPrediction, Mover, GSTNInputCore, GSTNCombatCore, GSTNCombatNP,
  GSTNSpinDrillCore.
- `GSTNSpinDrillGAS`: Core, CoreUObject, Engine, GameplayTags, GSTNCombatCore, GSTNCombatGAS, GSTNSpinDrillCore, GSTNSpinDrillNP.
- `GSTNSpinDrillPresentation`: Core, CoreUObject, Engine, GameplayTags, GSTNSpinDrillCore, GSTNSpinDrillNP.
- `GSTNSpinDrillTests`: the four modules above.
- Never: `GSTNParts`, `GSTNLevelObjects` (their bridges are separate small plugins, spec §4), an Editor module or the project module.
  `GSTNLink` comes in with the latch (D9), through a new link type.

## Tags

Native, declared in `GSTN_SpinDrillTags.h` (`GSTNSpinDrillTags`), listed by `GetDeclaredTags` (21 tags):

| Tags | Kind |
|---|---|
| `Command.Movement.Spin.Clockwise`, `.CounterClockwise`, `.Button`, `Command.Movement.Drill` | command leaves (the `Command.Movement` family is the combat plugin's) |
| `Input.Button.Spin` | optional button leaf, not in the default kit |
| `Combat.State.Spinning`, `.Spinning.Clockwise`, `.Spinning.CounterClockwise`, `.AirSpinSpent`, `.Drilling`, `.DrillLatched` | projected from the drill fragment, never granted |
| `Combat.HitType.Drill` | hit type leaf |
| `Combat.Event.Drill.Latched`, `.Detached`, `.Launched`, `.GroundLag` | events, published once on the authority |
| `Ability.Movement.Spin`, `.SpinCancel`, `Ability.Combat.Drill.Dive`, `.Bore`, `.Launch` | ability identity |

Not declared yet: the `Anim.Set.*` leaves (presentation checkpoint, GSTNAnimation's rules), `Combat.Link.Type.Drill` and its roles
(latch checkpoint), the `GameplayCue.SpinDrill.*` ini leaves.

## How to enable

Off by default. To try it: enable `GSTNSpinDrill` in the `.uproject` (or Edit > Plugins), rebuild, and run the tests
`Automation RunTests GSTNSpinDrill`. Enabling it changes nothing in game until a kit binds its commands (D4 onward).

## Extending

Nothing to extend yet. The extension points (tuning asset, target response overrides, Blueprint hooks on the abilities) arrive with
D2 to D5; see spec §13.
