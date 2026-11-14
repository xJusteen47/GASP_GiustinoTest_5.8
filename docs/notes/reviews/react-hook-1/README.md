# react-hook-1: the victim reactions in game (2026-11-14)

How it was made: `Tools/GASNPPTest/pie_capture_react_ingame.py` (Play in L_Test_Hub, listen host and 2 clients, UE 5.9, t.OverrideFPS 60).
Client 1's pawn really hits the host pawn with the test move; nothing is played by the script. What the host shows comes from the game:
`UGSTN_ReactionAnimComponent` (DA_ReactionAnims) and the get-up abilities of KIT_Test_DefenceLanding.
Raw frames: `Saved/AnimReact/react-hook-1-a2` (not versioned). Views: `az180` behind the pawn, `az90` its right side. The cameras follow the pawn
only past a 120 cm dead zone.

| Sequence | What happens (60 fps video frames) | Files |
|---|---|---|
| hitstand | v10 attack, v23 Hitstun (HitStand), v60 back to the game pose | `hitstand.mp4`, `hitstand_az180_sheet.jpg`, `hitstand_az90_sheet.jpg` |
| knockdown_getup | v23 Tumble, v61 Knockdown (GroundHit, then LieUp after the bounce at v76), v111 jump: AM_React_GetUp v112-v137 | `knockdown_getup.*` |
| roll | the same landing, stick held toward the feet at v65: AM_React_Roll v84-v106 | `roll.*` |
| rollback | the same landing, stick held toward the head at v66: AM_React_RollBack v85-v107 | `rollback.*` |

Sheets show every second frame. The label on each cell is `v<frame> a<montage frame> <state>:<frames in it>`. `T` marks a press.

## Seen in the capture (for the review thread to check on the video)
- **The hit stand is short.** The HitStand pose shows for about 6 frames (v23 to ~v29); after that the pawn is back in idle while Hitstun lasts until v60.
  The dynamic montage is active for only one sampled frame (v23).
- **The rolls start in the air.** Both rolls start during the landing bounce (pawn z 132, the floor rest is 88), because the stick is held 4 frames after
  the knockdown. When the stick was held 50 frames later (attempt 1), no roll started: the roll is only taken at the end of the bounce.
- **The roll travels more than the data says.** The roll covers 129 cm (80 in the data) and the roll back 110 cm (65 in the data) while the stick is held.
  After the stick is released (2 frames after the get-up) the pawn keeps sliding about 45 cm (roll) and 90 cm (roll back).
  The held stick probably adds walking speed on top of the travel. Check this before tuning the travel keys.
- In the roll back the pawn turns (yaw 0 to -136) at the end, because of the held stick.
- **Framing is weak.** After the launch (about 220 cm) the side camera keeps the pawn at the edge of the frame. The behind view frames it better.
  I used both capture attempts, so I did not do a third one: the dead zone should be smaller (about 40 cm) next time.

## Attempts
1. `react-hook-1-a1`: `defence_setup(landing=True)` fell back to the runtime path on KIT_Test_Hits, so there was no roll back. The stick was held
   too late, so no roll started, and the roll back sequence began with the pawn still downed. The hit stand and the knockdown + neutral get-up
   were fine.
2. `react-hook-1-a2` (these files): the content kit KIT_Test_DefenceLanding is applied directly, the stick is held 4 frames into the knockdown,
   and the camera is closer (320 cm). All four sequences finished, `done.txt` = ok.
