# Air momentum 1 (thread vbnr9g, cloud, 2026-11-18) — code only, NOT compiled

Owner ask (11-18 08:08/08:21Z, scope: air movement of all kits with a locomotion profile): external pushes in their own channel that a
neutral stick does not kill; less air inertia with a neutral stick, per kit; stronger air control.
Branch `claude/project-thread-vbnr9g` from 98dml5 `cb8163ab`.

## What changed
- Core `Movement/GSTN_AirMomentumTypes.h` + `GSTN_AirMomentumLogic.cpp`: pure two-channel model. Player channel = total − external:
  stick accelerates to the air top speed (TakeoffCap), stick against brakes, neutral decays at the style's rate. External channel: slow
  decay, braked only by the stick against it, the stick along it adds nothing past the cap. No vectoring (one scalar cap).
- Tuning per kit: `DA_Locomotion_* > Run Stages > Air > Momentum` (`FGSTN_RunAirTuning::Momentum`). Style `HighFriction` (Astro, DEFAULT),
  `Carry` (Mario), `MoverLegacy` (exactly the old falling mode). Defaults (cm/s², ×air control 0.85 for player accel/brake):
  AirAcceleration 3200 (stick along, dot 1), AirAccelerationSide 3200 (dot 0), AirBrake 4500 (dot -1), linear in the dot between, HighFrictionNeutralDeceleration 4000, CarryNeutralDeceleration 550 (old Mover value 200),
  ExternalDeceleration 300, ExternalBrake 1800, ExternalMinSpeed 20. New fields → existing assets get these defaults, no editor work.
- State: `FGSTN_RunStageState::ExternalVelocity` (sync state, rollback-safe, +12 bytes on the wire at 0.1 cm/s, reconcile tol 0.5).
- NP: `FGSTN_RunStageMover::ApplyAirMomentum` from the combat component's existing `ProcessGeneratedMovement` hook (after the air turn)
  replaces only the horizontal velocity, only in a UFallingMode frame with no velocity-override move. `PostMovement` stores the external
  channel (wall collisions take their share). External sources: any `FLayeredMove_Launch` child (trampoline) and the knockback move
  (launch, relaunch, balloon frames, wall/ceiling/ground bounces that relaunch). A player-owned override move (dodge, lunge) empties it.
  Knockback `DecayPerFrame` acts on the external channel. Movement lock (stats factor 0) = no steering.
- Drift DI (owner 08:55Z, "the air control during hitstun"): under a movement lock with a knockback move active the victim keeps its air
  control × `DriftDIControlScale` (default 1 = Smash Ultimate-like, UNVERIFIED; 0 = off). It steers the player channel only and adds to
  the knockback, never brakes it. Launch DI (`FGSTN_DIParams`, hit frame) and SDI (Defence, hitstop) are unchanged: no overlap.
- Tests: `GSTNCombat.Core.AirMomentum.*` (6 tests: styles, steer, external, collision clamp, wire).

## Not done / risks (desktop thread)
1. Build + `GSTNCombat.Core.AirMomentum` + full `GSTNCombat` run (Run-Timed.ps1). Nothing compiled here.
2. Kits WITHOUT a locomotion kit entry keep the old falling mode (knockback victims included). Check the spawned dummy and all test kits
   have the entry (owner rule: shared systems on all kits).
3. Dodge long jump etc.: after an override move ends in the air, the drift is the player channel → it now decays at 4000 with neutral
   stick (Astro). If the long jump feels cut, the move must hand its speed as external (follow-up) or the kit uses `Carry`.
4. Pink kit jump height ("bassissimo") is NOT touched here (jump values live with the desktop thread / qaz1xr kit scripts).
5. Drift DI needs the victim's MoveInput to reach Mover during hitstun: if the input producer zeroes it under the lock, no drift (check).
6. Netcode: verify with the PIE harness (knockback + trampoline, 2 clients) that ExternalVelocity does not cause corrections.

## Decisions applied (rule 6)
- Default style HighFriction (owner: "tenerne di meno, o almeno per kit"); Mario kept as `Carry`.
- Air control multiplies the new accelerations, so stage AirControlScale, the AirControl stat and move overrides keep working.
