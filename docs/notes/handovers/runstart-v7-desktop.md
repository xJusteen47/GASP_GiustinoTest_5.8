# runstart-v7-desktop: run start v4r7 (braccia a riposo) + build camera-jitter-3 (desktop, 2026-11-20)
Branch claude/runstart-v7-desktop (da claude/playtest-cam-render-skid bf8cc00f, che contiene già runstart-v6). Lock _ue59 preso 22:30Z e
liberato; editor riaperto per l'owner. Ricontrollo (regola 11): Saved/RunStartV6/v4r6s/assert.txt = ASSERT: OK f95-170 93 %, come scritto.

## 1. v4r7 (Cascadeur, runstart_arms_rest_casc_v4r7.py su RunStart_v4r7.casc = copia v4r6)
- Mano in avanti: picco +16,5 cm sopra la spalla -> -4,1 cm (elevazione compressa sopra -40°, K 0,45, reach -12 %); gomito risolto
  con IK a lunghezze native (errore ossa 0,000 cm). Da f24 le braccia sfumano verso la posa braccia del loop della coda alla stessa fase
  (f+18,5; peso 1 su f30-38, nativo da f41). Gambe, piedi, root, busto non toccati. Clearance braccia min +4,9 cm (v4r6 +1,8)
  (runstart_arm_clearance.py, CL_RANGE 0-45). Export export_joints, bake, up Z (stessa dimensione FBX della v4r6).
- UE (Save-Assets runstart_v4r7_ue.py): AS_GSTN_RunStart_v4r7, curve = set della v4r6 (contact_l/r, enable_warping, phase), righe
  CHT v4r6 off, v4r7 on (Saved/AnimPipeline/rows/runstart_v4r7_new.txt). Per tornare: stops_rows_ue.py RS_ROWS=stock con
  RS_PAIRS v4r6=v4r7.
- Cattura Saved/RunStartV7/v4r7 (KIT_Test_Overlord, gstn.Loco.Base 1, 1 giocatore, -Assert assert_v4r7.json): ASSERT: OK v4r7
  f95-170 93 % (min 0,56). Il FAIL nello stesso assert.txt è l'expect interno della sequenza mv_run_start_v5, fermo alla v4r5: da
  aggiornare. Evidenze docs/notes/reviews/runstart-v4r7/ (griglie v4r6 sopra, v4r7 sotto, mp4, brief_review.md). Piedi f80-240 L/R
  36,7/33,3 vs 36,5/33,2 cm (runstart_v4_evidence.py). NESSUNA review ancora: serve il thread cloud (regola 10) prima del video owner.
## 2. Marker e fase (owner)
- Nessuna clip stock Run_Start/Run_Loop ha sync marker (42 asset, Saved/AnimPipeline/runstart/v4r7_ue.json): stock non li usa, MM
  sceglie col costo di posa + Phase. Non ne aggiungo.
- In gioco l'uscita NON è a f32: la clip pesa 1,0 fino a t 1,217 (f37) e sfuma nel loop fra v156 e v184 (t 1,28-1,42), loop
  BS_Relaxed_Run_Loop_F_Slopes da t 0,339 (metrics_v4r7.json). Piedi monotoni nella sfumatura: niente inversione di passo con le
  curve attuali (l'inversione era della variante phase stock v4r5c).
- Pista (non provata): la coda della clip viene dal loop Neutral (runstart_v4_ue.py LOOP = M_Neutral_Run_Loop_F), il gioco suona il
  loop Relaxed: se le braccia scivolano ancora, la correzione stock è rifare la coda dal Relaxed (prima della clip di transizione).
- Unreal Assistant: 2 tentativi falliti (ask_unreal_assistant.py, risposta MCP vuota a editor appena aperto). Domanda pronta nel comando.
## 3. Camera (coordinatore): merge hiq1zx c2257557, build OK, GSTN.Camera 14/14 (Saved/Logs/ci/test_camera_v7d.log)
- Fix di compilazione: CapsuleChangeKeepsHeight usava Near() senza using namespace (ab1f5d99). Da riportare su hiq1zx.
- Traccia 3 giocatori (camera_trace_check.py, Saved/CamJitter3/v1|v2, giocatore 0): Vertical 1: kick root 3 stand 3 cam_comp 3 in 0
  out 0; cambi capsula a terra 18, max in 0,5 s root 301,4 stand 271,7 cam_comp 12,0 in 5,3 out 5,3 cm. Vertical 2: kick 1/2/3/0/0;
  root 304,1 stand 275,9 cam_comp 13,2 in 11,9 out 11,9. "crouch flips 0": lo spam di crouch non ha accroucciato (input?), e le
  finestre includono salti: i numeri non isolano il crouch. Serve la prova owner (ZL spam, FAir, atterraggi).
COSTO: ~15 $ | ~20 min desktop | review 0 token
