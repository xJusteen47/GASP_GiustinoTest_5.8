# C1 cattura rapida GSTNCapture (fisso, 2026-11-24, brief handoffs/pulizia-2026-11-24/brief-checkpoint.md §C1) - branch claude/c1-fastcapture-2026-11-24
ESITO: OK (build al 1o tentativo, cattura al 1o tentativo, ASSERT: OK)
Riuso: Plugins/GSTNCapture + PIE_FASTCAP di pie_capture_move.py (fast-capture-1), Run-Timed.ps1 Build/Capture, assert_spec di dair-v9-stops.
FATTO
- _ue59 segnato (IN-USO.txt, owner C1-fastcapture) e portato su 49998c49. File locali dell'owner conservati: patch FOOTDBG di
  pie_capture_move.py riapplicata (non committata), 4 AS_GSTN_RunStart_Generic_w2t*.uasset locali ricopiati sopra quelli del commit;
  copie di sicurezza in `Unreal Projects/_scratch/c1-backup-2026-11-24/`. Lock liberato alle 02:25 UTC. Nessun asset salvato.
- Build: `Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\c1-build.log` -> ESITO: OK, 272 s, "Result: Succeeded" con GSTNCapture
  (solo 2 warning C4996: SubmitCommandsAndFlushGPU/BlockUntilGPUIdle deprecati, GSTN_FastCapture.cpp:58-59 -> SubmitAndBlockUntilGPUIdle).
- Cattura: `Run-Timed.ps1 -Preset Capture -Script <boot> -AssertFile Saved\C1FastCap\cap\assert.txt`, boot con PIE_CAPTURE_KIT=
  KIT_Test_OverlordDAirV9, SEQUENCES=dair_whiff,dair_hit_rise, VIEWS=az90,az180,game, PIE_CAPTURE_ASSERT=dair-v9-stops/assert_spec.json,
  PIE_FASTCAP=1. ESITO: OK, ASSERT: OK (peso 1,00 su 100% in f32-62 e f30-58). Log: Saved/C1FastCap/cap/capture.log, Saved/Logs/ci/c1-cap.log.
MISURA (righe "timing"/"fastcap" dei capture.log)
- Prima (GASP_Giustino2/Saved/DairV9Stops/cap/capture.log, PNG export, 7 viste, 150 frame): dair_whiff wall 100,1 s (export png 97,3),
  dair_hit_rise 99,2 s (96,3); driver totale 234,9 s.
- Dopo (GSTNCapture, 3 viste, finestra press-20..+90, fine anticipata): dair_whiff wall 4,57 s (100 shot, 300 PNG, 0 drop, 0 stall),
  dair_hit_rise 4,65 s (97 shot, 291 PNG); driver totale 47,0 s; processo intero con avvio editor 108 s (prima ~4,6 min).
- Per frame per vista: ~0,095 s prima (97,3/(150*7)) contro ~0,015 s dopo (4,57/300). Il confronto per sequenza mescola 7->3 viste e
  150->~100 frame: il guadagno puro del motore e' ~6x per immagine, quello del giro ~20x per sequenza.
- Passo 1/60 conservato: posizione z del pawn identica tra vecchia e nuova cattura ai frame 40/60/80 (505,3 / 750,7 / 949,2 cm, frames.jsonl).
- Guardato f045 delle 3 viste: personaggio illuminato e leggibile (luce semplice sulle az*, game piena).
DA SAPERE
- Il plugin scrive ancora PNG (asincroni), non un mp4 diretto: mp4 e griglie restano a pie_capture_grid.py, invariati.
- "FASTCAP: background throttle not changed: module 'unreal' has no attribute 'EditorPerformanceSettings'": il throttle non viene tolto da
  Python (Run-Timed lo toglie gia' con NoThrottleIni); da sistemare nel driver, non blocca.
- Le 2 chiamate deprecate vanno aggiornate prima del prossimo engine (codice, coda C4/C5).
SUBAGENTI: 0 (build e cattura lanciate direttamente con Run-Timed, nessun lavoro di giudizio delegato)
COSTO: ~1,5 $ | 7 minuti desktop (build 4,5 + cattura 1,8) | 0 token review
