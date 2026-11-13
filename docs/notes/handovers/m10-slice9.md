# M10 slice 9: SUITE_DEFAULT in one PIE, G9 thrown enemies (desktop, 2026-11-11)
Branch `claude/desktop-m10-9` from `claude/desktop-m10-8` (3cb19a4f). Brief: `m10-slice8.md`, spec `combat-m10-held-objects.md` §6b. Doc: `Plugins/GSTNHeld/README.md` step 14.

**SUITE_DEFAULT** with `run_suite` (one PIE, listen + 2, in order) on 3cb19a4f: **50/50 PASS**, known reds included. Slice 8's 35/50 was the per-PIE
batch driver (kits left by earlier scenarios), not a regression: no baseline run needed. Fixed `m3_suite_driver.py`: without `Saved/R31` the report
write raised every tick and the editor never quit (verdicts were read from the log: `Saved/M10i/suite-tip-verdicts.txt`).

**G9 done (fighters)**: grabbing an enemy you can lift projects `Combat.Held.CarryingEnemy` (held tick extension, `UGSTN_HeldSubsystem::FindLiftedEnemy`,
static data of the two fighters). RT = `Command.Held.ThrowEnemy` (slot 111, above the grab's kit throws at 100), same `UGSTN_GA_HeldThrow`, same tap
(stick) / hold (reticle + camera) aim. Shot = `ThrownEnemies.Throw`, source `Combat.Source.Thrown.Enemy` (child of Thrown), credit to the thrower, the
body seeded as already hit (MaxHits + 1). Host `NoteEnemyThrown`: LinkCommand Break (Throw) to both sides, the enemy leaves the parked grab with the
shot's velocity (Teleport exit) and `RegrabLockFrames` immunity. B = the grab's own release (the put-down). Not liftable: RT keeps the kit throws (M11).

**Gate**: builds b1-b3 ok (~20 s each). Headless GSTN 1497/0 (+2: `GSTNHeld.Core.EnemyLift`, `EnemyThrowSlot`), 0 ensures. Harness r1 (`Saved/Logs/m10i/`):
held listen 2 % 7/7 PASS, dedicated 0 % 7/7 PASS (new `held_throw_enemy` 10 checks: throw posted on both sides, grab gone everywhere, B out of the
parked mode and > 1 m away, no self-damage), 0 ensures, 0 tracebacks. Fix 1 (of 2): fighters' `Weight` is in knockback units (100 = standard), not kg.

**Choices for the owner (all TEST VALUES, `DefaultGSTNHeld.ini` `ThrownEnemies`)**: LiftRule both shipped: `WeightVsCapacity` (default; enemy `Weight`,
else `DefaultEnemyWeight` 100, vs grabber `MaxCarryWeight`, else `DefaultEnemyCarryCapacity` 100 = a standard fighter lifts a standard fighter) or
`AnyGrabbed`; `Combat.Held.NoLift` on an enemy forbids it in both; `bEnabled` off = never. Liftable types: the grab only. Hitbox 12 m/s, sphere 45 cm,
14 dmg, 3 targets; speed x0.75 at full load; regrab lock 60 frames. Source tag is the child `Combat.Source.Thrown.Enemy` (spec said a second tag).

**Throw timing today (for the wind-up request, next slice)**: the held throw is instant on the button's release (`UGSTN_GA_HeldThrow::OnSimulationTick`
calls `Fire()` the frame the button is up, or at `MaxAimFrames`); no montage, no release frame. A wind-up hooks there: store a sync release clock
(press/release + `WindupFrames` from the object or settings) and call `Fire()` when the clock is reached, playing the throw montage from the release; a
hit during the wind-up already cancels via `EndAbility`. The enemy throw shares the same path.

**Left**: minions are not grabbable, so G9 covers fighters only; the body flies on its own movement and the hitbox on the analytic shot (same start
and velocity, gravity -980 both; may drift); no carry load or thrown state on the body while lifted/flying; RT not tested with real input (harness has
no Strong button, it uses `gstn.Held.Throw`). G12-G13 not started. Nobody has seen it in PIE.

Cost: ~150k context, Opus, estimated ~$9-12 (desktop went offline once mid-slice). Build ~20 s, headless ~80 s, suite ~12 min, harness r1 ~9 min.
