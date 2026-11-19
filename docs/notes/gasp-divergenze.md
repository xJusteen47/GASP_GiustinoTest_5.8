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

Decisione owner aperta: base stock nuova (1A) / revert (1B); vedi "Make run start flow into the run".
