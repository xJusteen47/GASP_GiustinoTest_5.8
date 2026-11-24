# Lanci 1A 2A: PIE on the desktop (2026-11-24, 15:16-15:53 UTC)

Worktree GASP_Giustino2_lanci, branch claude/project-thread-5yaz54. Rules: launch 750 (f9eb2c0d), balloon on (af47b80f).
Captures: L_Test_Hub, listen host + 2 clients, t.OverrideFPS 60. Scripts: `Tools/GASNPPTest/lanci_measure_boot.py` (sequences, per-frame
camera log, UI shots), `lanci_flight_report.py` (numbers below, full table in `pie_flight_table.md`). Video: `lanci_1a2a_desktop.mp4` (24 s).

## Main finding: launches lose all their horizontal speed (blocks the flight, zoom, arrow and wall checks)

On a client-controlled victim (the hub's dummy is player 2), every launch flies **straight up**: 0 cm/s horizontal on the first frame, from
0 % to 120 %, for the strong move, the dropkick and the dair. It is not the new flight or the balloon: the same with the balloon off and
with the old flight (KnockbackGravityScale 0, LaunchDeceleration 0). The launch vector is right. With `log LogGSTNKnockbackMove Verbose`
(`knockback_move_verbose.log`) the knockback move asks for `(2868 0 1656)` (strong at 100 %: K 3311 at 30 deg), and the Falling mode
has `(0 0 1593)` one frame later. So the Mover falling step on that pawn removes the horizontal part. A push back on the ground keeps it
(strong at 0 %: 580 cm/s, 1.39 m).
The wall run (`pie_wall_bounce.py`, victim = the host) keeps the horizontal part, but that test move only gives ~550 cm/s even at 700 %:
0.6 m and a knockdown, no wall contact, so there are no wall bounce numbers (`wall_bounce_capture.log`).
Suspects to check (not tested): the falling mode's air control/braking with no stick input on a remote client pawn (AirControl 0.85 from
the movement preset), or the hitstun movement lock. A memory note from 2026-11-16 says the same harness flew a dummy ~17 m, so this
changed after that date.

## Numbers (strong = GA_Playtest_Strong on KIT_Test_Medium; KIT_Test_Playtest has no Strong binding)

| Hit | Up speed cm/s | Rise | Horizontal | Flight to floor | Locked frames (hitstun/tumble) | Floor impact |
|---|---|---|---|---|---|---|
| Strong 0 % | push back 580 | 0 | 1.39 m | - | 39 | - |
| Strong 40 % | 1198 | 0.87 m | 0 | 0.57 s | 85 | -807 cm/s |
| Strong 80 % | 3852 (balloon) | 2.01 m | 0 | 0.58 s | 107 | -1232 cm/s |
| Strong 100 % | 5664 (balloon) | 2.74 m | 0 | 1.0 s with 2 bounces | 111 | -1393 -> +874 (f72), -769 -> +410 (f96) |
| Strong 120 % | 7800 (balloon) | 3.59 m | 0 | 0.63 s | 112 | -1500 cm/s (fall speed cap) |
| Strong 40 / 100 %, balloon off | 837 / 1593 | 0.76 / 2.63 m | 0 | 0.52 / 0.85 s | 82 / 126 | -733 / -1404 |
| Strong 40 / 100 %, old flight | 1235 / 5953 | 1.22 / 3.80 m | 0 | 0.72 / 0.88 s | 94 / 103 | -825 / -1463 |
| Dropkick 0 % (air hit*) | 645 | 0.43 m | 0 | 0.78 s | 35 | -1500 |
| Dair 0 % (air hit*) | 916 | 0.75 m | 0 | 0.65 s | 36 | -1440 |

*The hub's test pawn jumps ~9.3 m (JumpVelocity 1350 with gravity scale 1: the jump gravity attributes are not on it), so the dropkick
and the dair strike 3-6 m above a dummy on the floor. Two hits came from a dummy held next to the attacker until it is struck
(`pin_rel`): an air hit. The ground hop at 0 % could not be measured in this harness.

Balloon hitstun at 40 / 80 / 120 %: 85 / 107 / 112 locked frames (includes the hitstop). At 100 %, 111 with the balloon and 126 without.
Ground impacts above 1200 cm/s: 80 % (1232) and 100 % (1393, which bounced twice). At 120 % the dummy hit the floor at the 1500 fall speed
cap and did not bounce. At 80 % it hit at 1232 and did not bounce either: worth a check (FGSTN_GroundImpactRules).

## Camera and the three switches (strong at 100 %)

- Victim on screen: 315/315 frames in every run, but that is trivial while the launch has no horizontal part.
- Zoom: the host camera backs off from 678 to 1172 cm (1325 at 120 %). With `gstn.Launch.Debug.Zoom 0` it stays at 623-659 cm: works.
- Effects: the puff shows at the take-off with effects on (UI shot f51). With `gstn.Launch.Debug.Effects 0` it does not show (f50): works.
- Arrow: not verified. The other players are parked ahead in Z1, inside the frame, and the victim never leaves the screen, so no shot
  shows an arrow in either state. It needs a horizontal launch, or a player placed behind the camera.

## Notes
- UI shots: `shot showui` per frame shoots the whole editor window (960x640). The video crops the viewport (622x306, scaled x2).
- Tests after both rule changes: GSTNCombat.Core.Reaction + GSTNCombat.NP.Hits 118/118 OK.

ESITO: misure fatte. Lanci senza componente orizzontale su vittima client (causa nel falling mode di Mover, vettore giusto); muro e freccia non verificabili finché non è risolto.
SUBAGENTI: nessuno.
COSTO: ~1 h desktop (8 catture PIE, 6 headless), lock _ue59 15:16-15:53 UTC.
