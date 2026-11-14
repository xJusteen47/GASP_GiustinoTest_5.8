# react-hook-5: the two hits from behind after the "back hit turn" fix (2026-11-14)

Made with `Tools/GASNPPTest/pie_capture_react_ingame.py` (Play in L_Test_Hub, listen host + 2 clients, UE 5.9, t.OverrideFPS 60),
`PIE_CAPTURE_SEQUENCES=back_hitstand,back_knockdown`, branch `claude/project-thread-a5idak` at `070b562a` (8952dd93 fix + a test compile fix), built in `_ue59`.
Files as in react-hook-4: `<seq>.mp4` (60 fps), `<seq>_az45_sheet.jpg` / `<seq>_az90_sheet.jpg` (every 2nd frame), `<seq>_frames.jsonl`, `capture.log`.
Raw frames: `Saved/AnimReact/react-hook-5`. One run, both hits landed (no re-run).

## Facts from frames.jsonl (no verdict: the cloud review watches the video)
Attacker at x=0, yaw 0 (facing +X); the victim stands in front of it at +X with yaw 0, so its back is to the attacker. Facing the attacker = yaw 180.
frames.jsonl has no hitstop field; "loc unchanged" below is read from `loc`.

| | back_hitstand | back_knockdown |
|---|---|---|
| frame before the hit | v22: game, yaw 0 | v22: game, yaw 0 |
| start distance | 194 cm | 206 cm |
| first reaction frame | v23: Hitstun, yaw 0 | v23: Tumble, yaw 0 |
| hit / hitstop window | v23-v58 Hitstun, yaw 0, loc unchanged (no push) | v23-v30 Tumble, yaw 0, loc unchanged; **v31 yaw 180** (first frame that moves) |
| rest of the reaction | - | v31-v61 Tumble 180, v62-v112 Knockdown 180, v113-v138 getup 180 |
| end of reaction -> end of capture | v59-v129 game, **yaw 0 throughout** (never faces the attacker) | v139-v209 game, **yaw 180 throughout** (stays facing the attacker) |

- back_hitstand: no turn at any frame; position never changes.
- back_knockdown: the turn is a one-frame snap 0 -> 180 at v31, 8 frames after the first Tumble frame (react-hook-4: between v30 and v34);
  after the get-up it holds 180 to the end (react-hook-4: back to 2 at v163). Final loc x=427.6.
