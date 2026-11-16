# Projectile slice 2: fire ability, arc aim, world path verified (desktop, 2026-11-09)

Branch `claude/desktop-projectile-2` from merge tip 642f5165 (slice 1). Plugin doc: `Plugins/GSTNProjectile/README.md`.

**Done**
- `UGSTN_GA_FireProjectile` (GSTNProjectileNP, child of `UGSTN_GA_Base`): template `FGSTN_ProjectileParams`, `FireDelayFrames` (combat
  frames, sync wait of the base), muzzle in the owner's snapshot space, aim Facing / CommandAim / EventTarget, `LaunchPitchDeg`, `bHighArc`.
  Fires through `FireProjectile` on the host only (`CanFire()` false on a client: skipped), spawn once (the host never resimulates). The
  event target is kept as its entity id in a sync int; origin and target come from the combat snapshot, not from Mover queries.
- `UGSTN_GA_FireProjectileServer`: same, ServerOnly policy (AI, traps, dev command, harness: no client desync on a host-only activation).
- Arc variant: `FGSTN_BallisticMode::SolveLaunchDirection` (flat or high arc through a target, any gravity vector, false when out of reach;
  the ability then throws at 45 degrees). Test `GSTNProjectile.Core.ArcAimReachesTarget` (goldens tan 0.5 / 2, sideways gravity, out of range).
- Subsystem: `CanFire()`, `FindEntityIdOf(Actor)`; console `gstn.Projectile.Ability [Shooter] [Target] [Speed] [GravityZ] [HighArc] [Lifetime]`
  (grants the ServerOnly ability, triggers it with the target pawn as event target). `gstn.Projectile.Fire` stays as the raw path.
- Harness `Tools/GASNPPTest/projectile_scenarios.py` (`SUITE_PROJECTILE` = `projectile_ability_hits`): straight, flat arc, lob from the host
  at the other pawn; checks activations, fired/hit log, one inbox row per shot (attacker = shooter, damage > 0), damage up, victim reaction
  (hitstop/hitstun), clients draw the lob, damage converged on server and both clients.

**Gate**: build `Saved/PJ2/build-b1.out` 9 s first try, `build-b2.out` after the fix; headless `Saved/PJ2/headless-h1.log` 1465/0/0 ensures
h2 on b2 1465/0/0. Harness ded + 2 (driver `Saved/PJ2/run.ps1`): r1 FAIL (lob expired at 120 frames, flight ~153: first two
shots went fire -> sweep -> PostHit -> drain -> reaction fine), r2 PASS 7/7 at 0 % and at 2 % loss (`Saved/PJ2/r2.log`).

**Choices made**: lifetime is never stretched to the solved flight (template wins; the dev command takes it as an argument); EventTarget
is for AI/harness only, never to steer a player's shot (owner: no auto-play); no player input binding yet (content, owner's Blueprints).
**Not done**: predicted ghost on the shooter (client sees its own shot one RTT late), timeline key (FireDelayFrames stands in), world
collision, minion ranged move, lag compensation of the sweep.

Cost: ~95k context, Opus, estimated ~$5. Builds 2 (~10 s each), headless 2 (~3 min), harness runs 2 (~1 min each).
