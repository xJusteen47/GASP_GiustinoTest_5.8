# Run start frantic r2 (desktop, Blender only, 2026-11-19)

Clip `AS_RunStart_Frantic_r2` (FBX + .blend), 27 frames at 30 fps, root in place, UEFN mannequin from the idle FBX (as in r1).
Authored to the r2 spec in `../r1/REVIEW-fable.md` by `author_runstart_r2.py`. That script is r1's export, proxy and render
tail plus a new core: FK arms, fists, measured lean, legs by analytic two-bone IK keyed as FK on every frame.
Done in 1 attempt. Numbers come from `metrics.json` (`summary` + `per_frame`), written by the same script. No Unreal, no _ue59.
I looked once at each grid.

## Spec item -> measured
| # | Spec | Measured (metrics.json) |
|---|---|---|
| 1 | Arms FK (S + E), sine ease, abduction <= 12, elbow >= 12 cm under the shoulder | FK, sine between extremes; abduction 8.0 L/R; elbow under the shoulder min 0.132 m |
| 2 | f1-10 S +45/E100, -40/E85; f11-27 S +55/E100, -60/E80; E 75-105 everywhere | S range -60.0..+55.0 (L and R); E range 81.0..100.7. Per-frame values are in `S_*` and `E_*` |
| 3 | Extremes 1 frame after the contacts (4, 7, 10, 14, 20, 26); wrist lags 1 frame, 8 deg | Keys at those frames; wrist bend = 8 deg x the swing speed of 1 frame earlier (clamped) |
| 4 | Fists on all frames: 75/90/60, thumb 30/40 | Keyed on all 27 frames (rotations 75/90/60, thumb 30/40, applied on top of the idle's relaxed curl). Measured middle finger at f1: base 80, middle joint 132-144, tip not measurable (the skeleton has no fingertip joint). The proxy draws a ball, not fingers |
| 5 | Lean pelvis->neck_01 within 3 deg of keyed | max error 0.7 deg. Split pelvis 30 / spine 70 is keyed first, then a correction on spine_01-03 makes the line match (the hunch adds lean) |
| 6 | Hunch: clavicles 12 up / 10 fwd, spine_04-05 +8, head 4 cm ahead of the chest line | Keyed; head 0.044-0.050 m ahead of the pelvis->neck line |
| 7 | Pelvis f1 0.856, f3 -8, f6 -10, f9 -12 (0.786), ripple -1.5 on contact, f10 -8, f11 0.876 | f1 0.856, f3 0.811, f6 0.791, f9 0.771 (= -12 plus the ripple), f10 0.826, f11 0.876 |
| 8 | Stamps R3 L6 R9, flat feet, lift 0.30-0.34, within 8 cm of the start spot | Lift max L 0.32 / R 0.33; spread from the start spot L 0.06 / R 0.04 m |
| 9 | Run contacts L13 R19 L25, 0.18 ahead; toe-off 0.15 behind; feet <= 0.40 apart; slide 0.33 m in 3 frames, linear; heel kick 0.38; thigh 70 deg at f11, f16-17, f22-23 | Contact ahead 0.180 (all 3); toe-off 0.150 (L16, R22); feet apart max 0.38; stance keyed 0.11 m/frame linear; heel kick 0.38; drive thigh L11 69.9, R16 71.2, R17 67.3, L22 68.8, L23 65.5 |
| 10 | Pelvis f13 0.831, f19 -7, f25 -6, mid-flight 0.876; ease-in into contact, ease-out after | f13 0.831, f16 0.876, f19 0.836, f25 0.846 |
| 11 | Chest nod +3 one frame after the run contacts, head +4 two frames after, back in 3 | Keyed (spine_05 at f14/20/26, head at f15/21/27) |
| 12 | Hand-over to the run loop | **Not done**: f27 phase kept from r1 (L stance, R knee passing), pelvis 0.861 + head pulse; to re-key in the editor |
| 13 | Root untouched; ankle >= 0.094, ball >= 0; knee no lock/flip | Ankle min 0.094, ball min 0.018; knee interior angle 64-137 deg (no lock, no flip) |
| 14 | Foot slide with warping, blend-out pop | Capture round (gioco) |

## What I saw (stills, once per view)
- Hunched, low and forward on every frame. Knees high while stamping in place, short choppy run steps.
  In the 3/4 view the back fist stays beside the hip: no chicken wing.
- The forward fist comes up to the face (f7, f13-f15, side and 3/4): it reads a bit like a boxer's guard rather than a pump.
  This is the spec value (S +45..55 with E 100 while the lean is 30+). If Fable wants a pump, lower the forward E or S.
- The hunch plus the lean gives a strongly rounded back in the side view. Heavy, maybe too crouched for the hand-over (only the capture will tell).

## Unsure
- The finger rotation axis is chosen per joint by trial (the side that brings the tip toward the palm). It is not checked on the real mesh.
- FBX scale and root on the way into UE are unchanged from r1 (Blender defaults, armature object = `root`). Check with `bone_lengths.py`.
