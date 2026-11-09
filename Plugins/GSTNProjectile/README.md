# GSTNProjectile

Projectiles for the GSTN combat, **deterministic first** ([entita-dinamiche review](../../docs/notes/reviews/entita-dinamiche-2026-11-09.md)
§2, §3, §8; design background in [combat-m10-held-objects.md](../../docs/architecture/combat-m10-held-objects.md) §9-§10).
A projectile is spawned once with its parameters and every machine computes where it is from them; only the spawn and the
destroy cross the wire.

Status: **experimental (0.1), slice 3**, `EnabledByDefault: false`, enabled in `GASP_Giustino2.uproject`. Nothing else knows it.

## Modules

| Module | Type | What it is |
|---|---|---|
| `GSTNProjectileCore` | Runtime | Pure (engine + `GSTNCombatCore`). `FGSTN_ProjectileParams` (origin, direction, speed, gravity vector, lifetime frames, owner entity and team, hit shape, hit data, max hits), `FGSTN_BallisticSegment`, `FGSTN_BallisticMode` (the "ballistic" mode: `PositionAt`/`VelocityAt`/`CarrierAt` in closed form of the frame, `SweepStep`, `FindFirstContactFrame`, `SolveLaunchDirection` = the arc variant: flat or high arc through a target under any gravity vector, `Quantize`), the segment's wire format (`NetSerialize`: packed 1/100 units, ~20-35 bytes), `FGSTN_ProjectileGhostReconcile` (adopt / snap / no match, blend offset). Tests `GSTNProjectile.Core.*`. |
| `GSTNProjectileNP` | Runtime | World side (NP fork for the fixed tick, `GSTNCombatNP` for snapshot and inboxes). `AGSTN_Projectile` (segment replicated `COND_InitialOnly`, no movement replication, drawn at `PositionAt(last fixed frame)`), `UGSTN_ProjectileSubsystem` (host: `FireProjectile`, sweep at every non-resimulated fixed frame, `PostHit`), `UGSTN_GA_FireProjectile` (combat ability: template params, `FireDelayFrames`, muzzle, aim Facing / CommandAim / EventTarget, launch pitch, high arc; fires on the host only, spawn once) and its ServerOnly child `UGSTN_GA_FireProjectileServer` (AI, traps, harness), consoles `gstn.Projectile.Ability` (harness path) and `gstn.Projectile.Fire` (raw), cvar `gstn.Projectile.MaxActive` (64). Predicted ghost (slice 3): `SpawnGhost`, `ReconcileGhostFor`, cvars `gstn.Projectile.PredictGhost` (1), `GhostToleranceCm` (50), `GhostMatchFrames` (3), `GhostBlendFrames` (6), `GhostTimeoutFrames` (60); dev `gstn.Projectile.GhostFire`, `GhostReport`, `Burst`, `NetReport`. |

## How a hit happens

1. Host: `UGSTN_ProjectileSubsystem::FireProjectile(Params)` from `UGSTN_GA_FireProjectile` (on a client the fire is skipped; the host never resimulates, so it spawns once), `gstn.Projectile.Ability [Shooter] [Target] [Speed] [GravityZ] [HighArc] [LifetimeFrames]`, or raw `gstn.Projectile.Fire [Speed] [GravityZ] [LifetimeFrames] [Damage]`.
   The segment starts at the next fixed frame.
2. After the NP fixed tick of frame F, the host sweeps each projectile from F-1 to F against the combat step snapshot S(F): relationship
   and hittable list from the combat rules, swept hurtbox overlap with the opening rule, extra hurtboxes, `PickHurtboxIndex`.
3. A hit is a normal `FGSTN_HitInboxEntry` (AttackerId = owner) posted with `UGSTN_CombatWorldSubsystem::PostHit`, the call minion
   attacks use: the victim (fighter or minion) drains it at F+1 and runs the melee pipeline (block, parry, armour, hardness, wounds, parts).
4. One hit per target per projectile; consumed at `MaxHits` (1 by default) or after `LifetimeFrames`.

## Predicted ghost (slice 3)

The client that predicts `UGSTN_GA_FireProjectile` builds the same shot from its own snapshot and draws it at once as a local, never
replicated `AGSTN_Projectile` (`SpawnGhost`, once per owner and start frame, so a resimulation adds none). The host's projectile
replicates its Owner (the shooter) with the quantized segment; on its `BeginPlay` the firing client compares it with its ghosts of that
owner at the current frame: within `GhostToleranceCm` it is drawn at the ghost's place and the offset fades out over `GhostBlendFrames`
(adopt), beyond it the ghost is dropped and the projectile shows where it is (snap). A ghost without a host projectile goes after
`GhostTimeoutFrames`. Presentation only: the spawn and the hits stay the host's. The host quantizes its segment before using it, so it
sweeps exactly the bits the clients decode.

## Not yet (after slice 3)

World collision and bounces, projectile vs projectile and deflect, lag compensation (`PositionAt` of the
rewound frame), a timeline track key (the ability's `FireDelayFrames` stands in), a player input binding for the ability (content), pooling, VFX, minion ranged attacks, smoothing between fixed frames on the client.

Harness: `Tools/GASNPPTest/projectile_scenarios.py` (`SUITE_PROJECTILE`: ability hits and ghost adopt, dedicated server + 2 clients; `SUITE_PROJECTILE_BYTES`: wire bytes with ~20 alive, listen + 2).
