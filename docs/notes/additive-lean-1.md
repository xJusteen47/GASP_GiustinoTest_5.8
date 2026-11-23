# Additive lean 1 (cloud, 2026-11-18)

Owner 2026-11-18 12:08Z: "voglio l'additive lean durante il movimento, mettilo con GASP". 12:10Z: locomotion animation depends on other factors
(character, body, weight, state), not only on the kit: dedicated data; the run / walk animations go there too.

Branch `claude/project-thread-zqg4ty` from `claude/project-thread-98dml5` (134dbbb1). Not built: needs the desktop build + tests.

## What GASP offers and what we used
- GASP 5.x has no separate lean layer: the lean is in the mocap clips picked by motion matching (run arcs, starts, stops, pivots). Adding an
  additive Control Rig or a lean blend space in the main ABP would mean BP wiring in the shared ABP (rule: no manual wiring).
- We already have the right seam: `FGSTN_AnimNode_DirectionalHitAdditive` in the main AnimGraph (after the montage slot) bends the spine chain
  (spine_01..head) for any `IGSTN_AdditiveReactionSource` on the actor; the skid placeholder lean already uses it. Legs, root, foot lock and
  GASP's orientation warping stay untouched; motion matching still picks the clip from the real trajectory.
- So: zero ABP changes. `UGSTN_RunPresentationComponent` (created by the sim component, game thread, presentation only) now also computes the
  locomotion lean and returns it as layer 1 of its additive request (layer 0 = skid placeholder).

## How the lean is computed (`Presentation/GSTN_LocomotionLean.h`)
- Measured horizontal acceleration of the presented Mover velocity, low-passed (`AccelSmoothingSeconds`, the velocity steps at 62.5 Hz).
- Split along the heading: sideways part = turning (speed x turn rate, points to the centre) x `TurnScale`; along part = speeding up / braking
  x `SpeedChangeScale`. Angle = accel x `MaxAngleDeg / AccelForMaxLean`, clamped to `MaxAngleDeg`.
- x speed fade (`MinSpeed` -> `FullLeanSpeed`), x `StageScale[run stage]`, x `AirScale` in air, x `AttackScale` in a move timeline,
  x `SlideScale` in skid / attack slide. Defaults 0 for the three states: the lean springs back to straight.
- A damped spring (`SpringFrequency` 4.5 Hz, `SpringDamping` 0.65) follows the target: a little overshoot = follow-through. Teleports ignored.
- Cost: a few vector ops per character per frame, no extra anim node, no asset load.

## The locomotion animation profile (owner's principle)
- `UGSTN_LocomotionAnimSet` = an anim set (`UGSTN_AnimSetDefinition` child) in the new slot `Anim.Set.Locomotion`. It is chosen by the
  existing anim set system: the character's collection (`DA_AnimSetCollection_SandboxCharacter`) by default, kit / weapon / skin collections
  above it, variants by cosmetic tags (`Anim.Trait.Build.Heavy`...). So it belongs to the character and its body, not to the kit; all kits
  share it today. No profile applied = the same values as built-in C++ defaults.
- `Tools/GAS/setup_locomotion_profile.py` creates `DA_AnimSet_Locomotion_Default` and adds it to the character collection (data only).

## Proposed migration (not done, needs the owner's go)
| What | From | To | Est. |
|---|---|---|---|
| Skid presentation values (`PlaceholderLeanDeg`, clip stop fraction, exit rate) | kit `FGSTN_RunSkidTuning`, layer class defaults | `Anim.Set.Skid` set / profile | ~2 $ |
| Run start clip choice | layer class default | `Anim.Set.RunStart` set (clip as a field) | ~1.5 $ |
| GASP locomotion set (chooser table / pose search databases: walk, run, sprint) | hard refs in the main ABP / chooser | profile field read by the main ABP through a script-built binding | ~5-7 $ + desktop capture |
Skid and run start clips are already per-character anim sets (`Anim.Set.Skid`, `Anim.Set.RunStart`); only their numbers sit in the kit.

## Values to tweak (in `DA_AnimSet_Locomotion_Default` > Lean, or live: `a.GSTN.LocomotionLean 2` doubles, `0` off)
MaxAngleDeg 14, AccelForMaxLean 3000, TurnScale 1, SpeedChangeScale 0.5, MinSpeed 60, FullLeanSpeed 350, StageScale {1, 1.15, 1.3},
AccelSmoothingSeconds 0.06, SpringFrequency 4.5, SpringDamping 0.65, AirScale 0, AttackScale 0, SlideScale 0.
Debug: `GetLocomotionLean()` on the run presentation component (BP pure). `a.GSTN.HitAdditive 0` turns off the whole node (hits too).

## Desktop to-do (one thread, editor closed)
1. Build (Run-Timed.ps1), run `GSTNCombat.NP.LocomotionLean.*` + the usual suite.
2. Run `Tools/GAS/setup_locomotion_profile.py` headless (expects `ESITO: OK`).
3. Capture: run straight, S-turn, 180 at sprint; real speed + game camera; with `a.GSTN.LocomotionLean 0` as the base (rule 10). Fable review.

## Known limits
- The head bends with the spine (the chain shares include neck/head): if it reads as "head tilts", add a head counter-share option.
- The bend is about the hips' up axis: a strong lean without hip shift looks like bending, not like a whole-body tilt; a later option could
  tilt the pelvis too (needs foot IK care).
