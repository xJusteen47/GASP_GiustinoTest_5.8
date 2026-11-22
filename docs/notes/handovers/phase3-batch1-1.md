# phase3-batch1-1 (2026-11-20, fisso, Opus high) - run stop v3 (pilota fase 3) + basi stock di skid e run start
Branch claude/loco-runstop-1 (base 021780b0). ESITO: run stop v3 FATTO e PASS (blind, piccolo margine sullo stock); skid e
run start: solo cattura base stock (fermo al limite di contesto, regola 2). Niente all'owner da questo thread finche' il
coordinatore non decide (sotto).

RUN STOP v3 (opzione A della diagnosi):
1. Tools/AnimPipeline/cards/runstop/sprintstop_restyle_casc_v3.py (Cascadeur run_python sulla scena v0_gaspZ): pelvis = traslazione
   del gruppo pelvis+busto, busto/testa/braccia = rotazioni rigide dei sottoalberi attorno al giunto (spine_02, neck_01, upperarm),
   ginocchia IK nel piano originale, piedi pinnati. Stessi tempi di v2 (LEAN 13 gradi, NOD 7, ARM 10/7). Gate pre-UE nel file.
   Gate L: top 17.3 (neck_01/head f41), salto max 27.2 gradi/f, GATE OK; R: top 17.3, salto 34.8, GATE OK (output run_python).
   Scene: Saved/AnimPipeline/runstop/SprintStop_{L,R}_v3.casc, fbx v3 + v0e (stesse impostazioni export: export_joints, up Z, bake).
2. UE: sprintstop_restyle_ue.py con RS_VER=v3 RS_V0=v0e via Save-Assets (log Saved/Logs/ci/save_runstop_v3.log, ESITO OK).
   ue_build.json: root_diff 0.0, 3 curve, 8 notify; top_delta locale max calf_l 17.2 / calf_r 17.0 (prima v2: head 179).
3. Righe: Tools/AnimPipeline/cards/runstop/stops_rows_ue.py (RS_ROWS=dump|new|stock, RS_PAIRS). Stand Stops di
   CHT_GSTN_StockMover_PoseMatch: aggiunte righe 35/37 AS_GSTN_SprintStop_Ganon_{L,R} sotto le Sprint_Stop_F (celle copiate,
   solo la cella 4 PoseSearch cambia asset), mocap disabilitate (Saved/AnimPipeline/rows/runstop_new.txt). Stock = sezione 10.1
   di docs/knowledge/gasp/gasp-structure-5.9.md (risposta Assistant gia' registrata): nessuna divergenza. Fallback C = RS_ROWS=stock.
4. Cattura base 1 (PIE_CAPTURE_CVARS="gstn.Loco.Base 1", 1 player): ASSERT: OK *SprintStop_Ganon* f200-259 min 0.88
   (docs/notes/reviews/runstop-v3/v3_assert.txt). Isolamento: timeline clip identica allo stock base1 tranne la clip di stop,
   x finale 1379 = 1379 (script inline su frames.jsonl, Saved/RunStop1/base1 vs Saved/RunStopV3/cap1).
5. Review blind Fable high (A=stock, B=v3, griglie cmp_* stesse frame v188-262): B piu' vivo/piu' in carattere di poco; A PASS,
   B PASS se v245 testa pulita -> verificato io a piena risoluzione v241-250: nessun pop. Cancello C superato: si tiene v3.
   Note della review per un giro di posa futuro: hold v209-227 troppo fermo, swing braccia tardivo poco leggibile, look
   "atleta" piu' che "bruto". mp4: docs/notes/reviews/runstop-v3/v3_mv_run_stop.mp4 + stock_mv_run_stop.mp4 (7 viste, 60 fps).

SKID / RUN START (base stock 1, nessun restyle ancora):
- mv_skid: lo stock sceglie M_Relaxed_Run_Turn_R_180_Rfoot (pose-matched, da f137), ASSERT: OK *Run_Turn_*180* f140-175
  (docs/notes/reviews/skid-stockbase/, pack stock_mv_skid_*). Da controllare con le note owner: piede dietro piantato, blend in/out.
- Run start: lo stock gioca M_Relaxed_Run_Start_F_Rfoot (f53 in entrambe le catture run stop). Design owner (0.3 s stomp + corsa
  frenetica) = restyle, non stock.
Prossimo thread (fisso, Cascadeur aperto): 1) skid: review della base stock contro le note owner, restyle solo se batte lo
stock (stesso schema v3: copia clip, scene Cascadeur, diff in UE, righe con stops_rows_ue.py generalizzato alla tabella Run F
Loco Transitions/IsPivoting). 2) run start: brief con tratti d'identita' (regola 13a) prima della posa.
Da portare all'owner (coordinatore): run stop v3 PASS con mp4; 2 tab Cascadeur v0 aperti, nessuno da salvare.
Divergenze: nessuna. Lock _ue59 liberato. Asset salvati: AS_GSTN_SprintStop_Ganon_{L,R}, CHT_GSTN_StockMover_PoseMatch.
COSTO: ~22 $ (stima) | 6 min desktop (2 catture PIE) | ~90k token review (1 subagente)
