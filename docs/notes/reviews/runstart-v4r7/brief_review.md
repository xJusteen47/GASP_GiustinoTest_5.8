# Review brief: run start v4r7 (one move, rule 10)

Variant AS_GSTN_RunStart_v4r7 vs base AS_GSTN_RunStart_v4r6 (active until now), same capture frames.
Files here: grid_az90.png, grid_az0.png, grid_game.png (top v4r6, bottom v4r7), runstart_v4r7.mp4, metrics_*.json.
Capture: Saved/RunStartV7/v4r7 (mv_run_start_v5, KIT_Test_Overlord, gstn.Loco.Base 1, 1 player).
ASSERT: OK mv_run_start_v5 AS_GSTN_RunStart_v4r7 f95-f170: weight >= 0.8 on 93% (min 0.56) (Saved/RunStartV7/v4r7/assert.txt).

## Owner asks (2026-11-20)
1. Arms more at rest: lower, less high, less forward. Feet: "perfect", must not change.
2. No visible high-to-low arm interpolation when the start blends into the GASP run loop (in game ~v156-v184).

## Identity (rule 13a, check first)
- Frantic rev-up: fast arm pump from frame 1, same rhythm as v4r6.
- Brute Overlord: heavy, forward-driving torso; arms pump with the body, not stiff.
- Continuous acceleration ramp, no planted first step.

## What changed (Cascadeur, runstart_arms_rest_casc_v4r7.py)
Forward hand peak +16 cm above the shoulder -> -4 cm (elbows re-solved, bone lengths exact). From clip f24 the arms blend to the
stock loop arm pose at the same phase (weight 1 on f30-38). Legs, feet, root, torso untouched (feet slide f80-240 L/R 36.7/33.3 vs
36.5/33.2 cm). Arm clearance min +4.9 cm (v4r6 +1.8).

## Judge
Life first on the video without numbers (anticipation, arc, weight, follow-through), then: arms lower and calmer but still frantic;
no arm pop or slide into the loop; no arms through the body. FAIL items labelled posa / dati montage / gioco. A fix that cuts arm
amplitude by more than 15 % beyond the owner's ask is FAIL.
Known lead (not proven): in game the loop is BS_Relaxed_Run_Loop_F_Slopes, while the clip tail was built from the Neutral run loop:
if the arms still slide into the loop, say so (label "dati montage").
