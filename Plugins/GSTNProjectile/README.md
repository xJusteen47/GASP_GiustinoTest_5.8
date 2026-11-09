# GSTNProjectile

Projectiles for the GSTN combat, **deterministic first** ([entita-dinamiche review](../../docs/notes/reviews/entita-dinamiche-2026-11-09.md)
§2, §3, §8; design background in [combat-m10-held-objects.md](../../docs/architecture/combat-m10-held-objects.md) §9-§10).
A projectile is spawned once with its parameters and every machine computes where it is from them; only the spawn and the
destroy cross the wire.

Status: **experimental (0.1), slice 1**, `EnabledByDefault: false`, enabled in `GASP_Giustino2.uproject`. Nothing else knows it.

## Modules

| Module | Type | What it is |
|---|---|---|
| `GSTNProjectileCore` | Runtime | Pure (engine + `GSTNCombatCore`). `FGSTN_ProjectileParams` (origin, direction, speed, gravity vector, lifetime frames, owner entity and team, hit shape, hit data, max hits), `FGSTN_BallisticSegment`, `FGSTN_BallisticMode` (the "ballistic" mode: `PositionAt`/`VelocityAt`/`CarrierAt` in closed form of the frame, `SweepStep`, `FindFirstContactFrame`). Tests `GSTNProjectile.Core.*`. |
| `GSTNProjectileNP` | Runtime | World side (NP fork for the fixed tick, `GSTNCombatNP` for snapshot and inboxes). `AGSTN_Projectile` (segment replicated `COND_InitialOnly`, no movement replication, drawn at `PositionAt(last fixed frame)`), `UGSTN_ProjectileSubsystem` (host: `FireProjectile`, sweep at every non-resimulated fixed frame, `PostHit`), console `gstn.Projectile.Fire`, cvar `gstn.Projectile.MaxActive` (64). |

## How a hit happens

1. Host: `UGSTN_ProjectileSubsystem::FireProjectile(Params)` (an ability on the host, or `gstn.Projectile.Fire [Speed] [GravityZ] [LifetimeFrames] [Damage]`).
   The segment starts at the next fixed frame.
2. After the NP fixed tick of frame F, the host sweeps each projectile from F-1 to F against the combat step snapshot S(F): relationship
   and hittable list from the combat rules, swept hurtbox overlap with the opening rule, extra hurtboxes, `PickHurtboxIndex`.
3. A hit is a normal `FGSTN_HitInboxEntry` (AttackerId = owner) posted with `UGSTN_CombatWorldSubsystem::PostHit`, the call minion
   attacks use: the victim (fighter or minion) drains it at F+1 and runs the melee pipeline (block, parry, armour, hardness, wounds, parts).
4. One hit per target per projectile; consumed at `MaxHits` (1 by default) or after `LifetimeFrames`.

## Not in slice 1

Predicted ghost on the shooter, world collision and bounces, projectile vs projectile and deflect, lag compensation (`PositionAt` of the
rewound frame), a timeline track key, a sample GAS ability (the API is `FireProjectile`), pooling, VFX, minion ranged attacks, a compact
wire format (the actor replicates the segment as full doubles), smoothing between fixed frames on the client.
