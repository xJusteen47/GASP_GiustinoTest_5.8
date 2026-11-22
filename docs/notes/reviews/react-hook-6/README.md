# react-hook-6: the two hits from behind after the hit-turn fix (2026-11-14)

Made with `Tools/GASNPPTest/pie_capture_react_ingame.py` (Play in L_Test_Hub, listen host + 2 clients, UE 5.9, t.OverrideFPS 60), branch
`claude/project-thread-a5idak` at **08338b8d** (the SetFacing effect turns the capsule too), built in `_ue59`. Editor windows on virtual desktop 2.
Files as in react-hook-5: `<seq>.mp4` (60 fps), `<seq>_az45_sheet.jpg` / `<seq>_az90_sheet.jpg` (every 2nd frame), `<seq>_frames.jsonl`,
`capture_<seq>.log`. Two runs: back_hitstand (`Saved/AnimReact/react-hook-6`) and back_knockdown alone (`react-hook-6k`): in the first run the
knockdown came after the hitstand, the victim now started it facing the attacker, the walk-away placement ended at 382 cm and the hit missed.

## Facts from frames.jsonl (no verdict: the cloud review watches the video)
Attacker at x=0, yaw 0. Victim at +X, yaw 0 (its back to the attacker); facing the attacker means yaw 180.
The az45/az90 cameras keep the azimuth of the pawn's yaw at the sequence start, so after the turn they show the other side of the pawn.

| | back_hitstand | back_knockdown |
|---|---|---|
| start distance | 206 cm | 194 cm |
| hit-1 | v35 game, yaw 0 | v27 game, yaw 0 |
| hit (first reaction frame) | **v36 Hitstun, yaw 180** | **v28 Tumble, yaw 180** |
| frozen / no movement | v36-v71 Hitstun 180, loc never changes | v28-v35 Tumble 180, loc unchanged; v36 first moved frame |
| rest of the reaction | - | Tumble 180 to v66, Knockdown 180 to v117, get-up 180 |
| end of reaction -> end of capture | v72-v142 game, yaw 180 throughout | v144-v214 game, yaw 180 throughout |

react-hook-5 for comparison: hitstand never turned; knockdown turned only at hit+7 (first moved frame).
