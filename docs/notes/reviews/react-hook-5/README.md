# react-hook-5: the two hits from behind after the "back hit turn" fix (2026-11-14)

Made with `Tools/GASNPPTest/pie_capture_react_ingame.py` (Play in L_Test_Hub, listen host + 2 clients, UE 5.9, t.OverrideFPS 60),
`PIE_CAPTURE_SEQUENCES=back_hitstand,back_knockdown`, branch `claude/project-thread-a5idak` at **aa698b45** (fix attempt 2: the input's
orientation intent points at the hit turn), built in `_ue59`. Files as in react-hook-4: `<seq>.mp4` (60 fps), `<seq>_az45_sheet.jpg` /
`<seq>_az90_sheet.jpg` (every 2nd frame), `<seq>_frames.jsonl`, `capture.log`. Raw frames: `Saved/AnimReact/react-hook-5c`. One run, both hits landed.
Earlier runs: 070b562a (`Saved/AnimReact/react-hook-5`) and ebe6ab06 (`react-hook-5b`) gave the same yaw numbers, frame for frame.

## Facts from frames.jsonl (no verdict: the cloud review watches the video)
The attacker is at x=0 with yaw 0 (facing +X). The victim stands at +X with yaw 0, so its back is to the attacker. Facing the attacker means yaw 180.
frames.jsonl has no hitstop field. Below, "loc unchanged" (read from `loc`) marks the frames before the pawn first moves.

| | back_hitstand | back_knockdown |
|---|---|---|
| start distance | 194 cm | 183 cm |
| hit-1 | v21 game, yaw 0 | v22 game, yaw 0 |
| hit (first reaction frame) | v22 Hitstun, yaw 0 | v23 Tumble, yaw 0 |
| hit+1 .. hit+10 | v23-v32 Hitstun, yaw 0 | v24-v29 Tumble yaw 0; **v30-v33 Tumble yaw 180** |
| hitstop / no movement | v22-v58 Hitstun, yaw 0, loc never changes (no push) | v23-v29 Tumble, yaw 0, loc unchanged; v30 is the first moved frame and the first at 180 |
| rest of the reaction | - | v30-v60 Tumble 180, v61-v111 Knockdown 180, v112-v137 getup 180 |
| end of reaction -> end of capture | v59-v129 game, **yaw 0 throughout** | v138-v208 game, **yaw 180 throughout** |

- Editor log: `[HitTurn]` decided (turn=1, yaw ±180) and the pre-tick `SetFacing -180 ... effect=1` on the hit frame, in both sequences. There are no "no room" errors.
- back_hitstand: no turn on any frame. back_knockdown: a one-frame snap 0 -> 180 on the first frame the pawn moves (hit+7), then it holds 180 to the end.
