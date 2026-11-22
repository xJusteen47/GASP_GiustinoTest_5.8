# react-hook-3: the victim reactions in game after the GroundBounce/Tech input fix (2026-11-14)

Made with `Tools/GASNPPTest/pie_capture_react_ingame.py` (Play in L_Test_Hub, listen host + 2 clients, UE 5.9, t.OverrideFPS 60), branch
`claude/project-thread-wkds3r` at `125897ce` (`Timeline.bIgnoreMoveInput = true` in UGSTN_GA_GroundBounce and UGSTN_GA_Tech), built in `_ue59`.
Same stage and views as react-hook-2: attacker in front, victim faces it (facing error 0 deg in all five), `az45` in front of the victim
(attacker hidden), `az90` its right side, other pawns hidden. Client 1 really hits the host; nothing is played by the script.
Files per sequence: `<seq>.mp4` (60 fps, az45 | az90 side by side, label `v<frame> a<montage frame> <state>`), `<seq>_az45_sheet.jpg` /
`<seq>_az90_sheet.jpg` (every 2nd frame), `<seq>_frames.jsonl`. `capture.log` = the driver log. Raw frames: `Saved/AnimReact/react-hook-3`.

## Roll back (the focus): FIXED in the data and on the sheet
- Stick held against the facing from v88 (KD v84 + 4). **Yaw stays 180 from v84 to v131** (react-hook-2: 180 -> 88 while lying).
- Get-up at v107 = KD+23 plays **AM_React_RollBack** (react-hook-2: AM_React_Roll). Facing 180 through the whole roll.
- Travel: x 472.3 -> 537.3 = **65 cm toward the head** (backward), starts v112 = KD+28, ends v130. Forward roll travels 80 cm:
  the RollBack keys are 15 cm shorter (anim data, not a bug of this fix).
- After the get-up (v130) the stick is still held until v133, then the pawn turns to it: v132 170, v138 87, v162 0 (normal walk turn).
- az90 sheet: v108-v120 the body rolls back over the head, crouches up at v122-v126, stands at v128.

## Other sequences (unchanged from react-hook-2)
| | hitstand | knockdown_getup | roll | roll_late |
|---|---|---|---|---|
| states | Hitstun v21-v77 | Tumble v22, KD v84, GetUp v135-v160 | KD v84, Roll v107-v129 | KD v83, Roll v135-v157 |
| yaw while lying | - | 180 constant | 180 constant | 180 constant |
| travel (+30 f) | - | - | 79.4 cm | 79.4 cm |
| bounce peak | - | 105.9 (+17.8) at KD+6 | KD+5 | KD+6 |
- Hitstand: dynamic montage restarts once (AnimMontage_0 -> _3 at v41), still the held HitStand pose (open point from react-hook-2).
- The forward rolls still start at KD+23 / KD+52 (roll_late): the input fix did not change their timing.
