# Projectile slice 1: GSTNProjectile skeleton (desktop, 2026-11-09)

Branch `claude/desktop-projectile-1` from merge tip 34ee78cd. Owner OK: "single module GSTNProjectile before M10 = YES".
Design: `docs/notes/reviews/entita-dinamiche-2026-11-09.md` §2, §3, §8. Plugin doc: `Plugins/GSTNProjectile/README.md`.

**Done**
- New plugin `GSTNProjectile` (enabled in the .uproject, `EnabledByDefault` false), two modules:
  - `GSTNProjectileCore` (pure, engine + GSTNCombatCore): `FGSTN_ProjectileParams` (origin, direction, speed, gravity vector, lifetime frames,
    owner entity/team, hit shape, hit data, max hits), `FGSTN_BallisticSegment`, `FGSTN_BallisticMode` (the "ballistic" mode: closed-form
    `PositionAt`/`VelocityAt`/`CarrierAt` of the frame, `SweepStep` = the melee `SweptOverlap`, `FindFirstContactFrame`).
  - `GSTNProjectileNP`: `AGSTN_Projectile` (segment replicated `COND_InitialOnly`, no movement replication, drawn at `PositionAt` of the last
    NP fixed frame; a client draws at its own predicted frame), `UGSTN_ProjectileSubsystem` (host: `FireProjectile`, sweep at every
    non-resimulated `OnPostFixedTick` against the combat snapshot S(F), relationship/hittable/opening/extra hurtboxes/pick as melee, then
    `UGSTN_CombatWorldSubsystem::PostHit` with AttackerId = owner: the minion attackers' path, so block/parry/armour/hardness/wounds/parts
    run unchanged on fighters and minions), `gstn.Projectile.Fire [Speed] [GravityZ] [LifetimeFrames] [Damage]`, cvar `gstn.Projectile.MaxActive` 64.
- Tests `GSTNProjectile.Core.StraightPathHitsHurtboxAtExpectedFrame` (zero gravity, 16 cm/frame, hit at start+30, also at start 1000; miss
  off-line; lifetime cut; thin hurtbox not jumped over at frame 31) and `GSTNProjectile.Core.GravityClosedForm` (goldens + bit-identical recompute).
- `Tools/Modules/check_modules.py`: forbidden edges for both modules (no projectile findings; its 21 pre-existing GSTNPartsMinion findings unchanged).

**Gate**: build `Saved/PJ1/build-b1.out` Succeeded (9 s, first try); headless `Saved/PJ1/headless-h1.log` 1464 success / 0 fail / 0 ensures (+2).

**Not verified**: the world path (fire, host sweep, PostHit, victim reaction, client draw) was not run in PIE; no harness scenario yet.
**Choices made**: console command instead of a sample GA (the ability API is `FireProjectile`, host only); owner never hit by its projectile;
no attacker hitstop (combat-m10 §10); segment replicated as full doubles (not compacted).
**Next**: PIE check with `gstn.Projectile.Fire` against a dummy (ded + 2), then a harness scenario; ghost on the shooter; world collision; timeline key; minion ranged move.

Cost: ~90k context, Opus, estimated ~$4. Builds 1 (9 s), headless runs 1 (~10 min).
