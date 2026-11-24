# GSTNMode_Levels

Levels mode (M12 G6, 2026-11-23; [game-modes.md](../../../docs/architecture/game-modes.md) §5). A **Game Feature** plugin (`Plugins/GameFeatures/`), experimental 0.1,
`EnabledByDefault` false, listed in the `.uproject`, **off at runtime** until a level's GameRules asset enables the Levels piece (or `gstn.Levels.Enabled 2`).

## What it does
- **Level set** (`UGSTN_LevelSetDefinition`, GM.3): ordered list of maps with optional travel options, `bLoop`, `ReturnMap`. Entries without a map are skipped.
- **Goal zone** (`AGSTN_LevelGoalZone`, GM.5): an objective zone. ONE alive player inside starts a grace timer (`GoalGraceSeconds`, default 10 s, per-zone
  override); the level finishes when it runs out, or earlier when every alive player is inside. Downed players (events `DownEvents` / `UpEvents`) are not alive.
- **Collectible** (`AGSTN_LevelCollectible`): the mission framework's collectible with `Levels.Collectible` as default identity. The subsystem counts
  `Message.Mission.Collected` per team and per level (no save in v1).
- **Score** (`UGSTN_ScoreRules`): counters (event query -> points, optional magnitude), par-time bonus, completion bonus, ranks. `MakeDefault` has collectibles, kills, deaths. TEST VALUES.
- **Results + travel**: `AGSTN_LevelStateInfo` replicates phase, grace end, collectibles and results; a placeholder Slate screen shows them
  (replace it with UMG reading the info actor). After `ResultsSeconds` the server travels to the next level (seamless by default; the project needs a transition map).
- **Friendly fire ON by default** (GM.13): `UGSTN_LevelsRules::bFriendlyFire`, applied to a transient copy of the combat rules with `GSTNLevelsLogic::ApplyFriendlyFire`.
- **Player count scaling** (GM.2): `UGSTN_LevelsRules::Scaling`, read with `UGSTN_LevelsSubsystem::GetPlayerScaling`.

## Module
`GSTNMode_LevelsRuntime` (one module on purpose). Dependencies: engine, GSTNCore, GSTNInputCore, GSTNCombatCore, GSTNMissionCore/Runtime, GSTNRespawnCore.
Never GSTNRespawnRuntime and never another mode: Levels meets Respawn through events on the authoritative stream and sibling pieces of one GameRules asset
(`GSTNLevelsLogic::BuildLevelsGameRules` builds levels + respawn + missions with the plan's defaults).

## Rules
- Server only, outside the simulation: it listens to the authoritative event stream (a re-simulated event is never counted) and polls zone positions at `PollHz`.
- Pure logic (`FGSTN_GoalGraceTracker`, `FGSTN_LevelTally`, `GSTNLevelsLogic::ComputeResults`) has no world, so the tests drive it with numbers.

## Tags (native)
`Levels.Collectible`; events `Message.Levels.LevelStarted`, `GoalGraceStarted`, `GoalReached`, `LevelCompleted`, `LevelSetCompleted` (server stream only).

## Debug
`gstn.Levels.Enabled` (0 rules, 1 off, 2 on), `gstn.Levels.FriendlyFire` (-1 rules, 0, 1). Commands `gstn.Levels.Status`, `gstn.Levels.Complete`, `gstn.Levels.Next`.

## Tests
`GSTNMode_Levels.*` in `Private/Tests/GSTN_ModeLevelsTests.cpp`: level set ordering, goal grace (one player, edges), team collectible tally, score calculation, friendly fire default.

## Not yet
UMG results asset, persistent save, hub/lobby that picks a set, example level (G7), PIE network pass (G8).
