# Projectile slice 3: predicted ghost, compact spawn, bytes at scale (desktop, 2026-11-09)

Branch `claude/desktop-projectile-3` from merge tip d9253d56 (slice 2). Plugin doc: `Plugins/GSTNProjectile/README.md` (section "Predicted ghost").

**Done**
- Ghost: the client that predicts `UGSTN_GA_FireProjectile` builds the same shot and draws it at once (`SpawnGhost`, local actor, once per owner +
  start frame so a resim adds none). The host projectile now replicates its Owner (the shooter); on its client `BeginPlay`,
  `ReconcileGhostFor` adopts (error <= `gstn.Projectile.GhostToleranceCm` 50: drawn at the ghost's place, offset fades over `GhostBlendFrames` 6)
  or snaps; unmatched ghosts go after `GhostTimeoutFrames` 60. Toggle `gstn.Projectile.PredictGhost` (1). Pure rule `FGSTN_ProjectileGhostReconcile` in Core.
- Wire: `FGSTN_BallisticSegment::NetSerialize` (packed 1/100 units, step in ms; test payload < 40 B vs 84 raw) and `FGSTN_BallisticMode::Quantize`;
  the host quantizes before use, so host and clients sweep/draw the same bits. Lifetime uint16, visual radius uint8, all `COND_InitialOnly`.
- Fix found by the harness: the actor updated at 1 Hz, so a lost spawn bunch was resent ~1 s later (ghost expired at 2 % loss) and 1 s projectiles
  often never reached clients. Now `NetUpdateFrequency` 30 + `ForceNetUpdate` at spawn (nothing is dirty after the spawn: compares, no bytes).
- Dev: `gstn.Projectile.GhostFire` (client world: ghost + server fire at the ghost's frame, stand-in for a predicted activation),
  `GhostReport`, `Burst`, `NetReport`. Test `GSTNProjectile.Core.GhostReconcile`. Harness `projectile_ghost_adopt` (in `SUITE_PROJECTILE`), `projectile_bytes` (`SUITE_PROJECTILE_BYTES`).

**Gate**: builds `Saved/PJ3/build-b1.out` (10 s, first try), `build-b2.out` (after the net-rate fix). Harness r1: ded+2 0 % 10/10, 2 % ghost FAIL
(1 expired, the 1 Hz bug); r2 (`Saved/PJ3/r2.log`, reports `Saved/Logs/pj3/r2-*.log`): ded+2 0 % and 2 %: ability_hits 7/7, ghost_adopt 3/3
(3/3 adopted, same start frame, error <= 0.01 cm, arrival 3-6 frames after the ghost); listen+2 bytes PASS. Headless `Saved/PJ3/headless-h1.log` on b2: 1466 success / 0 fail / 0 ensures (+1; payload 26 B).

**Bytes (listen+2, 60/15 ms)**: every client received all 300 projectiles, once each. Baseline (2 idle pawns, NP) ~25.9 KB/s per client and it
drifts ~±3 KB/s between windows, more than the projectiles cost. Read from the two load windows: 20 shots/s (1 s life) 23.2 KB/s vs 10 shots/s
(2 s life) 22.4 KB/s, both ~20-40 alive: ~0.8 KB/s per 10 spawns/s, so **~80 B per projectile per client, paid once** (channel open + close);
~20 alive with 1 s life ~1.6 KB/s per client, with 2 s life ~0.8 KB/s. Cost follows the spawn rate, not the number alive.

**Not verified**: a real predicted activation from player input (needs a command binding in content, owner's Blueprints); the ghost was driven by
GhostFire. Not seen in PIE with eyes (owner TODO: PIE with 100-250 ms lag). A snap is only covered by the headless test.
**Choices made**: tolerance 50 cm, blend 6 frames, timeout 60 frames (~1 s, covers RTT up to ~500 ms with loss); 1/100 cm quantization.

Cost: ~110k context, Opus, estimated ~$5-6. Builds 2 (~10 s each), harness runs 2 (~5 min each), headless 1 (55 s).
