# life-metrics-og (cloud, 2026-11-24, Opus medium) - metriche di vita della OG di nuovo in animpipe check
Owner: opzione C del consulto (thread 2). Base: claude/project-thread-vjbryc 276709b1 (riuso curves.py, body_steps, held_frames).
Thread 1 (invarianti + pacchetto MCP, claude/project-thread-y073op) legge `python -m animpipe life <dump> --json` (INV-11).
## Cosa c'e'
- `animpipe/life.py`: stops (GO-STOP-GO nella finestra d'azione, max 2 beat di inversione, pop isolati; i 2 passi verso il
  contatto sono lo snap voluto), still_runs (clip intera, > 3 passi morti < 0,4 cm/frame), holds, stop_sync, cascade, impact,
  anticipation; even_run come misura. `animpipe check`: `stop_and_go` + `life_*` BLOCCANTI su tutte le carte (K7 e' tangents).
- Tenute (owner 23:31Z): fermata = difetto salvo tenuta dichiarata con `why`. Limiti proposti e applicati: max 2 tenute,
  max 6 frame (12 su smash, UFD Little Mac/Ryu), smash ferma < 1 cm di rinculo = FAIL, "tenuta" che si muove > 6% della
  portata = FAIL. Numeri delle tenute delle approvate: NON misurati (dump assenti), da verificare con life_calibrate.
## Taratura (prove)
- `python tests/test_life.py` -> 17 ok. Dati reali in git (tests/data/life, tagliati dalle catture in docs/notes/reviews):
  stock GASP land idle/fwd, skid, jumpsquat PASS, stop_sync 0,08/0,07/0,05/0,18; jab-fk-3 (build a tangenti piatte) 0,84 FAIL.
- Falsi positivi corretti con prova (non ammorbiditi): il cancello vjbryc (passo >= 3,5 cm, max <= 4x mediana, fermo = 3,5 cm
  su tutta la clip) bocciava uno strike da manuale OG (test_good_strike_passes) e tutti gli atterraggi stock
  (test_stock_landing_would_fail_at_action_threshold: stock in recupero a 1-3 cm/frame per 9-27 frame, mai sotto 0,5).
- Catture: un tick ripetuto da' un passo esattamente 0 (stock e jab-fk-3 v45-46): `life.dedupe` lo toglie.
- Difetti noti: serie bacino K7 3 2 0 0 5... FAIL stops; mano K8 57 cm poi 3 FAIL (dip + pop); settle fermo 6 frame FAIL.
  Sono serie sintetiche dai numeri della diagnosi dropkick-k8-3, NON i dump veri.
- `python life_calibrate.py` -> `CALIB: INCOMPLETE 5/13 match` (Saved/AnimPipeline/life_calibration_result.json): mancano i
  dump di w3, w2, ftilt v3.1, jab v12, dair, K7, K8 r2, settle K6. Finche' non da' CALIB: OK le soglie su mosse vere non
  sono provate. `python -m pytest -q tests` -> 9 failed, 82 passed, 1 error, gli stessi 9 + 1 di prima (dump locali mancanti).
## Serve (PC, senza editor aperto, ~5 min, Haiku)
1. `Tools/CI/Save-Assets.ps1 -Script Tools/AnimPipeline/gstn_anim_mcp/clip_joints_dump_ue.py`, CJD_ASSETS =
   AS_GSTN_RunStart_Generic_w3,AS_GSTN_RunStart_Generic_w2,AS_GanonFTiltV31_Build,AS_JabV12_Build,AS_OverlordDAirV4_Build
   + il settle K6 (nome da trovare); K7/K8 sono gia' in Saved/AnimPipeline/runstart. 2. `python life_calibrate.py`.
3. Mismatch = correggere la metrica con prova, mai esentare. Da confermare: quale dair e' approvata (anim-approvate.md non ha la riga).
SUBAGENTI: 0
COSTO: ~4 $ (stima) | 0 minuti desktop | 0 token review
