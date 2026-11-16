# M13 implementation plan: Expedition mode (low priority)

Plan only, no code (2026-11-06). Spec: [game-modes.md](../game-modes.md) §3 (procedural generation on a listen server), §6 (flow,
reinforcement, extraction, drops), §7 (spawn director and budgets), §9.1 spikes G1-G3, G6. Owner: GM.1 "Levels first, Expedition low
priority"; GM.9 shared reinforcements as lives, no revive. Target hardware is low-end (sequenza D4): every budget of §7.3 is re-read on the
capped-frame profile. Ground rules: [resto-m3-m4b-m5-t.md](resto-m3-m4b-m5-t.md) §0. Starts only on the owner's go (decision D9).

## 1. Needs

M12 merged (Experiences, verbs, respawn, missions), M8 (minions, waves; platforms optional), M10 (retrieve objective), M11 G2/G6
(pickups and backpacks for drops).

## 2. Gates

| Gate | Content | Machine / model | Cost | Needs |
|---|---|---|---|---|
| **G0** | Spikes: G1 solver determinism (10 000 seeds, editor + a Linux job, ≤ 8 KB, ≤ 1.5 s), G2 module instancing host + 2 clients (level instance vs packed actor, collision digest equal, ≤ 4 ms/frame), G3 runtime navigation across module seams | laptop + desktop, Opus | $10-16 | M12 |
| **G1** | `GSTNProcGenCore` (pure): module sockets, slot plan, traversability validator (wall slides, trampolines, gravity), seed rejection with an attempt cap, determinism goldens | laptop, Opus | $10-18 | G0 |
| **G2** | `GSTNProcGen` builder: streaming, loading flow with a loading screen, late join (spike G6: joiner ready ≤ 20 s) | desktop, Opus | $10-18 | G1 |
| **G3** | `GSTNSpawnDirector` + `GSTNSpawnDirectorMinion`: pressure, cost budget, caps on the low-end profile | laptop, Opus | $8-14 | G2, M8 G12 |
| **G4** | `GSTNMode_Expedition`: briefing and kit choice, drop-pod arrival, mission pool, reinforcement verb, extraction with `Defend`, drop tables (spawn the M11 pickups) | desktop, Sonnet | $8-14 | G3, M11 |
| **G5** | Module kit: hand-authored modules as Blueprint level instances, module validator, layout viewer in `GSTNProcGenEditor` | desktop, Sonnet | $8-14 | G1 |
| **G6** | Sweep: `scenario_layout_replicate`, `scenario_director_budget`, late join; host bench on the low-end profile (60 awake minions + generation); docs | laptop, Sonnet | $5-8 | G4, G5 |

**Total: $59-102.**

## 3. Blueprint and C++

C++: solver, builder, director, reinforcement and extraction rules. Blueprint/data: modules (level instances), drop tables, expedition
definition (difficulty rows, mission pool, director rules), drop-pod actor, extraction console; one example module set built by the
startup script.

## 4. Stop and ask

G0-G2 fails (digests differ, > 4 ms per module) → stop and propose the fallback of game-modes §10 risk 3: a fixed set of pre-assembled chunks
chosen by seed (cheaper, about $20-30 for G1-G2 instead of $20-36). Host bench over budget on the low-end profile → lower director caps first.
