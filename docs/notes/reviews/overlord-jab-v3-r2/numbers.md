# Overlord jab v3 round 2: numbers (base = r1 build captured before; variant = r2 recaptured after jab 1 was rebuilt; Play captures Saved/AnimR2/jab/{base,variant})
Sequences jab1, jab1_jab2, jab1_jab2_hit, jab1_jab2_faf (kit KIT_Test_Overlord). Hand = hand_r, component space of the built copies (+X left, +Y fwd, dummy centre y 95); numbers by Saved/AnimR2/measure_jab_r2.py.
Build worst vs source: jab1 2.27 cm (calf_r), at the poses 2.27 cm (jab 1 rebuilt after the first frames were re-posed); jab2 2.03 cm (calf_r), at the poses 2.03 cm. (hand_r is not the worst bone any more; bone lengths PASS both.)
Author/build exit 0 both cards; manual layer 0 keys; ik feet max 0.0 cm. Kit refreshed (setup_overlord_jab_kit refresh=True: AS/AM_OverlordJab{1,2}V3 rebuilt, montage settings unchanged), lengths 0.467 s / 0.867 s.
## check gates (recorded, not fixed)
jab1 (rebuilt, gates re-recorded): FAIL (legibility: rear arm hides the fist 2, attacking arm hidden from game camera 2, hand_l open 1, hand_r open 1); bone_lengths, clearance (1 pair-frame <2 cm), contact_faf (peak f6, window 6-8, FAF 22), planted_feet 0.23 cm, fists, source_fidelity, hitstop_snap (max 8.4 cm 6->7), co_motion PASS; motion_check FAIL (stiff chain 4, no overlap 4, unnatural joints wrist_l/wrist_r/foot_r...).
jab2: FAIL (legibility: rear arm hides fist 3, attacking arm hidden from game camera 3, hand_l open 1, hand_r open 1); bone_lengths, clearance (6 pair-frames <2 cm), contact_faf (peak f9, window 9-11, FAF 26), planted_feet n/a, fists, source_fidelity, hitstop_snap (max 6.3 cm 11->12), co_motion PASS; motion_check FAIL (stiff chain 3, no overlap 4, unnatural joints wrist_r/foot_l/elbow_l...).
## Hand (hand_r)
jab 1 f0 (-3.4, 19.0, 99.0); f6 (-17.9, 77.6, 134.2); f8 (-24.0, 75.1, 134.5); f12 (-22.3, 74.1, 136.5).
jab 2 f0 (-40.8, 43.0, 150.1); f9 (-20.2, 79.0, 128.1); f11 (-20.9, 78.9, 125.2); f26 (-25.0, -17.0, 85.5) = idle region.
## Jab 2 f26 distance from idle (= jab 2 f52 pose): pelvis 0.54 cm, foot_l 0.05, foot_r 0.04. Pelvis at f9 (10.1, 21.0, 88.6), idle (2.1, -2.5, 90.6).
## Capsule over the chain (variant, all 4 sequences, every frame): loc constant (0, -450, 100), z 100.0, displacement 0.0. Base same.
## Frame mapping (variant vs base), confirmed on the new capture: identical. 95/140/160/150 frames; first press v20, jab 1 from v21, jab 2 from v33, new jab 1 on v59 (faf). Combat timeline idle from v48/v84/v87/v87 in both.

## Montage frame (jab1_jab2_hit, variant | base)
v20 none | none; v21 Jab1 0.02 | 0.00; v22 2.10 | 1.25; v23 2.10 | 1.67; v24 4.18 | 2.71. Variant repeats frame 2.10 on v22 and v23 (one stalled tick), base does not.
v32 Jab1 10.51 | 9.04; v33 Jab2 0.00 | 0.00; v34 1.12 | 0.26; v35 1.60 | 1.17; v36 2.64 | 2.21; v37 3.68 | 3.25. v34->v35 advances 0.48 (variant), 0.91 (base): slow tick still there.
## Last video frame with a montage | first frame without (variant | base)
jab1: v43 Jab1 22.93 | v44 23.35 (idle from v44 | v45). jab1_jab2: v79 Jab2 47.02 | 46.93, idle v80 both. jab1_jab2_hit: v82 47.53 | 47.10, idle v83 both. jab1_jab2_faf: v82 Jab1 23.73 | 23.79, idle v83 both.
