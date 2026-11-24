# import-veloce-1: import e build delle clip più rapidi (SC, lato import), 2026-11-24, cloud, modello della sessione Opus
OBIETTIVO: animpipe + import (6 min giro 2 K8, 12 giro 1, 15 K7) più corto a qualità OG: solo automazione, nessun passo tolto. Codice NON provato in UE.
Riuso: editor_session.py (esteso), ue.py (aggancio unico), ue_step.py, no_stop_gate.py, Run-Timed.ps1 invariato. Nessun tocco a GASP (regola 14) né alla cattura (thread bazqb1).
## Fatto (branch claude/project-thread-jlb6i3, base 6blrlz)
1. MISURA: `Tools/AnimPipeline/round_runner.py` cronometra ogni passo e ogni chiamata al bridge (Saved/AnimPipeline/timing/<giro>_<ora>.jsonl);
   `import_timing_report.py` aggiunge dal log UE avvio editor, shader, DDC, PoseSearch, FBX, lavori della sessione (span tra righe con timestamp: misure, non stime).
2. CACHE CAMPIONI (`animpipe/samplecache.py`, via ue.call): la stessa clip veniva campionata 3 volte a giro (build 12 ossa, legibility, clearance) + la sorgente.
   Chiave = percorso + sha1 del .uasset salvato; pacchetto sporco in editor, asset fuori /Game o risposta bloccata = si chiede all'editor. Risposta servita = dict identico.
   Prefetch: alla prima richiesta si chiedono tutte le ossa già viste per quella clip. `ANIMPIPE_SAMPLE_CACHE_VERIFY=1` chiede comunque e logga la differenza max: deve essere 0.
3. UN EDITOR: editor_session.py ha una coda lavori (Saved/AnimPipeSession/jobs): sync (= run_python_file) e boot (script tipo -ExecCmds: QUIT_EDITOR chiude il
   LAVORO, callback del lavoro tolte, PIE fermato, env ripristinato). Il runner riusa una sessione viva (ping), altrimenti la avvia in background mentre girano i passi offline.
4. PASSI GIÀ FATTI: un passo con `inputs` si salta se sha di definizione + file è uguale all'ultimo OK e gli `outputs` esistono; `ok_pattern` evita di marcare fatto
   un comando legacy che fallisce con exit 0 (author/build). Passi offline (carte, card, no_stop_gate) prima: un FAIL ferma il giro e chiude l'editor appena avviato.
5. Giro K8R3 come dati: `cards/overlord/dropkick/k8/round_k8r3.json` (ordine di dropkick-k8-4-piano-r3.md; ue_step.copy idempotente per la take). Cattura esclusa.
Test: `python -m pytest -q Tools/AnimPipeline/tests/test_import_speed.py` = 13 passed (cloud). Suite intera: 10 fallimenti già presenti (mancano sheets/raw e Saved in cloud), nessuno nuovo.
## PROVA (portatile o fisso, editor chiuso, Git Bash in Tools/AnimPipeline, max 2 tentativi regola 5)
1. `python round_runner.py cards/overlord/dropkick/k8/round_k8r3.json --dry` (elenco passi), poi
   `ANIMPIPE_SAMPLE_CACHE_VERIFY=1 python round_runner.py cards/overlord/dropkick/k8/round_k8r3.json --keep` (giro 1: misura base + verifica cache).
2. Subito dopo, senza cambiare niente: `python round_runner.py cards/overlord/dropkick/k8/round_k8r3.json` (giro 2: editor riusato, passi saltati, cache).
3. Per ognuno: `python import_timing_report.py <timing.jsonl stampato> --ue-log=Saved/Logs/ci/animpipe_session_k8r3.log` -> report .md accanto al jsonl.
   Accettazione: VERIFY max diff 0 (altrimenti cache spenta con --no-cache e si scrive perché); check json uguale a un giro --no-cache; tempi giro 1 vs 2 vs 6 min di K8 r2.
Rischi: modalità boot (cattura nella sessione) mai provata su pie_capture_move: usarla solo dopo un confronto frame per frame con una cattura Run-Timed (regola 10);
il ping trova la sessione solo se il bridge non è bloccato da un dialogo; session_alive aspetta 20 s.
INDICE-PROGETTO (quando claude/indice-progetto è in main): "Giro animpipe come dati: Tools/AnimPipeline/round_runner.py + import_timing_report.py; cache campioni animpipe/samplecache.py; coda lavori editor_session.py".
SUBAGENTI: 0
COSTO: ~3 $ (stima) | 0 minuti desktop | 0 token review
