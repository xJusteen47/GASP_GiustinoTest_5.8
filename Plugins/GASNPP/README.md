# GASNPP: the GAS + Network Prediction fork

`Plugins/GASNPP/` holds the plugins that make the Gameplay Ability System run inside the Network Prediction plugin
(the "GAS-NPP fork"): abilities, effects, tags, attributes, cues and montages are predicted on the owning client and
rolled back and re-simulated on server corrections, instead of using GAS's own prediction keys and RPCs. The fork is
the work of Kai / Sirkai (Yohoho Productions), ported to UE 5.8 by this project. Design notes:
[docs/architecture/gas-npp.md](../../docs/architecture/gas-npp.md).

## Plugins in this folder

| Plugin | What it is | README |
|---|---|---|
| `NetworkPrediction` | Epic's Network Prediction plugin (fixed-tick simulation, rollback, input buffer), with the project's rollback and hitch fixes. Overrides the engine plugin of the same name. | [NetworkPrediction/README.md](NetworkPrediction/README.md) |
| `NetworkPredictionExtras` | Epic's NP samples and mock simulations; kept so the fork builds and tests as a unit. | [NetworkPredictionExtras/README.md](NetworkPredictionExtras/README.md) |
| `NetworkPredictionInsights` | Epic's Unreal Insights trace viewer for NP; shows the ability simulation's frames, reconciles and task names. | [NetworkPredictionInsights/README.md](NetworkPredictionInsights/README.md) |
| `GameplayAbilities` | Epic's Gameplay Ability System with the small hooks the fork needs (`Np*` effect container functions, simulation time, friend access). Overrides the engine plugin. | [GameplayAbilities/README.md](GameplayAbilities/README.md) |
| `AbilitySystemSimulation` | The fork proper: `UNpAbilitySystemComponent` (an NP simulation), `UNpGameplayAbility`, prediction tasks, sync variables, Mover move effects, synced montages, targeting and projectiles. | [AbilitySystemSimulation/README.md](AbilitySystemSimulation/README.md) |

Every project change made on top of the port is listed in [PATCHES.md](PATCHES.md), with the commit and the reason.

## How they layer

```
NetworkPrediction (+ Extras, Insights)        fixed tick, input buffer, rollback, trace
        ^
GameplayAbilities fork                        stock GAS + hooks that let a subclass drive effect timing and restore state
        ^
AbilitySystemSimulation                       UNpAbilitySystemComponent = one NP simulation per pawn; UNpGameplayAbility
        ^
GSTNInputNP, GSTNCombatNP, Source/GASP_Giustino2/.../GAS/     project simulations and abilities
```

- `GameplayAbilities` does not depend on NP: it only exposes what `AbilitySystemSimulation` overrides.
- `AbilitySystemSimulation` depends on both, plus Mover and MotionWarping (effects that move the character go through
  Mover queued moves). Mover itself is a project copy in `Plugins/EngineOverrides/Mover` so it builds against this NP.
- In the fixed tick the ability system model (`AbilitySystemSimulation`, sort priority `PreKinematicMovers - 2`) runs
  before Mover: tags and effects granted in a frame are seen by Mover in the same frame; Mover's new mode is seen by
  abilities one frame later. Project simulations that must act before the ability system (the combat model) sort
  earlier and use `UNpAbilitySystemComponent::OnSimulationPreTick`.

## Rules that follow from the fork

- Only `UNpGameplayAbility` children can be granted; stock `UGameplayAbility` subclasses are rejected with an error
  log. In this project derive from `UGSTN_GameplayAbility` (`Source/GASP_Giustino2/Public/GAS/`).
- Simulation code is deterministic and rollback-safe: state across frames lives in sync variables or sync/aux state,
  no net-role branches, no reads of game-frame component state. The full list is in
  [gas-npp.md](../../docs/architecture/gas-npp.md), "Hard rules for simulation code".
- Changes to the fork are verified with the PIE harness (`Tools/GASNPPTest/gasnpp_test.py`), see
  [testing.md](../../docs/architecture/testing.md) and the last section of [PATCHES.md](PATCHES.md).

## Where to read more

[gas-npp.md](../../docs/architecture/gas-npp.md) · [combat.md](../../docs/architecture/combat.md) (sections 12.7 and 17
for the network model and the spike) · [testing.md](../../docs/architecture/testing.md) · [PATCHES.md](PATCHES.md) ·
[EngineOverrides/Mover/README.md](../EngineOverrides/Mover/README.md)
