# Overlord dropkick r1: numbers (variant only, Play capture Saved/AnimR2/dropkick/variant_premerge, sequence dropkick_whiff, kit KIT_Test_OverlordDropkick)
Montage layout (frames of AM_OverlordDropkick): startup 15 + loop 11 x4 + land 31 (land starts at montage frame 59). Attack sent on v42 (T), montage v43-v129 (m4), idle from v130; 170 frames.
Land section on v100-101 at height h about 9 / -4 cm above the standing z (v100 h 8.6, v101 h -3.8); first grounded frame v102 (h -11.8, flat from there).
Hitbox window: active from startup f15 to the end of the loop = montage frames 15-58, about v58-v100 (timeline TL_Overlord_Dropkick, from the timing CSV; shape and hit numbers PROVISIONAL).
Check gates (recorded, not fixed), Saved/AnimPipeline/animpipe/overlord_dropkick_<clip>_check.txt:
- startup: FAIL; legibility FAIL (rear arm hides fist 1, attacking arm hidden 1), contact_faf FAIL (peak f15, window 14-14, FAF 15), fists FAIL (0.99 > 0.7), hitstop_snap FAIL (14->15 54.1 cm, max 10), co_motion FAIL (arm_r 3-8); bone_lengths, clearance, planted_feet, source_fidelity PASS.
- loop: FAIL; legibility FAIL (check crashed: KeyError '12'), contact_faf FAIL (peak f0, window 0-11, FAF 12), fists FAIL (0.99 > 0.7); bone_lengths, clearance, planted_feet, source_fidelity, hitstop_snap, co_motion PASS.
- land: FAIL; legibility FAIL (fist not distinct 1), clearance FAIL (hand_r|spine_03 -4.17 cm f15), contact_faf FAIL (peak f3, window 30-30, FAF 31), fists FAIL, co_motion FAIL (arm_l, leg_l 10-30; leg_r 4-8, 11-30); bone_lengths, planted_feet, source_fidelity, hitstop_snap PASS.
No base: new move. Loop count and press frame were tuned for this capture only; the game does not yet hold the loop until landing.
Files: dropkick_whiff_variant.mp4 (60 fps, 7 views), dropkick_whiff_variant_slow.mp4 (15 fps = 0.25x), g1_dropkick.jpg (2 rows game/az90 x 12 columns).
