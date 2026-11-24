# GSTNExperience

Experiences for the mode framework (M12 G0/G1, owner decision D5 = A, 2026-11-21; [game-modes.md](../../docs/architecture/game-modes.md) §2.2,
[modularity-and-tags.md](../../docs/architecture/modularity-and-tags.md) §3.4). Experimental 0.1, `EnabledByDefault` false, enabled in the `.uproject`.
Lyra's experience pattern, trimmed to what the project needs. **Not compiled or run yet** (written without the engine): see the handover
`docs/notes/handovers/m12-g0-g1.md`.

## Module
| Module | What |
|---|---|
| `GSTNExperience` (Runtime) | `UGSTN_ExperienceDefinition`, `UGSTN_PawnData`, `UGSTN_ExperienceActionSet` (data assets); `UGSTN_ExperienceManagerComponent` (GameState component, replicated `CurrentExperience`, load state machine); `UGSTN_ExperienceWorldSubsystem` (adds the manager to the GameState and registers the GameState as a Game Framework receiver); the simulation gate (`IGSTN_ExperienceGatedSimulation`, `UGSTN_ExperienceGateComponent`, `UGSTN_ExperienceGateLibrary`); `UGSTN_ExperienceSettings`. |

Dependencies: engine only (`GameFeatures`, `ModularGameplay`, `GameplayTags`, `NetCore`, `DeveloperSettings`). No GAS, Network Prediction, Mover or other
GSTN plugin, so it works without the GAS-NPP fork (modularity 3.7). The NP adapters call the gate; the gate never calls them.

## Data
- `UGSTN_ExperienceDefinition` (primary asset type `GSTN_Experience`): `ModeTag` (`Experience.Mode.*`), `GameFeaturesToEnable` (plugin names), `DefaultPawnData`,
  `ActionSets`, own `Actions`. Const, shared by all machines.
- `UGSTN_PawnData` (`GSTN_PawnData`): `PawnClass`, `AbilitySets`, `InputConfig` as soft references (bundles `Client` / `Server`); typed assets belong to G2.
- `UGSTN_ExperienceActionSet` (`GSTN_ExperienceActionSet`): a reusable list of `UGameFeatureAction`s plus the Game Features it needs.
- The three types are scanned by the Asset Manager from `/Game/GSTN` (`Config/DefaultGame.ini`, `PrimaryAssetTypesToScan`); add a directory there for a mode's content.

## Load order (host and client, same steps)
`Unloaded -> Loading` (asset bundles) `-> LoadingGameFeatures` (Experience plugins, then each action set's, deduplicated) `-> ExecutingActions` (own actions,
then each set's) `-> Loaded` (= the spec's "Active"). The host calls `SetCurrentExperience`; clients start from `OnRep_CurrentExperience`. A feature that fails
to activate is logged and listed in `GetFailedGameFeaturePlugins()`; the Experience still reaches Loaded. Use `CallOrRegister_OnExperienceLoaded[_HighPriority]`.

## Simulation gate (spike G4)
Rule (`FGSTN_ExperienceGate::IsOpen`): a world with **no Experience assigned is never gated** (classic GameMode, harness maps unchanged); once one is assigned the gate is
closed until `Loaded`. A pawn (or its sim component) puts `UGSTN_ExperienceGateComponent` on itself, asks `ShouldRunSimulationTick()` before each tick and calls
`ReportSimulationTickRan()` after. Refusals are counted (`GetHeldSimTicks`); a tick that ran while closed is a leak (`GetLeakedSimTicks`, spike G4 expects 0).
The game mode asks `UGSTN_ExperienceGateLibrary::CanSpawnPawns`. No fork change: the gate sits on the caller's side. Whether that holds for a pawn already registered
with the NP service is what the PIE measurement of G4 decides (plan m12 §5: stop if it cannot be held back without a fork change).

## GameState component action
`UGSTN_ExperienceWorldSubsystem` registers every `AGameStateBase` with `UGameFrameworkComponentManager` and adds the manager to it through the same
`AddComponentRequest` path that `UGameFeatureAction_AddComponents` uses, so one mechanism serves both. Settings (Project Settings > GSTN > Experience): turn
`bRegisterGameStateAsReceiver` off if the GameState derives from `AModularGameStateBase`, `bAddManagerToGameState` off if it owns the component already.
`DefaultGameFeaturesProjectPolicies` needs no ini change (the engine default policy is enough until a mode ships a Game Feature).

## Tags
None declared. `Experience.Mode.<Name>` is owned by each mode's Game Feature (ini), per game-modes.md 8.2.

## Tests
`GSTNExperience.*` (`Private/Tests/GSTN_ExperienceTests.cpp`): gate rule, collection order and deduplication, activation order with a fake backend (bundles, then plugins,
then actions, then Loaded, then listeners, then gated simulations), failed feature, no features, shutdown with late completions, and the pre-Active tick holdback with a
negative control (a pawn that ignores the gate is caught). The host/client GameState half of G4 needs PIE: harness scenario `scenario_experience_order` (M12 G8).
Run: `Tools/CI/Run-Timed.ps1 -Preset Test -Tests GSTNExperience`.

## Use in another project
Copy the folder, enable `GameFeatures` and `ModularGameplay`, add the three primary asset types to `PrimaryAssetTypesToScan`. Nothing else.
