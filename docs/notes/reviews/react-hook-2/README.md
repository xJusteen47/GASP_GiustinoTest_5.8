# react-hook-2: the victim reactions in game after the react-hook-1 fixes (2026-11-14)

Made with `Tools/GASNPPTest/pie_capture_react_ingame.py` (Play in L_Test_Hub, listen host + 2 clients, UE 5.9, t.OverrideFPS 60), branch
`claude/project-thread-0tmywc` built in `_ue59` after `setup_reaction_anims.py` (headless: 4 rules, get-ups written). Client 1 really hits the host;
nothing is played by the script. Views: `az45` in front of the victim (attacker hidden), `az90` its right side, dead zone 40 cm.
Files per sequence: `<seq>.mp4` (60 fps), `<seq>_az45_sheet.jpg` / `<seq>_az90_sheet.jpg` (every 2nd frame, label `v<frame> a<montage frame> <state>`),
`<seq>_frames.jsonl` (per frame: seg = host combat state, segf, montage [name, frame], loc, yaw). `capture.log` = the driver log. Raw frames:
`Saved/AnimReact/react-hook-2` (not versioned). Numbers below are from frames.jsonl (60 fps video frames, v).

## Facing (capture.log, error 0 = victim faces the attacker)
| hitstand | knockdown_getup | roll | rollback | roll_late |
|---|---|---|---|---|
| **90 deg**, 283 cm | **30 deg**, 141 cm | 9 deg, 101 cm | 6 deg, 106 cm | 7 deg, 94 cm |

hitstand and knockdown_getup did NOT face the attacker. Retry with FACE_SECONDS 0.5 (new env `PIE_CAPTURE_FACE_SECONDS`, attempt `react-hook-2-a3`):
90 / 44 deg, no better: on the first sequences the pre-walk steers the host along -Y (yaw -90) instead of toward the attacker, so a longer walk
does not help. The fix is in the placement/steer (`_seq_prep`, `g._steer_value`), not the time. The files here are attempt 1 (all 5 sequences).

## Hit stand
- Hitstun v33-v69 = **37 frames; the HitStand montage is active on all 37** (react-hook-1: 1 frame). Back to the game pose at v70.
- Montage frame: a0 v33, a1 v34, then **a2 from v35 to v69**: the pose is held at clip frame 2, the clip does not play further. Check on the video.

## Knockdown bounce (actor loc z; rest 88.1)
- Knockdown starts at z 92.9 (rollback 97.0), peaks at **105.9 = +17.8 cm** 5-6 frames in, back to rest after **11-12 frames** (all four sequences
  identical). Target was ~15 cm / ~13 frames (react-hook-1: 46 cm / 38 f). The 85 cm entry drop is a mesh offset and does not show in loc.
- Montages: Tumble clip, then GroundHit (15 frames), then LieUp chained; knockdown_getup: AM_React_GetUp v127-v151, game pose at v153.

## Rolls (stick held 4 frames into the knockdown; roll_late after 50)
| | roll | rollback | roll_late |
|---|---|---|---|
| Knockdown start | v84 | v84 | v84 |
| get-up ability starts (montage) | v107 = KD+23 | v107 = KD+23 | v136 = KD+52 |
| pawn starts moving | v113 = KD+29 | v112 = KD+28 | v142 = KD+58 |
| montage played | AM_React_Roll | **AM_React_Roll** (not RollBack) | AM_React_Roll |
| travel of the roll keys (steps until the stop) | 80 cm | 80 cm | 80 cm |
| travel start -> 30 frames later | 94.4 cm | 82.7 cm | 94.4 cm |
| yaw change start -> +30 | 0 deg | 0 during the roll, **79 deg** after it | 0 deg |

- Rolls start only after the bounce (all on the ground): fixed. The roll keys give exactly 80 cm (4 x 14 cm, then 4 cm steps, then a hard stop).
- The 30-frame figures include **a slow drift after the get-up** (0.3-2 cm/frame from ~v133 when the game pose returns with the stick still held,
  about 14-18 cm in total): the stick walks the pawn once the get-up ends.
- **rollback is wrong**: the stick held against the facing TURNS the lying pawn (yaw 174 -> -93 between v84 and v106, Knockdown, no ability yet),
  so when the get-up starts the stick is no longer behind it and the forward Roll is picked; it travels 80 cm toward its new facing (-Y), not 65 cm
  toward the head. After the get-up it keeps turning to -6 deg. The facing should be locked while downed (not only during the get-up timeline).
