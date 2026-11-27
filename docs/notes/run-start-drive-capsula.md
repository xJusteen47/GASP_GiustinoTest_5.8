# Run start driven by the simulation (greybox capsule)

Owner 2026-11-24 23:05 Rome: "Mi piace, salviamoci l'idea e implementiamola sulla capsula". Idea from the consulting thread (message of
23:03 UTC); scope: run start, run stage steps and the boost dash of every playable character, tried first on the greybox capsule.

## The idea

The animation does not run on its own clock. It reads three numbers from the simulation and lets them drive it:

1. **Stamp progress**: the sim's start stamp (FGSTN_RunStartStampTuning, `UGSTN_RunPresentationComponent::GetRunStartTime` /
   `IsRunStartStamp`).
2. **Speed**, relative to the stage 1 speed.
3. **Effort** = forward acceleration, normalised: how fast the speed is still climbing. This is what ties the animation to the shape of the
   speed curve.

Rule: **speed = cadence × stride length**.
- The **cadence** (steps per second) is chosen: high and almost fixed in the stamp, then easing down to the run cadence. Estimates: a
  Toad-style scramble is ~6-7 steps/s, the measured "Ganon" run ~2.7 steps/s.
- The **stride** is not chosen, it is speed ÷ cadence. In the stamp the speed is almost zero, so the stride is almost zero: stepping in
  place comes for free. On the ramp the stride lengthens by itself. Feet cannot slide, by construction.
- The step phase is accumulated frame by frame, so a cadence change never jumps.

The **effort drives the body**: forward lean and arm swing in proportion to the acceleration, not to the speed. A steep ramp gives more lean
and push; when the curve flattens the body straightens and hands off to the run, in phase with the feet. The animation then follows any
curve the owner tunes, without remaking clips. The lean is what reads "I am speeding up" (Astro Bot, Mario).

## On the capsule (this branch)

`UGSTN_GreyboxAnimInstance`, settings `Greybox|Run Drive` and `Greybox|Boost Dash` in `FGSTN_GreyboxShapeSettings`:
- `bDriveFromRunState` (on): the frantic share of the sway (cadence `FranticCadenceHz`, amplitude `FranticAmplitude`) is 1 while the stamp is
  live, else the effort (`EffortFullAcceleration`, `EffortDeadZone`); it rises at once and eases back at `DriveReleaseSpeed`. Off = the old
  fixed `FranticSeconds` window after leaving rest.
- Effort lean `EffortLeanDeg` on top of the acceleration lean; `StageTellLeanDeg` while the next stage is announced (sim tell).
- A run stage step (sim entry burst) is a short climb of the speed, so it gets a short frantic burst and lean by itself.
- Boost dash (L3, Smash's dash): `DashPoseSeconds`, `DashLeanDeg`, `DashSquash`: forward and low at the launch, eased out into the run.
- The capsule shows no legs in locomotion, so the stride is implicit (the sway phase at the cadence); the limb markers and the Voyager can
  read it explicitly later.

## For a real character (Voyager, later)

Per-character data in the animation profile: the cadence curve (stamp cadence, run cadence, release), the maximum effort lean and the exit
threshold. The Voyager would have a higher cadence and less lean than the Overlord.

## Run stage steps

Owner 2026-11-24 23:05: between run stages "not a ramp, almost a step". The simulation already has it (readable stages S1,
`gstn.RunStage.Readable` 1): per stage `EntryBurstFrames` / `EntryAccelMultiplier`, which replace that stage's acceleration ramp. The data is in
the owner-local `DA_Locomotion_Default` (and in each kit profile): `Tools/GAS/setup_run_stage_steps.py` writes 3 frames ×6 on stage 2 and
2 frames ×8 on stage 3 (`STEPS_MODE=dump` reads the current values first).
