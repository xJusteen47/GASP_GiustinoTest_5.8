# react-hook-4: victim reactions in game, front knockdown + the two hits from behind (2026-11-14)

Made with `Tools/GASNPPTest/pie_capture_react_ingame.py` (Play in L_Test_Hub, listen host + 2 clients, UE 5.9, t.OverrideFPS 60), branch
`claude/project-thread-7bhwol` at `1b641dca` (c709fbc3 + combat BP CP1), built in `_ue59`. Same stage, views and file layout as react-hook-3:
`<seq>.mp4` (60 fps, az45 | az90), `<seq>_az45_sheet.jpg` / `<seq>_az90_sheet.jpg` (every 2nd frame), `<seq>_frames.jsonl`, `capture.log`.
Raw frames: `Saved/AnimReact/react-hook-4` (knockdown_getup) and `Saved/AnimReact/react-hook-4-back` (back_*).
Desktop under load (Fortnite running): run 1 failed (host pawn not ready), run 2 OK for knockdown_getup, back_* missed (victim 320 cm away);
back_* re-shot after the behind walk was made to stop on distance + heading (script change in the same commit).

## Facts from the data (no verdict: the cloud review watches the video)
| | knockdown_getup | back_hitstand | back_knockdown |
|---|---|---|---|
| start | 90 cm, face to face | 183 cm, back to attacker | 194 cm, back to attacker |
| states | Tumble v24, KD v86, getup v137-v162, game v163 | Hitstun v24-v59, game v60 | Tumble v24, KD v62, getup v113-v138, game v139 |
| yaw | -180 then 180 (same heading) | **0 the whole time (no turn)** | **0 -> 180 between v30 and v34** (turn in tumble), 180 while lying, 2 at v163 |
- Points for the reviewer: back_hitstand never turns back along the knockback (yaw 0 throughout); back_knockdown turns in about 4 frames mid-tumble.
- knockdown_getup started at 90 cm (react-hook-3: same script, the 0.3 s face walk is wall time, longer under load); the hit still lands.
