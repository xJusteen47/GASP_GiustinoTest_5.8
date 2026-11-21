# GSTNMission

Missions and objectives (M12 G4 part 1, 2026-11-21; [game-modes.md](../../docs/architecture/game-modes.md) §4). Experimental 0.1, enabled in the
`.uproject`, **off at runtime** until a level's GameRules asset turns it on (or `gstn.Mission.Enabled 2`).

## Modules
| Module | What |
|---|---|
| `GSTNMissionCore` (pure) | `UGSTN_MissionDefinition` (data asset), `FGSTN_ObjectiveSpec`, `UGSTN_ObjectiveRule` (Blueprintable: `OnEvent`, `OnPoll`) with `CountEvents` (eliminate, collect, activate), `Reach`, `Defend`; `UGSTN_MissionRules` (GameRules piece "Missions"); `FGSTN_MissionTracker`; tags. Tests `GSTNMission.Core.*` incl. "10 kills with rollbacks count 10". |
| `GSTNMissionRuntime` | `UGSTN_MissionSubsystem` (server), `AGSTN_MissionStateInfo` (replicated state for UI), `AGSTN_ObjectiveZone`, `AGSTN_Collectible`. |

Dependencies: GSTNCore, GSTNInput only. KOs, verbs, respawns arrive as tagged events on the authoritative stream, so no combat or respawn module is needed.

## Rules
- Progress only from the authoritative stream (`UGSTN_SimEventSubsystem`, host publishes once) and from server polls (zones, collectibles, timers at
  `PollHz`): never from the simulation, so a rollback cannot count twice. Same type + target + frame = one fact (KO credit partner copy).
- Order: `Sequential` or `Parallel` with `Prerequisites` (graph). Failure: objective `FailQuery`, mission `FailQuery`, time limits. Optional objectives and missions.
- Seam D-2 decision: the stream itself is the bus (no `Message.*` republisher, no GameplayMessageRouter in the project). Seam D-1 (timeline event key
  -> outbox) already exists, so an `Activate` objective counts a verb ability's event key directly.

## Tags (native)
Roots `Mission.Def`, `Mission.Objective` (leaves in ini); `Message.Mission.ObjectiveActivated/Progressed/Completed/Failed`, `MissionCompleted/Failed`, `Collected`.

## Debug
`gstn.Mission.Enabled` (0 rules, 1 off, 2 on), `gstn.Mission.Debug`. Commands `gstn.Mission.Status`, `gstn.Mission.Reset`, `gstn.Mission.CompleteActive`.

## Not yet (part 2)
Markers, results screen, Escort/Extract/Retrieve kinds, chains between missions (`UGSTN_MissionSet`), the interact verb (`GSTNModeVerbs`).
