# Run start frantic r3 (desktop, Blender only, 2026-11-19), last pose round

Clip `AS_RunStart_Frantic_r3` (FBX + .blend), 27 frames at 30 fps, root in place, UEFN mannequin from the idle FBX.
`author_runstart_r3.py` = r2's script with the r3 changes from `../r2/REVIEW-fable.md`. Numbers come from `metrics.json`
(`summary` + `per_frame`). No Unreal, no _ue59.

Two attempts. Attempt 1 (straight parameter change) failed two gates: left elbow at f7 only 0.072 m behind the back, and back bend 14.7 deg.
Cause: **the idle start pose is twisted**, hips ~16 deg and shoulders ~24 deg (left shoulder 14 cm ahead of the right),
so the left arm sat forward. That also explains the left/right asymmetry in r1/r2. Attempt 2 adds three steps per frame:
1. square hips and shoulders first;
2. if the back bends more than 11 deg, take the excess out on spine_03, then re-match the lean from the pelvis;
3. pull the head back on neck_01 to 2 cm ahead of the chest line.

Consequence: the lean split is no longer exactly pelvis 55 / spine 45. It is keyed that way, then the lean match and the back
straightening are done on the pelvis and spine_03.

## Spec item (r3) -> measured
| # | Spec | Measured |
|---|---|---|
| 1 | S from the world vertical, S_world per frame in metrics | `S_world_l/r` per frame; f4 L +30 / R -50, f14 L -62 / R +35 |
| 2 | f1-10 fwd +30/E90, back -50/E95; f11-27 fwd +35/E90, back -62/E95 | S range -62.0..+35.0; E range 90.6-96.1 |
| 3a | Fist centre >= 5 cm below the shoulder, >= 25 cm from the head | min 0.067 / 0.070 m below (L/R); min 0.367 / 0.369 m from the head |
| 3b | Back extremes: elbow >= 10 cm behind the pelvis-neck_01 line, >= 10 cm under the shoulder | L7 0.143, L14 0.201, L26 0.232, R4 0.157, R10 0.143, R20 0.215 behind; under the shoulder 0.122-0.168 |
| 4 | Abduction 6 forward, 12 back | 6 -> 12 with the back swing, max 12.0 |
| 5 | Lean f1 18 ... f9 28 ... f27 16 | max error vs keyed 0.0 deg |
| 6 | Split 55/45, spine_04-05 +3; back bend <= 12 deg every frame | keyed 55/45 + 3 (see above); back bend max 11.0 |
| 7 | Clavicles 8 up / 4 fwd; head 2 cm ahead; head counter 25% | keyed; head 0.021-0.022 m ahead |
| 8 | Pelvis f3 -7, f6 -9, f9 -10 (+ ripple -1.5), f10 -7, f11 -3; knee 60-150 | f1 0.856, f3 0.821, f6 0.801, f9 0.791, f10 0.836, f11 0.876; knee 64-136 |
| 9 | Nods halved: +1.5 chest, +2 head | keyed |
| 10 | f27 unchanged; ankle >= 0.094, ball >= 0, root in place | f27 = r1 phase (unverified); ankle min 0.094, ball min 0.018; root untouched |
| r2 kept | Run legs, stamps, fists | contacts 0.18 ahead, feet apart max 0.38, lift 0.32/0.33, spread 6/4 cm, fists on all frames |

## My look (side grid + `strip_back_extremes.png` f14/f20/f26, once)
- Back fist behind the torso line: **yes at f14 and f26** (the arm nearest the camera: elbow well behind the back, fist at the hip).
  At f20 the back arm is the far one, hidden by the body in the side view; metrics say its elbow is 0.215 m behind.
- No boxer guard: the forward fist is at chest height, below the shoulder. The back is broad and nearly straight, leaning from the hips.
- Still low and loaded, knees high while stamping, short steps.

Fable's process note: with gates 3 and 6 passing, the next step is the editor import (f27 re-key to the run loop + capture).
