# M10 slice 6: carry load and size classes (G6), impact behaviours (part of G7) (desktop, 2026-11-10)
Branch `claude/desktop-m10-6` from `claude/desktop-m10-5` (d912e837). Brief: `m10-slice5.md`, plan `m10-held.md` (PR #27, commit 1d095223). Doc: `Plugins/GSTNHeld/README.md` steps 11-12.

**Done**
- G6: four size classes as data (`UGSTN_HeldSettings::SizeClasses`, `DefaultGSTNHeld.ini`), object `SizeTag` + `WeightKg` (default Light, 5 kg). The host writes a
  load payload on the Held link's Form (`FGSTN_HeldLoad`: class index 2 bits, weight/capacity 6 bits; capacity = `MaxCarryWeight`, else
  `DefaultCarryCapacityKg` 50). Holder role effect `UGSTN_GE_HoldLoad` (MultiplyCompound on WalkSpeed, JumpVelocity, AirJumpVelocity via 2 MMCs
  decoding the payload) applied/removed by GSTNLink's link tick on every machine. `Combat.Held.Carrying.<size>` projected from the payload.
  Command bans per class in the loadout extension (hierarchical; air-only via `Input.Context.Airborne`; `Command.Held.*` never). `bRequiresCapacity`
  (Huge) skipped by a pickup whose holder is too weak. `ThrowSpeedScale` per class in `PrepareShot`.
- GSTNLink (generic, tiny): `FGSTN_LinkCommandEntry::Payload` copied into the formed link state.
- G7 part: `EGSTN_HeldImpactBehaviour` Land/Break/Bounce/Stick (`FGSTN_HeldRules::ResolveImpact`, host). Break = new state `Broken` (hidden, no
  collision, no record) + respawn at home after `BreakRespawnSeconds`; Bounce = new projectile shot from the contact (same hit, thrower credit);
  Stick = rests at the contact. `gstn.Held.Spawn [Pawn] [Fwd] [Size] [Kg] [Impact]` (host-only test definition); `HeldHandReport` adds `size= walk=`.
- Fork fix (GASNPP AbilitySystemSimulation): `FAttributeSyncData::NetDeltaSerialize` sent sim proxies a "current = base" bit against a base they
  never receive (NetSerialize skips it): when a stat effect ended, proxies read the stat as 0 (seen as walk=0 after a drop). Proxy delta = current only.
- Tests: `GSTNHeld.Core.{LoadPayload, PickupCapacity, SizeBans, Impact}`, `GSTNHeld.Runtime.CarryLoad`. Harness: `held_carry_load`, `held_throw_break`.

**Choices (owner can overturn)**: all numbers TEST VALUES (Light -10 %, Medium -30 % no shield/air attacks, Heavy -50 % no jump = walk/drop/throw,
Huge -70 % + capacity; throw x1/0.85/0.7/0.5); load linear in weight/capacity; a rebound can hit a target the throw already hit (no per-throw ring);
Break also on a fighter hit. Unchanged as asked: re-pick delay 0.64 s, contested focus -> nearest free object.

**Gate**: builds b1 (missing include, fixed) b2-b5 ok, ~10-20 s each. Headless h2/h3 1495/0/0 (`Saved/M10f/headless-h3.log`), GSTNHeld 15/0.
Harness r1: held listen 2 % / dedicated 0 % 3/4 (carry_load: proxies walk 0 after the drop = the fork bug above); r3 after the fix 4/4 PASS
both, 0 ensures (`Saved/Logs/m10f/r3-held-*`); r4 grab suite listen 2 % 5/5 PASS. Not done: three-config build, PIE by eye, SUITE_DEFAULT/MOUNT reruns.

**M10 left**: G7 rest (aim while shoulder-aiming = the plan's AimPolicy; friendly fire by mode; per-throw hit ring; Explode), G8 (object ghost blend,
`scenario_throw_ghost/host`), G9 thrown enemies, G12-G13 (size-binding validator, debug/editor, sweep, three-config build).

Cost: ~150k context, Opus, estimated ~$10-12. Builds 5, headless 3, harness 4 (~3 + 1 + 6 + 3.5 min).
