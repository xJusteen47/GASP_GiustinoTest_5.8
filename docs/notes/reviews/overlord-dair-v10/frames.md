# Overlord dair V10: frame index of the captures (video frames v, 60 fps, from 0)

Base = KIT_Test_OverlordDAirV9 (on-disk AM_OverlordDAirV9 with the owner's local edit), variant = KIT_Test_OverlordDAirV10. Captures: `Saved/DairV10/{base,variant}/<seq>/frames.jsonl`
(`Tools/GASNPPTest/pie_capture_timed.ps1 -Kit <kit> -Out <dir> -Sequences dair_whiff,dair_hit,dair_land`). The press can fall 1-2 video frames apart, so the grids align
the variant on its own charge start (`pie_capture_pair.py --vshift`). Anim frames from 0: charge 0-12, active 15-17, FAF 42.

| sequence | build | charge start (anim 0) | hit/active (anim >= 15) | ground contact (h <= 2 cm) | landing lag clip starts | last timeline frame (FAF side) | last move-montage frame |
|---|---|---|---|---|---|---|---|
| dair_whiff | base V9 | v38 | v53 | v84 | - | v78 | v70 |
| dair_whiff | V10 | v40 | v55 | v86 | - | v80 | v72 |
| dair_hit | base V9 | v74 | v89 | v86 | - | v114 | v106 |
| dair_hit | V10 | v76 | vNone | v86 | v87 | v101 | v97 |
| dair_land | base V9 | v76 | v90 | v85 | - | v116 | v108 |
| dair_land | V10 | v75 | vNone | v84 | v88 | v102 | v98 |

Grids (base row on top, V10 below, same move frames): g1-g3 whiff charge side/back/game (charge start -2 .. +15), g4-g5 hit game/side (+10 .. +27),
g6-g8 landing case side/game/back (-2 .. +44, every 2nd frame). mp4: `<seq>_{base,variant}.mp4`, 7 views, v30-v140, 60 fps.
Numbers (head height above the root, clip, anim 0-11): V9 124.6-149.7 cm, V10 111.6-114.7 cm; fall pose before the move 110 cm.
Source: `Saved/DairV10/head_curve.json` (`python Saved/AnimR2/run_in_editor.py Saved/DairV10/head_curve.py`).
