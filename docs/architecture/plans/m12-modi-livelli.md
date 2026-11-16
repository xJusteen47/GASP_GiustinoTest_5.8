# M12 implementation plan: mode framework + Levels mode

Plan only, no code (2026-11-06). Spec: [game-modes.md](../game-modes.md) §2, §4, §5, §9.3; [interaction.md](../interaction.md);
[camera.md](../camera.md); modularity-and-tags.md §3 and §6 (S-J, S-K). Owner answers used: GM.1 (Levels first), GM.4 (lives exhausted →
restart from checkpoint or level start), GM.6 (camera far, Mario 3D World, sections set angle), GM.9 (shared lives, no revive), sequenza
D1 (kit drop on death is a mode rule). Ground rules: [resto-m3-m4b-m5-t.md](resto-m3-m4b-m5-t.md) §0.

## 1. What is already there

- Seam D-1 (timeline event key → sim outbox) exists (`GSTN_CombatTimelineLogic.h`); `UGSTN_CombatSimComponent::ServerRespawn()` exists;
  M4 KO event and KO zones; M6 defence rules.
- `GSTNInteract` and `GSTNCamera` are experimental 0.1, disabled by default; the camera never ran against a real pawn (spikes C1/C4 open).
- No Game Features / Modular Gameplay plugins enabled in the `.uproject`; no Experience asset; D-2 (outbox → `Message.*` republisher) missing.
- TH-1b `DropKit` hook (M11 G2 wires it behind a flag).

## 2. Plugin layout (game-modes §2.1)

`GSTNExperience` (definition, pawn data, action sets; Lyra-style), `GSTNModeVerbs` (ability → verb → server confirm, D-2 republisher),
`GSTNRespawn` (lives pool, checkpoints, `KOReason` policy, drop-on-death rule), `GSTNMission` (objectives, Core pure + Runtime),
`GSTNMode_Levels` (Game Feature: level set, checkpoint/goal/collectible actors, score rules). Each mode is a Game Feature that an
Experience turns on; disabling one leaves the others buildable.

## 3. Gates

| Gate | Content | Machine / model | Cost | Needs |
|---|---|---|---|---|
| **G0** | Spikes: game-modes G4 (no pawn NP tick before the Experience is Active; GameState component action on host and clients) and G5 (verb seam under forced rollbacks and 2 % loss: exactly one authoritative event per confirmed verb) | laptop, Opus | $6-10 | M11 G2 |
| **G1** | Game Features + Experiences adoption (decision D5): enable `GameFeatures`/`ModularGameplay`, `GSTNExperience` (definition, pawn data, action sets), the test hub becomes an Experience; `.uproject` and ini edits announced before landing | desktop, Opus | $10-18 | G0 |
| **G2** | `GSTNModeVerbs`: D-2 republisher, verb abilities (`Ability.Mode.*`), predicted hold with server refusal and one cancel | laptop, Opus | $6-10 | G1 |
| **G3** | `GSTNRespawn`: `UGSTN_RespawnRules` (`LivesPoolSize`, `LifeRegenSeconds`, `bUseCheckpoints`, `OnLivesExhausted`, delay, invulnerability, `bDropKitOnDeath`, `bKeepBackpackOnDeath`), KO by health and by zone, respawn on a fresh NP entity, team restart | laptop, Opus | $10-18 | G2 |
| **G4** | `GSTNMission`: objectives Reach, Eliminate, Collect, Activate, Defend (timer); replication outside rollback; chains and failure; Core tests incl. "10 kills with forced rollbacks count exactly 10" | laptop, Opus | $10-18 | G2 |
| **G5** | `GSTNInteract` and `GSTNCamera` on a real pawn: interact focus + placeholder prompt, camera spikes C1/C4, far follow camera, section volumes (angle, lock), over-the-shoulder aim stage | desktop, Opus | $10-16 | G1 |
| **G6** | `GSTNMode_Levels`: level set, checkpoint (team shared, first touch), goal zone (`Reach`), collectibles, score rules, results screen (placeholder UMG), seamless travel to the next level | desktop, Sonnet | $8-14 | G3, G4, G5 |
| **G7** | Example content: `L_Test_Level01` (start, two checkpoints, a KO zone, enemies from M8 spawners, a ground kit, goal) assembled at editor startup / Play by script; owner saves it once | desktop, Sonnet | $5-8 | G6 |
| **G8** | Sweep: `scenario_experience_order`, `scenario_verb_confirm`, `scenario_respawn_checkpoint`, `scenario_mission_replicate`, `scenario_mode_toggles`; listen + dedicated; docs | laptop, Sonnet | $5-8 | G7 |

**Total: $70-120.** Order: G0 → G1 → (G2, G5 in parallel on two machines) → (G3, G4) → G6 → G7 → G8.

## 4. Blueprint and C++

- **C++**: Experience loading order, verb seam, respawn and lives arithmetic, mission state and replication, interact probe, camera
  evaluation.
- **Blueprint / data (designer)**: Experience and pawn data assets, respawn rules and mission definitions as data assets, checkpoint, goal,
  KO zone, collectible and camera section actors as Blueprint children tuned in the details panel; the results screen and the interact
  prompt as placeholder UMG widgets; one example verb ability in Blueprint (activate a console).

## 5. Stop and ask

G0: a pawn ticks before Active and cannot be held back without a fork change → stop. G1 is the only gate that edits `.uproject`: announce
it. G5: camera spike C1 shows a resimulation visible inside one frame → stop, report before building sections.

## 6. Defaults applied (all data)

Lives pool 4, refill 20 s, checkpoints on, restart from checkpoint; respawn delay 2 s, 60 invulnerable frames; kit drop on death **on**
in Levels and Versus (owner rule; the fallen player respawns with their lobby kit), dropped kit stays until picked up; goal = all alive players inside; collectibles per level, no save.
