# Skid fix (skid-fix-1) metrics, 2026-11-20

Capture: `pwsh Tools/AnimPipeline/cards/skid/skid_fix_capture.ps1 -Out Saved/SkidFix/<before|after> [-Bake]` (mv_skid, KIT_Test_Playtest,
1 player, PIE_CAPTURE_NOVFX=1, base 0). Rows count the 20 PRE frames: stick reversed on row f115, sim turns from f117.
Metric: `python Tools/AnimPipeline/cards/skid/skid_foot_metric.py <cap>/mv_skid/frames.jsonl <csv> [--from --to]` (back foot = behind along
the old run direction at the plant; planted = world z <= its min + 8 cm; travel = horizontal steps between planted frames).

| | before | after |
|---|---|---|
| Back foot (foot_r) planted travel, f117-f167 (reverse input -> clip end) | 171.0 cm (33 fr) | 153.9 cm (47 fr) |
| Back foot planted travel, f143-f167 (source clip's pivot plant, clip 0.43-0.83 s) | 40.5 cm (10 fr; run loop already) | 58.6 cm (25 fr) |
| Skid clip on screen | f124-f144 (cut at 0.45 s by the 0.42 s timed exit) | f123-f169 (whole clip, 0.833 s) |
| Capsule vs stick during the clip | exit push to the stick from f132, walking mode from f139 | none: capsule on the clip root to f167 (peak 66.5 cm vs baked 66.9, end 42 vs 45) |
| Mesh root offset from actor in the pivot | 0.6-8.7 cm | 0.0-0.7 cm |

ASSERT (gate in pie_capture_move.py, SS_180_Turn* >= 0.8 on f117-f160, coverage 0.9): before FAIL 43%, after FAIL 89% (f117-f122 = the
0.08 s Skid blend-in plus one frame of anim latency). The gate was first written as f97-f140 without the PRE offset (before run), corrected to
f117-f160 before the after run.

Finding: the controller no longer moves or turns the body toward the stick during the skid, but the back foot still travels because the
clip itself moves it: in the source FBX (Desktop/Game Dev Stuff/SwordAndShield180Turn.UE.fbx = Mixamo "sword and shield 180 turn (2)") the
right foot slides ~120 cm low to the ground in frames 0-13 (the authored brake slide) and is fixed only in frames 13-25; in the in-game
SS_180_Turn_Root180 (root from our root_fix_skid_180.py) it still moves ~58 cm in that window while the capsule follows the clip root exactly,
so the residual is in that clip's root/turn, not the controller. Next: bake and play SS_180_Turn2_Src (owner's untouched root-motion clip)
or re-derive Root180's root so the pivot foot stays fixed.
