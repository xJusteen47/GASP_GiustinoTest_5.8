# Divergenze da GASP Mover stock (regola 14, dal 2026-11-20)

Default = comportamento stock (pose matching, root motion dalla clip, uscite a evento). Ogni deviazione: una riga
PRIMA del commit. Prima di aggiungerla chiedi all'Unreal Assistant come lo fa GASP stock.

| # | Cosa | Dove | Perché | Alternativa stock | Costo per tornare | Stato |
|---|---|---|---|---|---|---|
| 1 | Righe chooser nostre con start time fisso, senza pose match | CHT_GSTN_OwnStates | far suonare le nostre clip nei nostri stati | pose-match rows (MM raffina lo start frame) | ~5-10 $ | da rivedere (run-start-handoff-1 §6) |
| 2 | Uscita run start a tempo (poi contatto piede, max 0,15 s) | run_start_states.py, UGSTN_RunPresentationComponent | stamp frantico | uscita a evento stock | basso | parziale (run-start-handoff-1 §2) |
| 3 | Clip senza root motion coerente (RunStart 0 cm, RunStop 125 vs ~360, Skid 45) | AS_GSTN_RunStart_Frantic, AS_RunStop_*, Skid_180 | clip generate in place | root motion dalla clip | ri-autorare le clip | da correggere |
| 4 | Skid in override layer, non riga chooser + MM | ABP, layer skid | nessun tool edita le righe (run-start-stop-skid-1) | riga chooser + orientation warping | ~5-8 $ | da rivedere |
| 5 | Splice automatico 0,2 s su dash/run start/run stop/skid | fix_clips_ue.py | hand-off gaps | blend stock + pose match | rigenerare le clip | da rimuovere |
| 6 | Interruttore base locomotion `gstn.Loco.Base` (0 = ABP attuale con nodi GSTN, 1 = copia stock pulita `/Game/GSTN/Locomotion/StockBase`: ABP_GSTN_StockMover + CHT_GSTN_StockMover_PoseMatch + CHT_GSTN_StockMover_PSDRelaxed) | UGSTN_LocoBaseSubsystem (SetAnimInstanceClass allo spawn) | fase 1 rebuild (owner 1A): confrontare vecchio e nuovo | nessuna: la copia 1 È lo stock; lo stock ha una sola ABP | togliere subsystem e cartella, 0 $ | attivo, default 0 (loco-base-1) |
| 7 | Jump squat: 3 frame sim (62,5 Hz = 48 ms) a terra prima del decollo (`JumpSquatFrames=3`) + stato ABP "Jump Squat" (alias da stati a terra, riga CHT_GSTN_OwnStates con posa segnaposto = carico di M_Relaxed_Jump_F_Start_Stand_Rfoot da 0,613 s, blend 0) | Config/DefaultGSTNCombat.ini, ABP_GSTN_Locomotion, UGSTN_RunPresentationComponent::IsAnimOwnerJumpSquatting, Tools/GAS/jump_squat/ | owner 1A 2026-11-17 (squat alla Smash); stock non ha squat: decollo il frame della pressione, In Air Transition entra su MovementMode Falling e la clip di stacco pose-matched salta il carico (entra a ~0,667 s, phase3-batch2-1) | nessuno squat: decollo immediato, stacco stock | `JumpSquatFrames=0` (0 $) + togliere stato/alias/riga (rimuovere "Jump Squat" da route_states.py, ~2 $) | attivo, posa segnaposto (jumpsquat-v0) |

Decisione owner 2026-11-20: 1A (base stock nuova, la vecchia resta per confronto); vedi "Make run start flow into the run".
