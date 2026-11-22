# GSTNRespawn

Lives and respawn of a mode (M12 G3 part 1, 2026-11-21; [game-modes.md](../../docs/architecture/game-modes.md) §5, plan
[m12-modi-livelli.md](../../docs/architecture/plans/m12-modi-livelli.md)). Experimental 0.1, enabled in the `.uproject`, **off at runtime**
until a level's GameRules asset turns it on (or `gstn.Respawn.Enabled 2`).

Owner rules: shared lives pool, **no revive** (GM.9); lives out = the team restarts from the last checkpoint or from the level start (GM.4);
balanced per level in data (20 lives and no checkpoints, or 4 fast-refilling lives with checkpoints).

## Modules
| Module | What |
|---|---|
| `GSTNRespawnCore` (pure) | `UGSTN_RespawnRules` (GameRules piece "Respawn and lives"), `FGSTN_LivesPool`, `FGSTN_CheckpointProgress`, placement choice, `Message.Respawn.*` tags. Tests `GSTNRespawn.Core.*`. |
| `GSTNRespawnRuntime` | `UGSTN_RespawnSubsystem` (server: KO listener on the authoritative event stream, timers, team restart), `AGSTN_Checkpoint`, `AGSTN_RespawnStateInfo` (replicated lives/checkpoint for UI), `IGSTN_PawnSpawnService`. |

Dependencies: GSTNCore, GSTNInput (event stream), GSTNCombatGAS (KO tags), GSTNCombatNP (KO zones, in-place respawn), NetworkPrediction.
Never GSTNMission: missions hear `Message.Respawn.*` on the stream.

## How it works
- KO = `Combat.Event.KO` published once by the host (never a re-simulation). A player KO spends `PerReason[reason].LifeCost` (default 1) and
  schedules the return after the delay; the credit partner's copy of the same KO is ignored. Bots and minions have no lives.
- Return: a **new pawn** through the game mode (`RestartPlayerAtTransform`, or the game mode's `IGSTN_PawnSpawnService`), at the active
  checkpoint's spawn points, else near a living teammate on safe ground, else the level start. `Method = InPlace` reuses the pawn (harness style).
- Checkpoint = first touch by any player (server position in its box, poll at `PollHz`), shared by the team, only forward by default.
- Pool empty: after `RestartDelaySeconds` everyone comes back at the checkpoint (or the start, `RestartLevel`, which also publishes
  `Message.Respawn.RestartLevel` so missions reset; `bReloadMapOnRestartLevel` reloads the map).
- Kit drop on death is **not here**: it is GSTNHeld's `KitSwap.DeathDrop` (same GameRules asset), already wired on the KO event.
- Works with a classic GameMode and with Experiences (decision D5): the rules come from the GameRules asset, the spawn from whoever owns the game mode.

## Tags (native)
`Message.Respawn.Down`, `.Respawned`, `.PoolChanged`, `.CheckpointActivated`, `.RestartFromCheckpoint`, `.RestartLevel` (server event stream only).

## Debug (GSTN Debug Options window lists them)
`gstn.Respawn.Enabled` (0 rules, 1 off, 2 on), `.Lives`, `.RegenSeconds`, `.DelaySeconds`, `.Checkpoints`, `.OnLivesExhausted`, `.Method`, `.Debug`.
Commands: `gstn.Respawn.Status`, `gstn.Respawn.AddLives N`, `gstn.Respawn.Restart`.

## Not yet (part 2)
Invulnerability after respawn (data only: needs a channel into the simulation), reinforcement call verb (Expedition), death presentation hook,
join in progress, tests in a running world (plan in `docs/notes/handovers/m12-parte1-cloud.md`).
