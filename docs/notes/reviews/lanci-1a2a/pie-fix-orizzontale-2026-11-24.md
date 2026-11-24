# Lanci: check of the horizontal launch fix (mixer), desktop 2026-11-24 16:10-17:00 UTC

Worktree GASP_Giustino2_lanci, branch `claude/project-thread-kyph4p` (2e93e149: `UGSTN_MovementMixer` installed by
`UGSTN_CombatSimComponent::BindAirTurn`). Same setup as `pie-desktop-2026-11-24.md`: L_Test_Hub, listen host + 2 clients,
t.OverrideFPS 60, strong = GA_Playtest_Strong on KIT_Test_Medium, `log LogGSTNKnockbackMove Verbose`, rules 750 + balloon on.

## Build and tests
- Build (Run-Timed -Preset Build): OK, 44 s, only GSTNCombatNP recompiled, no new warning.
- Tests: GSTNCombat.NP.Hits (88, includes Launch.MixerKeepsVelocityOverride) + Core.Reaction (31) + Core.AirMomentum (6) +
  NP.AirTurn (3) + GSTNInput (139) = **267/267 PASS**.

## The fix works: the horizontal part survives the launch frame
Before: knockback move `want (2868 0 1656)`, Falling one frame later `have (0 0 1593)`. Now (player victim, `cap_fix_player2` UE log):

```
[Knockback] ..._C_0 frame 2469 elapsed 1 mode Falling want (1425 0 834) have (1425 0 834)
[Knockback] ..._C_0 frame 2470 elapsed 2 mode Falling want (1425 0 834) have (1420 0 753)
[Knockback] ..._C_0 frame 3690 elapsed 17 mode Falling want (3304 0 1908) have (3304 0 -866)
```
The horizontal speed stays and fades by ~5 cm/s per frame; only gravity eats the vertical. The victim's snapshot at 100 %
(frames.jsonl): balloon burst 11879 cm/s on the take-off frame, down to the base 2868 cm/s in 13 frames, then ~2860 to the floor.

## Table (strong, 0-120 %)
`LANCI_VICTIM=player` (new in `lanci_measure_boot.py`): the victim is client player P257's pawn (with its locomotion profile, the
case of the bug). Dummy = the hub's AI dummy BP_GASNPP_TestCharacter_C_3 (the victim of the earlier report). Horizontal = distance at
the first floor contact after the peak; far = farthest point (after floor bounces and slide). Speeds are the first 20 frames' peaks.

| % | victim | up cm/s | horiz. cm/s | rise | horizontal at landing | farthest | flight | floor impact | bounces | locked frames |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | player | push 0 | 580 | 0 | 0.09 m (push back) | 1.39 m | - | - | 0 | 38 |
| 0 | dummy | push 0 | 580 | 0 | 0 (push back) | 1.39 m | - | - | 0 | 39 |
| 40 | player | 1198 | 2338 | 0.87 m | **6.07 m** | 6.07 m | 0.53 s | -881 | 0 | 82 |
| 40 | dummy | 1198 | 2338 | 0.87 m | **6.07 m** | 6.07 m | 0.55 s | -881 | 0 | 83 |
| 80 | player | 3852 | 7951 | 2.01 m | **14.9 m** | 19.7 m | 0.58 s | -1309 | 0 | 106 |
| 80 | dummy | 2889 | 7221 | 2.01 m | **19.7 m** | 19.7 m | 0.93 s | -788 | 1 | 106 |
| 100 | player | 5664 | 11879 | 2.74 m | **20.6 m** | 26.9 m | 0.60 s | -1537 | 1 | 109 |
| 100 | dummy | 5664 | 11879 | 2.74 m | **20.6 m** | 26.9 m | 0.58 s | -1537 | 1 | 108 |
| 120 | player | 7800 | 16555 | 3.59 m | **27.8 m** | 35.3 m | 0.62 s | -1588 | 0 | 111 |
| 120 | dummy | 5570 | 14812 | 3.59 m | **27.8 m** | 35.3 m | 0.62 s | -1588 | 0 | 111 |

