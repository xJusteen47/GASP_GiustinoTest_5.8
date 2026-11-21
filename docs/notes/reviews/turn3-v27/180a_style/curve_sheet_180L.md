# Curve sheet, run start turnaround 180 L, variant 28 (style pass on 27), rule 13c
Scene RunStart_Generic_w2t180Ls.casc (root frame: +Z = final heading, +X = L; the 180 deg yaw is on the root bone, added at UE import).
Frames @60. H = 6 (hand-off to the w2 rev). Rev f9+ unchanged from the 27 composer (dense keys).

## Key poses (layers Arm_L, Hand_L, Arm_R, Hand_R, Head; keys 0, 2, 4, 6, 8, in-betweens 1/3/5/7 by Cascadeur)
| key | L elbow dir from shoulder (x,y,z) | R elbow dir | note |
|---|---|---|---|
| f0 | idle (unchanged) | idle | entry from idle |
| f2 lead | 0.45,-0.55,-0.70 back+out | -0.55,-0.75,-0.35 out | L arm leads one frame before the chest |
| f4 step plant | 0.70,-0.55,-0.45 toward new heading | -0.70,-0.60,-0.10 out to own side | no twinning: L back, R side |
| f6 H | 0.70,-0.50,-0.50 overshoot out | -0.60,-0.65,-0.35 | overshoot, settles into the pump at f8 |
| f8 H+2 | rev pump (composer) | rev pump | hand-over |
Forearm and hand rotate rigidly with the upper arm (lengths kept, gate bones 0.0 %).
Pelvis (dense, all layers above the legs): +0.5 / +3 / +4 / +5.5 / +5 / +2 cm at f1-6 -> height 89.5, 87.8, 87.7, 86.6, 86.9,
86.4, 84.8: phase 1 drop 3 cm (identity: -3/-4 cm; the 27 dropped 9 cm). Legs re-solved by Cascadeur, feet untouched.

## Interpolation and easing (Cascadeur clamped bezier, key weights left/right; 0.333 = default)
| key | left | right | intent |
|---|---|---|---|
| f0 | - | 0.33 | (r0 had 0.10 = instant departure; r1 back to 0.33, hand_l zigzag) |
| f2 | 0.33 | 0.33 | continuous lead |
| f4 | 0.33 | 0.25 | quicker push out of the plant |
| f6 | 0.20 | 0.33 | short in-handle = arrive fast, overshoot |
| f8 | 0.55 | - | ease into the pump (follow-through) |
