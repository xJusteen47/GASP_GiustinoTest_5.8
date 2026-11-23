# M14 implementation plan: Versus (PvP)

Plan only, no code (2026-11-06). Spec: [game-modes.md](../game-modes.md) §1 and §9.3 ("an Experience on the same frameworks: stocks, shield on,
authored arenas"); camera duel framing and arena bounds already in `GSTNCamera` ([camera.md](../camera.md)); M6 defence (shield, parry,
tech) is done. Ground rules: [resto-m3-m4b-m5-t.md](resto-m3-m4b-m5-t.md) §0. M14 is mostly data on M12: small on purpose.

## 1. Gates

| Gate | Content | Machine / model | Cost | Needs |
|---|---|---|---|---|
| **G0** | Audit of what M12 gives a Versus Experience; list the missing rule fields | cloud, Sonnet | $2-3 | M12 |
| **G1** | `GSTNMode_Versus` (Game Feature): stocks per player in `GSTNRespawn` rules, shield bound (mode toggle), teams and friendly fire per mode (M10 Q7), KO by blast zone, kit drop on death on | laptop, Sonnet | $6-10 | G0 |
| **G2** | Arena: blast-zone volume, duel camera on the arena bounds, one authored arena assembled at editor startup (owner saves once); stage hazards none | desktop, Sonnet | $5-8 | G1 |
| **G3** | Match flow: lobby and kit select (placeholder UMG), countdown, results, rematch; seamless travel | desktop, Sonnet | $6-10 | G1 |
| **G4** | Fairness: host advantage measured (host vs remote hit trade, 60/15 ms, 2 %), optional input delay CVar for the host, lag compensation window check | laptop, Opus | $6-10 | G2 |
| **G5** | Sweep: 1v1 and 2v2 with 4 players listen, 2 % loss; docs | laptop, Haiku/Sonnet | $3-5 | G3, G4 |

**Total: $28-46.**

## 2. Blueprint and C++

C++ only for the stock arithmetic, blast-zone KO and the host input-delay option. Everything else is data and Blueprint: Versus
Experience, rules asset, arena actors, lobby/results widgets.

## 3. Stop and ask

G4 shows the host wins trades by more than one frame on average at 60/15 ms → stop and ask (input delay on by default, or accept the host
advantage as in M7 §2c).
