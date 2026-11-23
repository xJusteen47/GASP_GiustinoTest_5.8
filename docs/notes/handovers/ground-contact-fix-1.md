# Ground contact fix 1 (cloud, Opus high, 2026-11-17, branch claude/project-thread-27htqa from 98dml5 164975b1)

Owner 21:35Z (ambito: reactions near the floor: knockdown, ground bounce, dropkick tech bounce, ground hit): "qualche impatto col terreno
si compenetra", wants a runtime correction that does NOT replace the animation.

## Choice (rule 6, applied)
- **C++ anim node at the end of the main graph** (`FGSTN_AnimNode_GroundContactFix`, GSTNAnimationCore), same pattern as the blend-out foot lock.
  Zero cost outside the listed states (no trace, pose untouched), testable pure math, inserted by script (no manual BP wiring), no project settings.
- Not Control Rig: same result (traces + IK) but a RigVM per character every frame and a Control Rig graph to author by hand/MCP; no gain here.
- Not Physics Control now: physics bodies are heavier on the toaster target and change the pose (not "only fix penetration"). It stays the
  M9 tool for "body settles on uneven floor, limbs react to hits while down" (piano §2b), on top of this node.

## What it does (only removes penetration)
1. Body: the deepest LiftBody sphere (pelvis, spine_03, head, knees, elbows) lifts `pelvis` along the floor normal; up at once, down with a
   0.05 s half-life, cap 35 cm (deeper = game bug, e.g. the capsule-restore order, left visible).
2. Limbs: hands/feet that touched the floor before the lift are put back there with two-bone IK (no floating hands); one still sinking is raised.
3. Weight 0..1 blended per rule: `DA_GroundContact_Default` rules = `Combat.State.Knockdown`, `Combat.State.Tumble`/`Combat.Mover.Tumble`,
   reaction montages found by name (TechBounce, GroundHit, GroundBounce, Knockdown, Lying, GetUp), plus anim curve `GSTN_GroundContact`.
4. Floor: one line trace a frame (game thread, PreUpdate) under last frame's pelvis, only while on; slopes over 50° ignored. LOD > 1 skipped.
Console: `a.GSTN.GroundContactFix 0` = off (A/B for the review), `a.GSTN.GroundContactFix.Debug 1` = floor, spheres (red = sank), lift.

## Files
Core: `GSTN_GroundContactSettings.h/.cpp`, `GSTN_GroundContactSolver.h/.cpp`, `GSTN_AnimNode_GroundContactFix.h/.cpp`, tests
`Tests/GSTN_GroundContactTests.cpp` (`GSTNAnimation.Core.GroundContact.*`, 3 tests). Editor: `GSTN_AnimGraphNode_GroundContactFix`,
library `InsertPoseNodeBeforeOutput`, `SetPoseNodeProperty`. Script `Tools/GASNPPTest/ground_contact_setup.py`. Plugin README row.

## NOT done (cloud, no engine): nothing compiled, nothing run
Next = desktop thread (Haiku/Sonnet low): build (Run-Timed 45 min), tests `GSTNAnimation.Core.GroundContact` + full suite, run the setup
script (changes the shared Mover ABP: owner warned in the thread), capture the r3 clips (reviews/dair-v10-dropkick-r3) fix ON vs OFF, then
Fable review rule 10 (the fix must not alter non-sinking frames). Max 2 fix attempts.

## Risks / to check
- Bone radii are first guesses for the mannequin; tune in the asset from the debug draw. Feet radius 7 = ankle origin.
- Retargeted characters (Overlord): if the visible mesh copies the pose from a hidden mannequin, the fix on the mannequin carries over
  approximately; if the visible mesh runs its own ABP, insert the node there too.
- One floor plane per body: steps/ledges under a lying body are not handled (M9 or per-bone traces later).
- Presentation only; the sim, timelines and hurtboxes never read the mesh pose (checked: GSTN_HurtboxPose is pure). No overlap with
  "Fix landing capsule and crouch glitches" (that thread fixes the capsule; this only hides what is left, capped).
