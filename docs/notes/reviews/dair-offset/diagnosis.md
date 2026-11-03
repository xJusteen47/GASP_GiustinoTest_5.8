# Dair one-frame offset: diagnosis (2026-11-03, before any change)
1. Author and build are aligned: built AS_OverlordDAirV4_Build(_VarI) foot_l z = 59.2 @14, -2.8 @15, -3.8 @16 (author 58.9/-3.0/-4.0); card key 15 lands on anim 15.
2. The editor-path capture (ingame_judge, base v4 raw) is aligned too: image motion correlates with pose motion at lag 0 (0.79 vs 0.26), feet drop at 14->15.
3. The standalone capture (`animpipe render --standalone`, varI_az90 raw) is one frame late: correlation 0.85 at lag 1 vs 0.31 at lag 0, feet drop at 15->16 => pipeline offset in standalone_capture.py (one set_frame call, no wait for the scrub to land), not in the card. Numbers: lag_test.txt.

## Result (case: pipeline offset, fixed at the source)
- Fix: `Tools/AnimPipeline/standalone_capture.py` re-applies set_frame each tick until hand_r is stable (as ingame_judge.grab does), then settles.
- Re-rendered standalone az 90, base v4 and variant I (card from 057bd21): lag-0 correlation 0.87 / 0.85 (lag-1 0.30 / 0.07);
  feet drop 228 -> 285 px at 14 -> 15 in both (lag_test_after.txt). Stomp key 15 now draws on anim 15.
- Card, author take and builds untouched: keys 37/42, FAF 37 and anim_active [15,17] unchanged by construction.
- Owner's anim_active [15,17] now matches the image: no card change needed. Old standalone videos (varG/varI az*) are one frame late.
