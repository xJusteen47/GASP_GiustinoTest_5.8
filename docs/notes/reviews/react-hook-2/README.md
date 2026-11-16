# react-hook-2: the victim reactions in game after the react-hook-1 fixes (2026-11-14, round 2)

Made with `Tools/GASNPPTest/pie_capture_react_ingame.py` (Play in L_Test_Hub, listen host + 2 clients, UE 5.9, t.OverrideFPS 60), branch
`claude/project-thread-0tmywc` at `7ed7341d` (facing held while downed or in a reaction; capture steers the victim 1.5 s after its teleport),
built in `_ue59`; content from `setup_reaction_anims.py` (round 1). Default FACE_SECONDS. Client 1 really hits the host; nothing is played by the script.
Views: `az45` in front of the victim (attacker hidden), `az90` its right side, dead zone 40 cm.
Files per sequence: `<seq>.mp4` (60 fps), `<seq>_az45_sheet.jpg` / `<seq>_az90_sheet.jpg` (every 2nd frame, label `v<frame> a<montage frame> <state>`),
`<seq>_frames.jsonl` (per frame: seg = host combat state, segf, montage [name, frame], loc, yaw). `capture.log` = the driver log. Raw frames:
`Saved/AnimReact/react-hook-2b` (not versioned). Numbers below are from frames.jsonl (60 fps video frames, v; KD = Knockdown start).

## Facing (capture.log, error 0 = victim faces the attacker)
| hitstand | knockdown_getup | roll | rollback | roll_late |
|---|---|---|---|---|
| 0 deg, 97 cm | 0 deg, 97 cm | 0 deg, 90 cm | 0 deg, 97 cm | 0 deg, 97 cm |

## Hit stand
- Hitstun v21-v77 = **57 frames; a HitStand montage is active on all 57**. Back to the game pose at v78.
- Montage frame: a0 v21, a1 v22, then **a2 until v77**: the pose is held at clip frame 2, the clip does not play further. The dynamic montage
  restarts once (AnimMontage_0 -> AnimMontage_3 at ~v45), still at a2. Check on the video.

## Knockdown bounce (actor loc z; rest 88.1)
- Knockdown starts at z 92.9, peaks at **105.9 = +17.8 cm** 5-6 frames in, back to rest after **11-12 frames** (identical in all four).
  Target ~15 cm / ~13 frames. The 85 cm entry drop is a mesh offset and does not show in loc.
- knockdown_getup: Tumble v21-v83, KD v84 (GroundHit v84-v98, LieUp v99-v134), AM_React_GetUp v135-v160, game pose v161. Yaw 180 throughout.

## Rolls (stick held 4 frames into the knockdown; roll_late after 50)
| | roll | rollback | roll_late |
|---|---|---|---|
| KD start | v84 | v83 | v84 |
| get-up ability starts | v107 = KD+23 | v106 = KD+23 | v136 = KD+52 |
| pawn starts moving | v112 = KD+28 | v112 = KD+29 | v141 = KD+57 |
| montage played | AM_React_Roll | **AM_React_Roll (should be RollBack)** | AM_React_Roll |
| roll keys travel (steps until the stop) | 80 cm | 80 cm | 80 cm |
| travel start -> +30 frames | 93.4 cm | 81.6 cm | 98.0 cm |
| yaw change while lying | 0 | **92 deg** (180 -> 88) | 0 |
| yaw change during the get-up | 0 | 16 deg | 0 |

- The 30-frame travel includes a slow drift after the get-up (0.1-2.3 cm/frame from ~v133 with the stick still held): the stick walks the pawn
  once the get-up ends (about 13-18 cm).
- **rollback still wrong (fix attempt 2 of 2 failed)**: the facing is now held during the bounce (yaw 180 from v83 to v96) but the pawn turns
  as soon as the bounce ends: v97 176, v100 152, v103 121, v105 88 (Knockdown, LieUp, still lying, stick held against the facing since v87).
  At the get-up (v106) the stick is no longer behind it, so the forward Roll is picked. The facing hold apparently covers the ground bounce
  but not the rest of the downed (lying) state. After the get-up it turns on to yaw 0 (the stick direction).
