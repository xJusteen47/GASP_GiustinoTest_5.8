# Weak point precision, code slice 2 (desktop, 2026-11-09)

Branch `claude/desktop-weakpoint-2` from merge tip 8cd88880. Plan: `docs/architecture/plans/precisione-weakpoint-2026-11-09.md` §5 (item 5 of §7), partial.
Time box: one hour on the desktop (owner needs it at 18:00 Rome), so only the deterministic pieces with tests landed.

**Done**
- Input: `FGSTN_InputCmd::AimStickX/Y` (right stick, +Y up, 127 = 1; one bit while centred, 2 bytes when pushed, kept by a repeated
  input). `UGSTN_InputConfig::AimStickAction` (soft, resolved with the others, `GetAimStickAction()`), sampled by the player source.
  Unset = always centred, so nothing changes until an asset sets it. Gyro = another device on the same action (plan §5), not done.
- Data: `FGSTN_CombatTimelineData::MaxSteerDegrees` (default 20, 0..90, every aim policy; 0 = no steering).
- Math: `FGSTN_StartupSteer::SteerYaw(StartYaw, AimStick, CameraYaw, MaxSteer)` in GSTNCombatCore: clamp against the start yaw
  (not accumulated, so a dropped or resimulated frame cannot drift), dead zone 0.25.
- Tests: `GSTNCombat.Core.StartupSteer.Goldens` (dead zone, clamp both ways, camera-relative, ±180 seam, idempotent);
  `GSTNInput.NP.Command.CameraAim` extended (aim stick round trip, bit cost, repeated input).

**Not done: next weakpoint thread (weakpoint-code-3), right after this one, same order** (still part of the approved plan; the owner's
"aspettiamo" of 15:51Z was about the UE 5.9 preview, not about these)
1. Sim: keep the move's start yaw in the combat sync state (quantized, 16 bits) when a timeline starts; every startup frame (before the
   first active hitbox, `FGSTN_TimelinePhases`) compute `SteerYaw` from the frame's cmd and turn the pawn through Mover. Nothing in the
   sim turns the pawn today (`EGSTN_RequestType::SetFacing` exists but has no producer or executor, and `AimPolicy` is still data only),
   so the Mover piece is new: a layered move or an executor for SetFacing that sets the orientation, rollback-safe, plus a harness check.
   Estimate $3-5. Do AimPolicy's own turn in the same step (same path).
2. Content: an `IA_AimStick` (Axis2D, right stick) on the input config asset + mapping context (owner or editor MCP; shared asset).
3. Plan items 6 (reticle on right-stick click + aim arrow) and per-mode flag: untouched.

**hit_basic / hit_trade / hit_host red (10-min check)**: environment, not code. In the "control" phase the victim is not placed, so it
stays where it spawned; on `DefaultLevel_Harness` on this machine the victim (entity 3) and another combat entity (9) sit inside the
attacker's reach, so the control attack hits both (`Saved/WP1/rbase.log` l.3676) and every later check counts the extra hit.
Fix belongs to the map's player starts / the scenario's control placement; not done.

**Gate**: build `Saved/WP2/build-b1.out` first try (~1 min). Headless `Saved/WP2/headless-h1.log`: 1471 / 0 fail / 0 ensures (+1).
Harness listen+2, 60/15, DefaultLevel_Harness, `Saved/WP2/r1.log`: 0 % command_set, camera_aim, combat_main_inputs, ricochet PASS;
kit_without_dodge FAIL identical to base dbda3046 (pawn spawns Falling at (-800,0,90), the dash never moves: map floor). 2 %: camera_aim,
combat_main_inputs, ricochet, hit_lagcomp PASS.

Cost: ~60k context, Opus, estimated ~$3-4. Builds 1, headless 1 (~1 min), harness 1 run (~5 min).
