# M10 slice 8: G7 shoulder aim and Explode, G8 object ghost (desktop, 2026-11-11)
Branch `claude/desktop-m10-8` from `claude/desktop-m10-7` (f9d5e0a3). Brief: `m10-slice7.md`, plan `combat-m10-held-objects.md` §8-§10. Doc: `Plugins/GSTNHeld/README.md` step 13.

**Done**
- G7 shoulder aim: `FGSTN_HeldThrowInput::AimView` (ini, both shipped). Shoulder (default) = held aim asks the camera for `AimViewRequestTags`
  (`Camera.Request.Aim`) via `UGSTN_PrecisionAimComponent::SetHoldAimShownWithView`/`GetViewRequestTags` (local only), release aim
  `EGSTN_HeldThrowAim::Shoulder` = converged command aim + `ShoulderAimPitchDeg`; KeepView = slice 5. Hook `UGSTN_GA_FireProjectile::GetShotLaunchPitchDeg`.
- G7 Explode: `EGSTN_HeldImpactBehaviour::Explode`; host fires a one-step blast shot (`ExplodeRadiusCm`, new `FGSTN_ProjectileParams::bRadialPush`),
  thrown source, thrower credit, then Broken/respawn. `bExplodeOnFighterHit`, `bExplodeOwnHit`/`ExplodeHit` settings. `gstn.Held.Spawn .. Explode`.
- G8 object ghost: thrower's client draws the object at the throw's ghost (`OnGhostSpawned` hook), then at the host projectile that adopted it
  (`UGSTN_ProjectileSubsystem::OnGhostReconciled`), until replication catches up; unmatched ghost = back in hand. `gstn.Held.GhostThrow`, `gstn.Held.GhostReport`.
- Harness: `held_throw_explode`, `held_throw_ghost` added to `SUITE_HELD`. Tests extended: `GSTNHeld.Core.ThrowAim`, impact rules (Explode).

**Gate**: build b1 ok (25 s). Headless h1 GSTN 1495/0, 0 ensures. Harness r1 (`Saved/Logs/m10h/`): held listen 2 % 6/6 PASS, held dedicated
0 % 6/6 PASS (ghost: 1 predicted, 1 handed, error 0.00 cm; explode blast=1), 0 ensures, 0 tracebacks.
Access violation at QUIT_EDITOR after the last batch (null address, no stack in the log); slice 7's r1 log has one too: pre-existing shutdown crash.
SUITE_DEFAULT (listen 0 %, run with the batch driver = each scenario in a fresh PIE): 35/50 PASS. Red: known hit_basic/hit_trade/hit_host, plus
combat_context, air_jump_fastfall, tilted_gravity_jump, dash_tilted_gravity, command_set, spike_stats, stagger_threshold, reaction_tiers, di,
ko_zone, dodge_blocked_in_hitstun, friendly_fire. None touches held/projectile code; likely the suite's order dependence (kits given by earlier
scenarios) under the per-PIE driver, NOT verified against a baseline. Next: rerun SUITE_DEFAULT with `gasnpp_test.run_suite` (one PIE, in order).

**Test values / choices for the owner**: AimView=Shoulder, ShoulderAimPitchDeg=0, ExplodeRadiusCm=250, bExplodeOnFighterHit=on, bExplodeOwnHit=off
(blast uses the throw's hit), the blast also hits the direct victim (direct hit + blast), reticle-tap stays a Camera aim (no shoulder).
Open: the shoulder view needs the pawn's `IGSTN_CameraTagSource` to add `GetViewRequestTags` (GSTNCamera is on no pawn yet); self-damage from a blast.

**Left in M10**: SUITE_DEFAULT baseline check; harness for unmatched ghost and pierce+rebound; G9 thrown enemies; G12-G13. Nobody has seen it in PIE.

Cost: ~140k context, Opus, estimated ~$8-10. Build 25 s, headless ~2 min, harness r1 ~38 min.