Before the fix: 0 m horizontal at every percent, same rise (0.87 / 2.01 / 2.74 / 3.59 m): the vertical part did not change.
Player and dummy now fly the same rise and distance (the 80 % and 120 % first-frame speed peaks differ, not looked into; the 80 %
dummy landed on its first floor bounce later). Victim on screen 314-315/315 frames, the launch zoom backs the camera off to 1080-1155 cm.

## Walls
- `pie_wall_bounce.py` (WALL_SKIP_EDIT=1, nothing saved): still no wall contact. It is not the bug: that script sets its own test rules
  (`g.set_tuning(launch_threshold=100, ...)`, `hit_rules_setup`) whose test move flies 475 cm/s at 0, 250, 400 and 700 %. It cannot
  measure the standard launch.
- So `lanci_measure_boot.py` got `LANCI_WALL=<cm>`: an AGSTN_TestSurface (leaf Wall) across the launch line, standard rules, one row per
  frame in `lanci_wall.jsonl`. Run with the face 700 cm ahead of the host, player victim:
  - **40 %: hard bounce, works.** 1498 cm/s into the face, impact outcome bounce (speed 1466), back out at -880 cm/s, lands 0.7 m off the wall.
  - **100 %: the bounce is resolved but the victim does not come off the wall.** It reaches the face 4 frames after the take-off
    (9631 cm/s), impact = bounce at 8672 cm/s, the rebound move asks `want (-5203 0 1203)` but Falling `have (0 0 1289)`: it stays
    31 cm from the face with 0 horizontal speed, slides down the wall and bounces on the floor (+874). A second wall bounce (3748 cm/s,
    want (-2249 0 -328)) is lost the same way. The log shows the launch's own knockback move (want (2868 0 1656), elapsed 16) still active
    next to the rebound one: my guess is that the original launch's velocity override (into the wall) still wins in the mix, and the wall
    blocks it. Not checked further: it needs its own diagnosis (which layered move the mixer keeps when two velocity overrides run).

## Surprises and what changed in the air
- The air momentum and the air turn now leave alone every frame whose layered moves override the velocity, not only knockbacks: in
  this run I measured only the knockback. Dodges and aerial lunges in the air were not captured (no sequence for them in this harness);
  worth a look in the next playtest.
- The 100 % launch flies 20.6 m to the first landing (27 m with the slide), 120 % 27.8 m: with the horizontal part back the balloon
  burst (11879 cm/s for a few frames) carries far. The numbers are the rules' (750 + balloon); whether they feel right is the owner's call.
- Arrow: not re-checked (needs a player leaving the host's frame; the victim stayed on screen 315/315 thanks to the zoom).

## Files
- Video: `lanci_fix_player_p100.mp4` (strong at 100 % on the player, 2 s real time, all views + state) and `..._slow.mp4` (frames 30-80 at 0.25x).
- Captures (not in git): `Saved/Lanci/fix_player2`, `fix_dummy`, `fix_wall` (pie_wall_bounce), `fix_wall_strong`; UE logs
  `Saved/Logs/ci/cap_fix_*.log.ue.log`.
- Tool: `Tools/GASNPPTest/lanci_measure_boot.py` (+ `LANCI_VICTIM=player`, `LANCI_WALL=<cm>`).

ESITO: fix verificato in gioco: i lanci su un giocatore hanno di nuovo la componente orizzontale (6 / 15-20 / 21 / 28 m a 40 / 80 / 100 / 120 %),
come sul dummy. Build OK, test 267/267. Rimbalzo a muro: OK a 40 %, a 100 % il rimbalzo viene deciso ma la vittima resta attaccata al muro (da diagnosticare).
SUBAGENTI: nessuno.
COSTO: ~50 min desktop (build, test, 5 catture PIE), niente _ue59.
