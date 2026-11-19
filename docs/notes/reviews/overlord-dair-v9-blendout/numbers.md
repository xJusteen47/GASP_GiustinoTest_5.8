# Down-air v9, whiff: montage blend-out test (2026-11-14, UE 5.9, `L_Test_Hub`, kit `KIT_Test_OverlordDAirV9`, `PIE_CAPTURE_SEQUENCES=dair_whiff`)

FAF = anim frame 42 (card `overlord_dair_v9.json`); clip 43 frames (0-42) at 60 fps; montage play length 0.700 s (42/60), play rate 62.5/60. Timeline ends (inTimeline false) at v79 (base), v80 (att1), v79 (att2).

## Settings
| run | blend_out time | blend_out_trigger_time | montage leaves (blend starts) | raw frames |
|---|---|---|---|---|
| base (as committed) | 8 frames (0.1333 s) | -1 (auto, starts 8 frames before the end) | v71 (a34.3) | `Saved/AnimDairBlendOut/base` |
| attempt 1 | 8 frames | 0 (plays to the last frame, then blends) | v80 (a42) | `.../variant` |
| attempt 2 | 16 frames (0.2667 s) | -1 (auto) | not logged | `.../variant2` |

Asset edits were made on `AM_OverlordDAirV9` from editor Python (the pipeline take is present but a refresh was not needed) and then put back to base values (8 frames, -1). `setup_overlord_dair_kit.py` gets `OVERLORD_DAIR_BLEND_OUT_TRIGGER` (default -1 = unchanged).

## Method
Same idea as the v9 review: character pixels that changed between two consecutive video frames, threshold 20, crop (105,60,375,420) of the 480 px views for side (az270) and behind (az180); the game view is the whole 480 px frame. Character = grey, low-saturation pixels (the floor is blue); this is my own mask, so absolute values are not the review.md numbers (review: 4769 side at v70-71 in the 5.8 capture; the 5.9 base shows its biggest step elsewhere, see below). Compare runs only with each other.

## Changed px per frame (frame N vs N-1)
| v | base side | base behind | base game | att1 side | att1 behind | att1 game | att2 side | att2 behind | att2 game |
|---|---|---|---|---|---|---|---|---|---|
| v64 | 275 | 205 | 1313 | 274 | 241 | 1286 | 359 | 327 | 1098 |
| v65 | 713 | 522 | 670 | 306 | 241 | 1032 | 1346 | 1134 | 564 |
| v66 | 1556 | 1031 | 477 | 1082 | 705 | 522 | 2315 | 1821 | 806 |
| v67 | 2317 | 1643 | 600 | 2966 | 2364 | 562 | 2981 | 2321 | 890 |
| v68 | 3162 | 2152 | 772 | 3167 | 2300 | 715 | 3611 | 2501 | 833 |
| v69 | 3970 | 2758 | 807 | 3884 | 3046 | 799 | 3911 | 2539 | 712 |
| v70 | 3988 | 2605 | 922 | 4546 | 3043 | 939 | 3272 | 2154 | 644 |
| v71 | 3855 | 2133 | 783 | 4438 | 2577 | 843 | 2543 | 1799 | 827 |
| v72 | 2632 | 1654 | 643 | 3898 | 2081 | 735 | 1843 | 1617 | 1095 |
| v73 | 2303 | 1552 | 1243 | 2757 | 1594 | 576 | 1976 | 1732 | 1315 |
| v74 | 3586 | 1969 | 1914 | 1736 | 1268 | 1182 | 1800 | 1787 | 1819 |
| v75 | 4676 | 2510 | 2197 | 2978 | 1476 | 1149 | 2015 | 1873 | 2075 |
| v76 | 4109 | 2491 | 2427 | 4288 | 2138 | 1749 | 1857 | 2054 | 2489 |
| v77 | 3513 | 2283 | 2098 | 2699 | 1548 | 1609 | 1922 | 2167 | 2753 |
| v78 | 4833 | 4309 | 2432 | 1967 | 1363 | 1700 | 1804 | 2458 | 3041 |
| v79 | 2150 | 2293 | 2134 | 1629 | 1346 | 1771 | 3824 | 6559 | 3151 |
| v80 | 2060 | 2175 | 2501 | 7217 | 4975 | 3768 | 1394 | 2399 | 3416 |

## Max in v69-v77 (target: no frame above 2400 side)
| run | side | behind | game |
|---|---|---|---|
| base | 4676 (v75) | 2758 (v69) | 2427 (v76) |
| att1 | 4546 (v70) | 3046 (v69) | 1749 (v76) |
| att2 | 3911 (v69) | 2539 (v69) | 2753 (v77) |

## Max over the whole recovery v64-v90 (where the biggest step really is)
| run | side | behind | game |
|---|---|---|---|
| base | 4833 (v78) | 4309 (v78) | 4630 (v90) |
| att1 | 7217 (v80) | 4975 (v80) | 3768 (v80) |
| att2 | 3911 (v69) | 6559 (v79) | 3760 (v84) |

## Verdict: target NOT reached; the cause is not the montage blend-out
- Base (5.9): in v69-77 side max is 4676 (v75), 3 views show one step at v78 (side 4833, behind 4309, game 2432 flat), the frame before the timeline ends (inTimeline false from v79). The montage already left at v71.
- Attempt 1 (blend starts at the FAF): v69-77 is quieter behind and in game (max behind 3046, game 1749) but side still has 4546 at v70, and at v80-81, the first frames after the timeline ends, the legs jump: side 7217 / 6974, behind 4975 / 4810, game 3768 / 3655. Worse than base.
- Attempt 2 (16 frame blend, auto start): v69-77 side max 3911 (v69), one step at v79 = the first frame without timeline: side 3824, behind 6559, game flat.
- In all three runs the one-frame leg jump (legs from tucked, straight down to seated with knees forward, see grids) follows the END OF THE TIMELINE (ability end at FAF), 1-2 video frames, not the start of the montage blend-out: moving the blend start from v71 to v80 moved the jump from v78 to v80-81. So the pose change comes from the game when the ability ends (animation blueprint state or leg/foot logic reacting to the combat state/tag leaving), which is outside montage data. Not touched (Anim BP is out of scope).
- The 2400 target is therefore not reachable by montage settings; next step belongs to the Anim BP / ability-end side: find what changes when `inTimeline` goes false in air (a layered leg pose or fall state switching on).

Files: `dair_whiff_base.mp4`, `dair_whiff_variant.mp4` (attempt 1), `dair_whiff_variant_slow.mp4` (0.25x), `dair_whiff_variant2.mp4` (attempt 2), `cmp_whiff_side.jpg`, `cmp_whiff_game.jpg`, `cmp_whiff_behind.jpg` (base on top, attempt 1 below, v64-v81; attempt 2 has no grid).