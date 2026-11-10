# GSTNHeld

Held and thrown objects for the GSTN combat (milestone M10, plan [m10-held.md](../../docs/architecture/plans/m10-held.md) on the plan
branch, spec [combat-m10-held-objects.md](../../docs/architecture/combat-m10-held-objects.md)). Built on
[GSTNProjectile](../GSTNProjectile/README.md): a throw **is** a projectile shot, so the host sweep, the hit into the victim's inbox, the
credit to the thrower and the predicted ghost come from that plugin unchanged ("deterministic first",
[entita-dinamiche review](../../docs/notes/reviews/entita-dinamiche-2026-11-09.md) §2, §9 category (a)).

Status: **experimental (0.1), slice 1**, `EnabledByDefault: false`, enabled in `GASP_Giustino2.uproject`. Nothing else knows it.

## Modules

| Module | Type | What it is |
|---|---|---|
| `GSTNHeldCore` | Runtime | Pure (engine + `GSTNCombatCore` + `GSTNProjectileCore`). `EGSTN_HeldObjectState` (Idle, Held, Flying), `FGSTN_HeldObjectTuning` (pickup radius, hold offset, rest half height, the throw as `FGSTN_ProjectileParams`; every default a test value), `FGSTN_HeldRules::ResolvePickups` (the race: earliest frame, then lower holder id, nearest object in range, lower object id on a tie), `HoldTransform`. Tests `GSTNHeld.Core.*`. |
| `GSTNHeldRuntime` | Runtime | World side. `UGSTN_HeldObjectDefinition` (data asset with the tuning), `UGSTN_HeldObjectComponent` (replicated state, holder, flight, rest location; places its actor on every machine), `AGSTN_HeldObjectActor` (ready-made object, engine cube placeholder), `UGSTN_HeldSubsystem` (host: pickups, holder gone, flight landing), abilities `UGSTN_GA_HeldPickup` / `UGSTN_GA_HeldThrow` and their ServerOnly children, dev commands `gstn.Held.Spawn`, `Pickup`, `Throw`, `Report`. |

## How it works

1. **At rest** the object is a replicated actor with nothing dirty: 0 bytes. Movement replication is off; every machine places it.
2. **Pickup**: `UGSTN_GA_HeldPickup` asks the host (`RequestPickup`). After the fixed tick of that frame the host resolves every request of
   the frame with `ResolvePickups` against the holders' combat snapshot positions and sets the winner's object `Held`.
3. **Hold**: every machine draws the object at its holder with `HoldOffset` in the holder's actor space (the plan's spike S2 fallback,
   root + offset; a hand socket later). If the holder disappears the host drops the object to the floor.
4. **Throw**: `UGSTN_GA_HeldThrow` is a `UGSTN_GA_FireProjectile` whose shot template is the object's `Throw` tuning (hook `PrepareShot`).
   On the host the shot is fired and the object becomes `Flying` with it (hook `OnShotFired`); the throwing client draws the projectile
   ghost. Nothing held: no shot.
5. **Flight and landing**: the object is drawn at its projectile. The host traces each frame's step of the segment against WorldStatic: on
   contact it removes the shot (`UGSTN_ProjectileSubsystem::RemoveProjectile`) and lands the object there; when the shot ended (hit or
   lifetime) the object drops to the floor below its last position. Landed objects are `Idle` and can be picked up again.

## Not yet (later slices of the plan)

Hold as a Hand-channel link (G1: role tags, break rules, drop on hit per object, grabbed holder drops), interact probe and prompt (G2),
detached credit and `Combat.Source.Thrown` (G3), carry load and size classes (G6), aim policy, friendly fire by mode, impact behaviours
Break/Bounce/Stick, ghost blend of the object itself (the projectile's ghost is used), thrown enemies (G9), editor preview, a player
input binding (content: owner's kit Blueprints; plan: interact hold = pickup).

Harness: `Tools/GASNPPTest/held_scenarios.py` (`SUITE_HELD`: `held_pickup_throw_hit`).
