# slowmo-jab-ftilt (cloud, 2026-11-24, Opus high) - jab v12 e ftilt v3.1 guardati frame per frame, nessuna modifica
Fonti: dump 1bc74999 (docs/notes/life-calib-run/dumps), video project files jab_v12_ingame(_whiff).mp4, ganon_ftilt_v31_ingame(_whiff).mp4 (60 fps, 1 frame video = 1 frame anim, catture 11-02).
Materiale: docs/notes/reviews/slowmo-jab-ftilt-2026-11-24/ (tutti i frame ritagliati, speed_strip.png). Velocita': `python speeds.py` da Tools/AnimPipeline;
fermi video: diff pixel per frame (ffmpeg crop 200x150 gray -> somma |diff|), valori 0 = frame identico.
## jab v12: FAIL confermato (non falso positivo)
- f1-2 tenuta di contatto 1 frame (braccio steso): A, voluta; va dichiarata (life_holds, motivo "impatto").
- f3-9: dopo lo snap il corpo e' una statua: bacino 0,4 cm in 5 frame, piedi 0,4, solo il pugno scivola ~2,7 cm/frame costanti;
  video fermo totale a f5 e f9 (diff 0) = C (stop-and-go sulle chiavi chamber 5 e recoil_peak 9) + B (nucleo morto). Il peso si assesta solo da f10.
- f18-37: 20 frame identici (dump 0,0 cm, video diff 0) dopo l'ultima chiave 18 (faf 19): B se il gioco li mostra a idle. DA VERIFICARE se il montage esce a FAF.
- stop_sync 0,73: 8 parti su 8 hanno il minimo a f6-7, cioe' UNA fermata comune contata 8 volte; la fermata e' reale (video). Non e' D.
## ftilt v3.1: PASS sui dati attuali, 2 tenute da dichiarare, conferma video mancante
- f5-7 anticipazione: decelera e riparte, minimo 1,3 cm/frame, bacino si muove 3,1 cm nella tenuta: A viva.
- f10-14 gamba tesa dopo il calcio: 1 solo passo quasi morto (10-11, 0,3 cm), poi ease-out continuo; bacino 1,5 cm, testa 3,0: A viva (5 frame, cap tilt 6).
- Il video 11-02 mostra fermo a f19 (diff 4) e gamba che cade a blocco 20-23 = C, ma e' PRIMA del fix `drop` della carta: nel dump attuale 19-20 = 14 cm/frame.
  Serve una cattura nuova del build attuale per chiudere (fisso, ~2 min).
- dump_gate.tsv FAIL 10-14 = D: still_runs con STOP_CM 3,5 su tutta la clip (passi 0,3-2,5 < 3,5; 14-15 = 3,6 esce), soglia di startup, non di clip intera.
## Correzioni proposte ai controlli (piu' severe o piu' precise, nessuna soglia ammorbidita)
1. stop_sync conta EVENTI (cluster +-1 frame), non minimi per parte; esclude i minimi dentro tenute DICHIARATE che passano judge_holds.
   Prova: ftilt con tenute 6-7 e 10-15 -> 0,0 su 11 minimi; jab invariato 0,73 (la sua fermata 5-9 non e' dichiarata).
2. Tenuta viva vs morta per TUTTE le classi (oggi solo smash/strong): nucleo (bacino o testa) >= 1,0 cm nella tenuta.
   jab 5-9 bacino 0,4 testa 1,8 / ftilt 10-15 bacino 1,5 testa 3,0 / ftilt 5-8 bacino 3,1. Proposta: bacino >= 1,0 -> jab morto, ftilt vivo.
3. life.dedupe cancella i frame identici anche nei dump di asset: il tail morto del jab (18-37) sparisce e still_runs passa (falso negativo). Dedupe solo su catture .jsonl.
4. audit_stop_table: still_runs a life_dead_cm 0,4 + controllo 2, non STOP_CM 3,5.
Classi: A tenuta voluta viva, B morta, C stop-and-go, D falso positivo del controllo.
SUBAGENTI: 0
COSTO: ~1,6 $ | 0 minuti desktop | ~60k token review
