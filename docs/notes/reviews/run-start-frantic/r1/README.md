# Run start frantic r1: key poses, curve sheet, self-check (desktop, Blender only, 2026-11-19)

Clip `AS_RunStart_Frantic_r1` (FBX + .blend here). 27 frames at 30 fps = 0.9 s, timing B (owner 23:53Z).
Skeleton: UEFN mannequin from `Saved/AnimPipeline/refs/idle/M_Neutral_Stand_Idle_Loop.fbx` (the only mannequin FBX on disk;
no mesh). Start pose = that idle's frame 1. Root in place, untouched. Made by `author_runstart.py` (blender -b, see its docstring),
grids by `grid2.py` (every 2nd frame), `metrics.json` = per-frame numbers. No Unreal, no _ue59.

**Author: Claude (Opus), not agy.** agy-bridge is sandboxed, has a 10-minute limit per call, and timed out on 4 of 4
pose-authoring calls on 2026-11-15 (memory agy-bridge-limits). So I scripted the poses as world-space targets with my own
two-bone IK, then keyed FK on every frame. Blender's IK was tried first (attempt 1): it fails on UE-imported bones because
their bone tails do not point at the child joints.

## Key poses (Blender world: -Y forward, metres; foot = ankle target)
| Frame | Pose |
|---|---|
| 1 | Burst: pelvis 0.84 (idle 0.906), lean 30 deg, right knee up (foot 0.32 high), left fist up near the chin, right fist back. |
| 3 / 6 / 9 | Stamps in place R / L / R: pelvis 0.80 (-4 cm vs the passing frames), flat feet within 10 cm of the start spot, arms at their widest 1 frame later (4 / 7 / 10). |
| 4-5, 7-8 | Knee up between stamps (foot 0.30), pelvis 0.84. |
| 11 | Push into the run: pelvis 0.87, lean 28, left knee drives up to 0.38. |
| 13 / 19 / 25 | Run contacts L / R / L at y -0.28..-0.30, pelvis 0.83 / 0.83 / 0.845 (less low as speed rises). The stance foot slides back about 0.38 m in 3 frames (in place, so the sim carries the travel). |
| 16-18, 22-24 | Toe-off, then heel kick to 0.35-0.38 behind the body. |
| 27 | Left stance and right knee passing. Lean 14 (13 at f25 = one small undershoot, then it settles). |

## Curve sheet (rule 13c)
- Foot height and pelvis height: QUAD **ease-in** into every contact (heavy, accelerates into the floor), **ease-out** out of it (snappy lift); smoothstep elsewhere.
- Everything else (foot y, toe pitch, pelvis sway/yaw, lean, hand and elbow targets): Catmull-Rom through the keys (fluid arcs, no stops at the keys).
- Follow-through: arm extremes 1 frame after each contact. Head and neck counter-rotate 35% and 25% of the lean, 1 frame late (head stays pitched down).
- Hips yaw ±6 deg toward the forward leg. Shoulders (spine_04) counter-yaw ×1.6. Clavicles 6 deg up/forward (hunched).
- Cadence: in-place stamps every 3 frames (10 steps/s), run steps every 6 frames (5 steps/s).

## Self-check (I looked once at each grid)
- Trait 1, frantic limbs: **mostly yes**. Knees high in place f1-f9, arms pump fist-to-chin / far back, upper-arm swing 141 / 132 deg
  (standard_run 124 / 106), elbows 32-106 deg. Problem: on the back swing in the run part (f13-f27) the elbow rises to shoulder height
  ("chicken wing", clearest in the 3/4 grid). The back-pump elbow pole is too high.
- Trait 2, heavy: **partly**. Pelvis drops 4-5 cm on each contact (0.80-0.88), flat feet, head low. But the torso reads only about 10-15 deg
  forward in the side grid, against the 26-30 deg I keyed: the neck/head counter-rotation and the spine split eat part of it.
  The lean was not measured, only judged by eye.
- Trait 3, short and clean: **yes for the start, unverified for the hand-over**. Run pose from f1, 0.9 s.
  The phase of M_Neutral_Run_Loop_F could not be read without the editor, so f27 (left stance, right knee passing) is a guess.
- Floor: the ankle never goes below 0.094 and the ball never below -0.002 m.

## Unsure / for the next thread
1. Run-loop phase match at f27 (needs the loop's foot curves from UE).
2. FBX scale and root on the way back into UE: exported with Blender defaults, armature object = `root`. Check bone lengths on import (pipeline `bone_lengths.py`).
3. No finger curl: hands are the idle's. Fists still to do (the grid draws a ball).
4. Fixes for r2 if Fable agrees: lower the back-pump elbow pole, more lean through the spine (less head counter-rotation).
