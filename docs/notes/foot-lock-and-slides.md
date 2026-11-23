# Foot lock, intended slides, return to idle, slopes (2026-11-19)

Sources: Unreal Assistant answers in `docs/notes/assistant/2026-11-19_q2..q4.md` (it cannot open our assets: it answered from
the GASP docs), plus what was checked in the assets themselves (string scan of the .uasset files, engine headers).

## Who locks the feet in OUR ABP (verified)
- `ABP_GSTN_Locomotion` does NOT use the engine Foot Placement node (no `DisableLockCurveName` there; only the GASP CMC
  sample ABP `SandboxCharacter_CMC_ABP` has one). Our feet go through the control rig `CR_Biped_FootPlacement`.
- That rig pins a foot from the clip's **`contact_l` / `contact_r`** curves (`GetCurveValue` + `FootContactLockThreshold`,
  `LeftFootPinned` / `RIghtFootPinned`, `MaxFootPinRadius`). So in GASP "planted" = contact curve over the threshold.
- After it, our C++ node `GSTN_AnimNode_BlendOutFootLock` (montage blend-out lock + pop guard).
- The skeleton also registers `Disable_FootPlacement_L/R` and `FootSpeed_L/R` (engine Foot Placement node curves, unused by us).

## Intended slide: how to author it (owner 2026-11-19: on the clip, by the designer)
One marker on the AnimSequence (a montage inherits it from its clip): float curve **`GSTN_FootSlide_l` / `GSTN_FootSlide_r`**
(or `GSTN_FootSlide` for both), 1 over the slide, a few frames of ramp in and out. The Assistant recommends exactly this
(its answer: a "disable" curve switches the IK off, a slide keeps the foot on the ground and only frees it in XY).
- GASP rig: the slide must not be pinned, so `contact_<side>` must read 0 inside the window. Rule for authoring tools
  (Tools/GAS/routing/curves_author.py): contact curves are zeroed where GSTN_FootSlide > 0.5. That is what lets GASP's pin go
  BEFORE the slide instead of popping at the end (skid pop, anim-setup-2: the pin let go of contact_r at 0.333).
- Our foot lock: the pop guard ignores that foot (property `SlideCurve`).
- Clip check: drift/skate inside the window = WARN, not FAIL. Fixers: floor clamp lifts only vertically.

## Return to idle with both feet up (owner: after the jab)
Cause found in our node: `SecondStepStart` 0.6 started the second step while the first foot was still up (40 % of a step with
both feet in the air). Now 1.15 (one foot at a time, short double support, farthest first; ClampMin 1). The Assistant adds:
lock in world space (Offset Root Bone goes to Release at montage end), and the GASP rig must not unpin the foot our node is
stepping (to check in the capture). Check rule in clipcheck: both balls > 2 cm up in the last 0.5 s before an idle = FAIL.

## Slopes (to test in L_Test_Hub zone Z7: hills at 8, 15, 25, 35 deg)
Assistant checklist: capsule speed measured in 2D for stride warping; idle on a slope (pelvis drops, ankle pitch limit ~30-35
deg); landings recorded on flat ground float/penetrate then snap; stop on a slope eats leg extension; Mover walkable settings
(`MaxWalkSlopeCosine`, floor `ImpactNormal`). Test: idle, run up/down, stop, skid while running downhill, landings.
