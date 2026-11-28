# Dash durante la corsa: build, test e prova in gioco (2026-11-28, desktop, _ue59)

Branch `claude/project-thread-w2t0kg` (fbb01fe1, sopra main 82dfd88d): BoostMode PerStage di default, bAllowBoost per stadio, DashSpeedMode Momentum (x1.2, tetto 1000) o Cruise.

- Build: `Tools/CI/Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build.log` -> ESITO OK, 115 s, solo warning di deprecazione già presenti.
- Test: `-Preset Test -Tests "GSTNCombat.Core.RunStages+GSTNCombat.NP.Run"`, `Check-TestLog.ps1` -> PASS 20/20 (DashPerStage compreso), log `Saved/Logs/ci/test.log.ue.log`.
- Aggiunto in questo giro: `dashTop` nella vista run del test component (GSTN_GASNPPTestComponent.cpp); pie_capture_move.py: `boost` accetta una lista, `boost_stage` (stadio, ritardo), riga `vel`, sequenze `mv_dash_walk/45/60`, `mv_dash_sprint`, `mv_dash_top`; `Tools/GASNPPTest/dash_rows.py` legge le righe.
- In gioco (KIT_Test_Playtest, 1 giocatore, `pie_capture_timed.ps1`, catture `Saved/DashStage/cap1|cap2`, numeri in `catture/dash/speeds_cap1|2.txt`):
  - (b) corsa fino allo stadio 3 (indice 2, raggiunto a v341), L3 a v371: lo stadio resta 3, dashTop 1000, velocità 850 -> 999 dopo 8 frame -> di nuovo 850 dopo ~40 frame.
  - L3 allo stadio 2 (cap1, v243): dashTop 586 (488 x 1.2) sotto la crociera 610: picco 610, nessun guadagno (come previsto).
  - (a) camminata lenta: stick 0.45 (141 cm/s) e 0.6 (230 cm/s) per 1.5 s, poi L3: il dash parte (stadio 1 -> 2, burst), ma la velocità sale solo a 183 e 299: la camminata analogica limita la velocità con l'inclinazione dello stick, non con la crociera dello stadio.
  - Stick 0.3 (66 cm/s): L3 visto (boostNow) ma nessun dash: sotto StickDeadzone 0.25 (dopo la dead zone del giocatore) la logica conta lo stick come rilasciato e torna prima del boost.
- Video 1x (vista laterale + camera di gioco): `catture/dash/dash_top_stage3_1x.mp4`, `catture/dash/dash_walk60_1x.mp4`.
- Da decidere (owner): il dash da camminata deve superare il limite della camminata analogica (es. fino alla crociera dello stadio di arrivo) e funzionare anche sotto la dead zone degli stadi?

## Giro 2: 1A (e80d33bf, tetto del dash = crociera dello stadio diviso l'inclinazione; sotto StickDeadzone L3 conta come spinta)

- Build OK 49 s; test `GSTNCombat.Core.RunStages+GSTNCombat.NP.Run` PASS 20/20 (DashPerStage con il caso a inclinazione leggera).
- Cattura `Saved/DashStage/cap3` (stesso kit), numeri `catture/dash/speeds_cap3_1A.txt` (`python -I Tools/GASNPPTest/dash_rows.py Saved/DashStage/cap3`), L3 a v133 (v371 per lo stadio 3). Crociera dello stadio 2 nel kit: 610.

| caso | prima | picco (frame dalla pressione) | ritorno |
|---|---|---|---|
| stick 0.3 | 66 | 361 (+10) | ~66 a ~50 f dal picco (stadio torna 1) |
| stick 0.45 | 141 | 455 (+10) | ~186 a 39 f dal picco |
| stick 0.6 | 230 | 526 (+10) | ~299 a 34 f dal picco |
| 0.3, poi stick pieno 2 f dopo L3 | 66 | 610 (+12) | resta 610 (nessun overshoot) |
| stadio 3, stick pieno | 850 | 999 (+9) | 850 a 22 f dal picco |

- Il dash da camminata ora parte anche a 0.3 e sale molto di più (x2.3 - x5.5), ma non arriva alla crociera 610: il burst dura 6 frame e la velocità sta ancora salendo quando finisce (picco al frame +10, 59-86 % della crociera); poi torna alla velocità dell'inclinazione in ~35-50 frame. Per arrivare a 610 servirebbe un burst più lungo o più accelerazione durante il burst da camminata (non fatto: scelta di design).
- Overshoot: nessuno (stick spinto a fondo durante il burst: picco 610 esatto).
- Video 1x del dash da camminata lenta (0.3): `catture/dash/dash_walk30_1A_1x.mp4`.

ESITO: PASS build OK, test 20/20, dash in gioco a ogni stadio; da camminata (1A) picco 59-86 % della crociera, nessun overshoot
SUBAGENTI: 0
COSTO: ~7 $ | 45 min desktop | 0
