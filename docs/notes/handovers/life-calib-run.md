# life-calib-run (fisso, 2026-11-24, Sonnet medium) - taratura life + audit stop
Dopo il commit dei dump script su ue59 (4686ea52) l'owner ha girato Save-Assets (audit_stop + calib_dumps): dump OK.
## Taratura (comando: python Tools/AnimPipeline/life_calibrate.py, wrapper per-clip; tabella docs/notes/life-calib-run/life_calibration_result.json)
- CALIB: INCOMPLETE. 8/13 riga MATCH: 4 stock PASS, jabfk3 FAIL, w3 PASS, w2 PASS, ftilt_v31 PASS (senza esenzioni).
- **jab_v12 (approvata) FAIL**: stop_sync 0.73 di 11 minimi di velocita' condivisi (max 0.5). Soglia NON toccata: da decidere
  (correggere la metrica con prova o dichiarare), cfr. ftilt/w3 che passano.
- dair_approved: ERRORE della carta, non della metrica: cards/overlord/dair/overlord_dair_v4.json approved[0] ref
  overlord/overlord_dair_v4.json non trovato sotto cards/ (locks.guard). Dump AS_OverlordDAirV4_Build presente. Serve fix del ref.
- K7, K8r2, settle K6: MISSING. Nomi reali non esistono (resolved.json: AS_Pilot_OverlordDropkick_StartupK7/SettleK6/LoopK6 = []).
  Serve il nome dell'asset reale o i dump di Saved/AnimPipeline/runstart gia' fatti dal thread dropkick.
## Audit stop (audit_stop_table.py, 3ufz10): docs/notes/audit-stop-2026-11-24/logs/dump_gate.tsv, 55 clip, 17 FAIL
- run start 0/13 FAIL, w3 0/1, dash 0/1, hit/react 0/15; jab 5/5 FAIL (V12, Jab1/2 V3 Overlord e Pilot); getup 2/2 FAIL (21-25);
  altre 10/18 FAIL: ftilt V3.1 (10-14) e Pilot (13-18), dair V4 (0-42, 3-9;15-31), dair land heavy/light, dropkick land.
  Turnaround: nessuna clip distinta nel raggruppamento per nome (t180/t090 sono dentro "runstart", 0 FAIL). Nessuna tenuta passata.
- Non classificato ne' ammorbidito; resolved.json: Getup glob, StartupK7, SettleK6, LoopK6 senza match.
SUBAGENTI: 0
COSTO: ~1,2 $ | 12 minuti desktop (dump dell'owner) | 0 token review
