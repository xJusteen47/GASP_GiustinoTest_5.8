# Overlord dair V10 round 3: frames (video v, 60 fps). Base = V9 (Saved/DairV10/base; base_r2 early, base_r3 apex), variant = KIT_Test_OverlordDAirV10R3
Captures `Saved/DairV10/variant_r3d/<seq>/frames.jsonl` (apex: variant_r3). Windows (internal): 0-2 LightR3, 3-13 HeavyTuckR3, 14-30 HeavyR2, 31-42 LightR3.
| case | charge start | touchdown | landing clip | pelvis world (cm) through the clip |
|---|---|---|---|---|
| dair_land (tucked charge) | v76 | v87 (ball, pelvis 22) | v88 HeavyTuckR3 | -12 (1-frame glitch v88), 22, 19, 18, 17 ... 21 (f9), 28, 38, 50, 63, 78, 87 (v103), standing 96 by v105 |
| dair_hit (after the strike) | v71 | v84 | v86 HeavyR2 | 70 -> 55.5 (f3) -> 60 -> 92 by v101 |
| dair_land_early | v84 | v86 | v86 LightR3 | 40 (contact, crouched) -> 68 -> 81 (f4) -> 84; from v95 the base layer's landing crouch shows again (pelvis 64) |
| dair_land_apex (pressed falling at 240 cm) | v68 | v90 | v91 HeavyR2 (anim ~23) | the late light window f32+ is still not reachable with the hub's jump |
Glitch v88 (code queue): capsule restored one frame before the landing clip shows (actor standing, air pose drawn from the standing root);
timing dependent (variant_r3c, same case: f0 at 25.7, no glitch).
Grids (base top): g1-g3 tucked landing side/game zoom/back, g4 heavy after the strike (hit), g5-g6 early light side/game zoom, g7 apex,
g8 whiff full move every 2nd frame. mp4: dair_land_base / dair_land_r3 / dair_land_early_r3 / dair_hit_r3 (7 views, 60 fps).
