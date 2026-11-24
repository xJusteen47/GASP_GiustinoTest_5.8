# T4 taratura vita (fisso, 2026-11-24, Opus; nota coordinatore, budget ~3 $) - branch claude/anim-skill-t3
FATTO
- dair: in life_calibration.json `dair_approved` = AS_OverlordDAirV9 + carta overlord_dair_v9.json (approvata 01:26Z, AM 916338ad).
  Causa del dump statico di T3: lanciato sul progetto lifecal, che non trova lo scheletro ("Impossibile recuperare USkeleton",
  Saved/Logs/ci/t4_dairv9.log.ue.log). Rifatto sul progetto _ue59 (headless): bacino 28 cm, testa 52, piede sx 84 su 43 frame
  (t4_dairv9b.log). Dump: docs/notes/life-calib-run/dumps/joints_AS_OverlordDAirV9.json. I dump vanno fatti dal progetto _ue59.
- K6 settle tolto dalle attese (lista `unjudged`): mai visto a video, quindi non e' un difetto noto. Casi difettosi = K7 e K8 r2.
- `python Tools/AnimPipeline/life_calibrate.py --write-gate`: CALIB FAIL 11/12, mismatch solo dair_approved
  (docs/notes/life-calib-run/life_calibration_result_t4.json). La dair v9 approvata esce FAIL su stop_sync: 0,76 dei 34 minimi
  condivisi (max 0,5); fermate comuni ai frame 4 (tutte e 8 le parti, bacino 0,0 cm allo step 3), 17, 25, 35.
  17 e 25 sono i bordi della "moving hold" 17-26 scritta nel design della carta ma NON in life_holds; 4 e 35 no.
  Non ho dichiarato tenute per farla passare (nessuno l'ha giudicata a video): sarebbe ammorbidire (regola 17).
- life_gate.json scritto e committato: 1 metrica bloccante, still_runs (passa stock x4, w3, w2, ftilt v3.1, dair v9;
  boccia jab v12, statua/coda morta viste nella review slowmo e accettate dall'owner come difetto vero). Tarata per regola 17.
  stop_sync NON e' bloccante (boccia una clip approvata): resta misura. stops/cascade/anticipation/impact restano misure:
  nessuna clip buona ha finestra/contatto, quindi manca la prova. `calib` nel file = FAIL perche' la riga dair non torna.
- anim_layer vs w3, dal codice w3 a 22a318f1 (driver docs/notes/anim-layer/t4_layer_driver.py, builder = `git show
  22a318f1:.../runstart_w3_ue.py` con DST/OUT da env). ref = 22a318f1 cosi' com'e', src = W3_ARM_R_OUT=0, lay = src +
  STEP_C1_02. Gomito dx entro 0,9 cm su tutti i frame (il passo stesso vale 6,1); mano dx entro 1,8 cm f0-14 ma 3,6/6,4/2,8 cm
  a f15-17 (il passo vale 11,2). Lo strato tocca solo upperarm_r, conservazione 0,0 gradi fuori finestra.
  Lettura: la rotazione del braccio e' riprodotta; il resto sulla mano viene dall'avambraccio, che il builder ricalcola dopo
  l'apertura. Equivalenza quindi buona sull'osso dello strato, non sulla mano a f15-17. Dati: t4-ue-run-22a318f1-compare.json.
- _ue59: lock preso e liberato, asset /Game/_T4tmp cancellati dal driver, nessun commit. Resta joints_AS_OverlordDAirV9.json in
  _ue59/Saved (non versionato; il classificatore ha bloccato la cancellazione).
PER L'OWNER / COORDINATORE
- dair v9 su stop_sync: o difetto vero (come jab v12) o metrica da correggere. Serve uno sguardo a video sui frame 3-5 e 34-36.
  Se la hold 17-26 e' viva, va scritta nei life_holds di una carta nuova (la v9 e' bloccata) o nella riga di taratura con la prova.
- Le stesse richieste di tag del T3 restano valide. INDICE-PROGETTO.md non e' su questo branch: voce da aggiornare dopo il merge.
SUBAGENTI: 0 (tre run headless brevi fatte direttamente)
COSTO: ~3 $ (stima) | 5 minuti desktop (3 run headless da 10-13 s + analisi) | 0 token review
