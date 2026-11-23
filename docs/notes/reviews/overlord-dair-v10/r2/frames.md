# Overlord dair V10 round 2: frames (video v, 60 fps). Base = V9 (Saved/DairV10/base, base_r2 for dair_land_early), variant = KIT_Test_OverlordDAirV10R2
Captures: `Saved/DairV10/{variant_r2,variant_r2b}/<seq>/frames.jsonl` (`Tools/GASNPPTest/pie_capture_timed.ps1 -Kit KIT_Test_OverlordDAirV10R2 ...`).
| sequence | charge start | touchdown (actor stops) | landing clip | clip | end actor z (standing 88.1) |
|---|---|---|---|---|---|
| dair_land V10R2 | v75 | v86 | v88 (anim 12 of the move) | AM_OverlordDAirLandHeavyR2 | 88.1 (round 1: 35.8, V9: 60.5) |
| dair_hit V10R2 | v71 | v84 | v86 (anim 14) | AM_OverlordDAirLandHeavyR2 | 88.1 (V9: 78.2) |
| dair_land_early V10R2 | v84 | v86 | v86 (anim 2) | AM_OverlordDAirLandLightR2 | 88.1 |
| dair_whiff V10R2 | v39 | after the move (lands in normal locomotion) | - | - | 88.1 |
dair_land_late (pressed rising at 60 cm): the move still ends in the air (v68): with this jump a landing in f32+ does not happen; light window f32+
NOT captured. Feet rebound f19-20 / recovery: g8 (whiff, strike to FAF, every 2nd frame).
Grids (base top): g1-g3 dair_land side/game zoom/back, g4-g5 dair_hit side/game zoom, g6-g7 early light side/game zoom, g8 whiff strike+recovery.
Game zoom = crop 135,290,295,450 of the 480 px game camera (`pie_capture_pair.py --crop`).
