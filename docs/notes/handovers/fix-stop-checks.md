# fix-stop-checks (cloud, 2026-11-24, Sonnet medium) - 4 correzioni ai controlli delle fermate, nessuna soglia ammorbidita
Base: claude/project-thread-mkedi4 (contiene 1uvcac). Branch: claude/project-thread-sgky92. Riuso: animpipe/life.py, curves.py, life_calibrate.py; audit_stop_table.py preso da 3ufz10 e riscritto su life.py (no_stop_gate resta su wwnerm/y073op, non duplicato).
Prove: `python tests/test_life.py` (22 ok, 7 nuovi su dump reali jab v12 e ftilt v3.1 in docs/notes/life-calib-run/dumps); `pytest tests/` 89 passed, 9 failed + 1 error IDENTICI a prima delle modifiche (file locali mancanti in cloud: sheets/raw, capture).
## 1) stop_sync (life.stop_sync)
- Una corsa piatta di minimi di una parte = 1 minimo (coda morta jab: da 0,98 su 145 a 0,82 su 17).
- I minimi dentro una tenuta DICHIARATA che passa judge_holds (motivo, tetti, nucleo vivo) sono saltati; morta o non dichiarata conta.
- ftilt: 0,35 senza tenute, 0,0 con tenuta calcio 10-15 (PASS). jab: 0,82, FAIL (non 0,73: quel valore veniva dal dump deduplicato, bug 3).
- DEVIAZIONE dal brief (regola 12/17): il blocco resta la quota PER PARTE; gli EVENTI sono riportati nella riga ("2 of 4 stop events") ma non bloccano, perché a soglia 0,5 lascerebbero passare il jab (0,5 su 4) e abbasserebbero jabfk3 da 0,88 a 0,67 = ammorbidimento. Soglia 0,5 tarata sulla quota per parte.
## 2) Tenuta viva/morta (judge_holds, tutte le classi)
- Nuova soglia life_hold_core_cm 1,0: percorso del bacino sulla tenuta. jab 5-9 = 0,4 cm MORTA; ftilt 10-15 = 1,5 cm VIVA.
- Una tenuta morta e' FAIL e non esenta niente (ne' still_runs-skip di stop_sync).
- Aperto: una tenuta di 1 frame (jab contatto 1-2: 0,1 cm; ftilt 6-7: 0,1 cm) risulta morta. L'anticipazione ftilt 5-8 non e' una tenuta (piede 9,7 cm/frame, oltre il 6% del reach) e non va dichiarata: il suo minimo non e' condiviso (4 parti su 8).
## 3) Dedupe solo sulle catture
- life.load_clip: dedupe solo su .jsonl; life_calibrate e CLI lo usano. jab: 38 frame, still_runs FAIL 18-37 (prima passava con 19 frame).
- I dati tests/data/life/stock_* erano gia' deduplicati al taglio (campo source): invariati.
## 4) Audit fermate (gstn_anim_mcp/audit_stop_table.py)
- Ora usa life.run sull'intera clip a life_dead_cm 0,4 + tenute (nucleo) + stop_sync; niente STOP_CM 3,5. ftilt: OK (prima FAIL 10-14, classe D); jab: FAIL 18-37 + stop_sync.
- Colonne nuove: stop_sync, failed. Va rigenerata la tabella dump_gate.tsv sul fisso (i dump in git bastano per jab/ftilt/w2/w3/dair).
## Calibrazione (life_calibrate.py, dump in git)
- life_calibration.json: ftilt con tenuta 10-15 dichiarata nel manifest (la carta e' approvata/bloccata: aggiungere life_holds alla carta = scelta del coordinatore); jab_v12 expect FAIL con motivo (difetto reale, slowmo). 9/13 match, dair ERROR (ref approved della carta dair v4 non trovato, preesistente), K7/K8/K6 mancano.
- life_calibrate ora segna ERROR una carta rotta invece di fermarsi.
## Da portare all'owner
- jab v12 approvato ma FAIL reale (statua f3-9, coda morta 18-37): decidere variante o esenzione scritta in anim-approvate.md.
- no_stop_gate.py (wwnerm/y073op) usa ancora 3,5 cm su tutta la clip: stesso falso positivo D; al merge farlo puntare a life.py.
SUBAGENTI: 0
COSTO: ~2,5 $ | 0 minuti desktop | 0 token review
