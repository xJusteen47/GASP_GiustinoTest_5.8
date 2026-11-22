# Dropkick tech bounce v1: frame index (video frames v, 60 fps, from 0)
Base = KIT_Test_Playtest as on disk (the owner's playtest kit), variant = KIT_Test_PlaytestTB (Tools/GAS/setup_dropkick_tech_bounce.py).
Captures: `Saved/DropkickBounce/{base_pt,variant3}/<seq>/frames.jsonl` (`Tools/GASNPPTest/pie_capture_timed.ps1 -Kit <kit> -Out <dir> -Sequences
dropkick_whiff,dropkick_hit,dropkick_low_whiff,dropkick_low_hit`). high = pressed rising at 150 cm (+5 f), low = pressed falling at 85 cm.
High jump: the variant's press is one video frame earlier, so its grids use `--vshift=-1` (same move frames).

| sequence | build | press | active (anim >= 15) | ground contact | hop peak (actor highest) | bounce clip on screen |
|---|---|---|---|---|---|---|
| dropkick_whiff (high) | base | v36 | v53 | v140 (Land section a59) | - | - |
| dropkick_whiff (high) | variant | v35 | v51 | v139 | v147 | v139-v150, GroundHit from v151 |
| dropkick_hit (high) | base | v36 | v53 | v142 | - | - |
| dropkick_hit (high) | variant | v35 | v51 | v141 | v149 | v141-v152 |
| dropkick_low_whiff | base | v121 | v137 | v138 | - | - |
| dropkick_low_whiff | variant | v121 | v137 | v138 | v146 | v138-v149 |
| dropkick_low_hit | base | v121 | v140 | v142 | - | - |
| dropkick_low_hit | variant | v121 | v141 | v142 | v149 | v142-v152 |

Hop (variant dropkick_whiff): actor z 92.3 at contact, 114.2 at the peak (+22 cm, track 25 cm), back on the floor at contact +12 (v151).
BASE today: the dropkick user is never downed (no KnockdownOnLanding window, no defence rules on the kit): the montage's Land section plays.
KNOWN GAME DEFECT (not pose): contact frame v139/v141/v138/v142: the body sits ~60 cm under the floor for ONE frame (actor 37 -> 92 cm while the
mesh offset of the dropkick Body Capsule lags a frame). Review the clip from contact +1.
Grids (base row on top): g1-g3 high whiff side/back/game, g4 high hit game, g5-g6 low whiff side/game, g7-g8 low hit game/side.
mp4 (7 views, v125-v169): dropkick_whiff_{base,variant3}.mp4, dropkick_low_hit_{base,variant3}.mp4.
Clip: 15 frames (r1 retime to the measured hop), arms crossed f0-f3, left uncrosses from f4, right from f6, peak f7-f8, lying = GroundHit f0 at f13-14
(the Knockdown rule's GroundHit takes over at the second touchdown, clip f11).
