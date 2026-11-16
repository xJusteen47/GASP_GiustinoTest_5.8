# M10 slice 1: held objects end to end (desktop, 2026-11-10)
Branch `claude/desktop-m10-1` from 7487fa51, merged with `claude/laptop-weakpoint-3b` (f6bae51c). Plan: `m10-held.md` (PR #27, not merged). Doc: `Plugins/GSTNHeld/README.md`.

**M10 in slices** (plan gates in brackets; GSTNProjectile already covers G4 math and most of G7/G8):
1. Pick up, hold, throw, hit, land, networked, on GSTNProjectile (G5 core + G7 core). **Done here.**
2. Hold as a Hand-channel link: role tags, break rules, drop on hit per object, grabbed holder drops (G1 + G5 rest).
3. Interact probe candidates + prompt, input binding interact hold = pickup (G2; binding needs owner's kit Blueprints).
4. Detached credit, `Combat.Source.Thrown`, friendly fire by mode (G3 + G7 rest).
5. Aim policy (stick / camera while aiming), impact behaviours Break/Bounce/Stick, object ghost blend (G7/G8 rest).
6. Carry load and size classes (G6). 7. Thrown enemies (G9). 8. Debug, editor arc preview, sweep, bytes, perf (G12-G13).

**Done**
- New plugin `GSTNHeld` (Core pure, Runtime world), enabled in the .uproject, off by default. Object = replicated actor/component, 0 bytes at rest,
  every machine places it (at the holder + offset, at its projectile, at rest). Host resolves pickups per frame (`FGSTN_HeldRules::ResolvePickups`:
  earliest frame, lower entity id, nearest in radius, lower object id). Throw = `UGSTN_GA_HeldThrow`, a child of the projectile fire ability: shot
  template from the object, so sweep, PostHit, credit to the thrower, predicted ghost are GSTNProjectile's. Host lands the object on WorldStatic contact
  (removes the shot) or below where the shot ended. Dev: `gstn.Held.Spawn|Pickup|Throw|Report`.
- GSTNProjectile, neutral: `PrepareShot` / `OnShotFired` hooks on the fire ability, `RemoveProjectile`. Module rules for GSTNHeld in `check_modules.py` (0 new findings).
- Tests `GSTNHeld.Core.PickupRace`, `HoldTransformAndDefaults`. Harness `Tools/GASNPPTest/held_scenarios.py` (`SUITE_HELD` = `held_pickup_throw_hit`).

**Gate**: builds b1 (24 s, first try), b2 after merge (23 s). Headless h1 1476/0/0, h2 on merged tip `Saved/M10/headless-h2.log` 1477/0/0. Harness (`Saved/M10/run.ps1`,
reports `Saved/Logs/m10/`): r1 FAIL only on my check (third pawn in the line of fire took the hit; check fixed), r2 PASS 6/6 listen+2 and ded+2 at 0 % and 2 %,
r3 on merged tip PASS listen 2 % and ded 0 %, 0 ensures.

**Choices (test values, owner can overturn)**: hold = host table, not yet a link (slice 2); pickup and throw are host side effects (client sees the hold one RTT late,
the throw gets the projectile ghost); no gating tags yet; object never collides; drawn at root + offset, not a hand socket; all tuning TEST VALUES (radius 200,
throw 1500 cm/s, 15° pitch, gravity -980, damage 10, knockback 40, one hit).
**Owner questions**: button and feel of pickup/throw (plan: interact hold = pickup, any attack while holding = throw?), real damage and arcs, plan Q1-Q3 still open.

Cost: ~150k context, Opus, estimated ~$10-12. Builds 2, headless 2 (+1 surface rerun), harness runs 3 (~1 min per scenario).
