# Overlord dair V10, review r2 (Fable, on 716d492c): FAIL (narrow); round 3 answers (desktop)
PASS: seiza diagnosis (pelvis not raised), heavy clip from the strike pose (g4/g5), whiff strike + recovery (g8), charge.
FAIL 1 (posa + dati montage): landing during the tucked charge: ball on the floor v86-87, clip f0 v88 nearly standing, squat by v90-91 (low-tall-low).
Reviewer's call: separate window f4-12 with a TUCKED START clip (f0 = ball on the floor, f1-2 feet slide out to 48 cm, pelvis at or 3-5 cm under the
ball, never above it before the f10 rise). Windows f1-3 light, f4-12 heavy-tucked, f13-31 heavy, f32-42 light, same lag values.
FAIL 2 (dati montage/gioco + posa f0): light landing: f0-f2 pop upright, then 8+ frames of a deepening squat with arms forward (something else plays).
Fix: light f0 crouched (pelvis ~-20, arms crossed) rising over the 4 f; find what plays after and remove it / blend to idle (max 2 attempts).
Warning: 2 f latency before the landing clip (code queue).

## Round 3 (desktop), kit KIT_Test_OverlordDAirV10R3 (TL_Overlord_DAirV10R3)
- Ball on the floor measured (Saved/DairV10/variant_r2/dair_land/frames.jsonl v86-87): pelvis 25-28 cm, feet 7-17 cm, head 54-60 cm over the floor.
- AS/AM_OverlordDAirLandHeavyTuckR3 (card overlord_dair_land_heavytuck_r3): f0 pelvis -65 (= the ball), f1-f2 feet slide to 48 cm, pelvis -67/-69,
  held -69..-66 to f9, rise f11 -52, f13 -30, f15 -7 (head first); knees over the toes, arms crossed at the sternum. Windows: internal 0-2 LightR3,
  3-11 HeavyTuckR3, 12-30 HeavyR2 (unchanged), 31-42 LightR3; lag 4/16/16/4.
- What played after the light clip (r2, v88+): the locomotion's own landing reaction (the combat montage had ended after 2 frames, the base layer
  showed its land pose: pelvis 77 -> 60, hands down to 58 cm). Attempt 1: AS/AM_OverlordDAirLandLightR3, 12 frames: f0 crouched contact (pelvis -20,
  arms crossed), upright by f4 (end of the lag), f4-f11 settle in the stance, blend out 4 f: the montage now covers the frames the land reaction used.
- Late light window: dair_land_apex (pressed falling at 240 cm) tried, see r3/frames.md.
