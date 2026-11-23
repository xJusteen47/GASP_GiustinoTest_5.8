# Skill di animazione: cosa manca e piano di test (2026-11-23, cloud, Opus high, sola lettura)

Branch letti: `claude/anim-mcp-v2-turn4` 1243e7ee, `claude/anim-mcp-followthrough` caa82e56 (contiene turn4),
`claude/anim-mcp-v2-turn4-capture` b389f6b0. Nessuno dei tre e' in `main` ne' in `claude/integ-2026-11-23`.
Test offline rilanciati da me: `python3 -m unittest test_follow_through test_gstn_anim_mcp test_anim_mcp_cheap`
(da Tools/AnimPipeline/tests, worktree su caa82e56): 45 test, 44 OK, 1 errore = `pwsh` assente nel cloud (come dichiarato).

## 1. Review indipendente della cattura w3m7: INCONCLUSIVA (non PASS, non FAIL)
Guardate `docs/notes/reviews/anim-mcp-v2-retest/grid_ab_az180.jpg` e `grid_ab_az90.jpg` (b389f6b0), una volta ciascuna.
- La catena tecnica funziona: register -> cattura -> unregister, ASSERT: OK dichiarato, chooser ripristinato con sha256.
- Il confronto NON e' omogeneo: la riga A e' la variante 21 (w2, approvata), la riga B e' w3m7. Gia' a v77, prima della
  finestra del ritocco, le due righe hanno posture diverse (in w3m7 il busto e' girato e le braccia larghe). Le differenze
  quindi vengono da w3 contro w2, non dal ritocco `arm_clr_deg 12`. "Gomiti piu larghi" non e' attribuibile all'edit.
- Campionamento ogni 15 v-frame: solo 2 immagini (v92, v107) cadono nella finestra modificata. Troppo poco per giudicare.
- Regola 10 non rispettata: mp4 e griglia della base w3 non committati (restano in `_ue59/Saved/CaptureSpeed/w3m7_t4`).
- Nessuna compenetrazione o piede sbagliato visibile, ma a questa densita' non e' una prova.
Il PASS numerico del handover (spostamento gomito ~5-7 cm, coerente con 12 gradi) resta plausibile: e' una misura, non uno sguardo.

## 2. Cosa manca per dire "skill finita"
Bloccanti
1. Branch non unito: anim MCP v2 + follow-through non sono in integrazione ne' compilati/provati insieme sul PC.
2. Follow-through mai eseguito in UE: lettura pose LOCAL e `set_bone_track_keys` mai provati (`follow_through_ue.py`).
3. L'MCP e' solo per il run-start: `anim_register_variant` scrive solo nel chooser del run-start, `anim_capture_ab` usa
   `mv_run_start_v5`, `anim_gate_check`/`anim_inspect` usano `runstart_pivot_gates`. Per una mossa di combattimento (montage
   suonato da un'abilita') non c'e' ne' registrazione ne' cattura via MCP: oggi servono `pie_capture_timed.ps1 -Assert` a mano.
4. Socket cotti: le hitbox `BakedSocket` leggono `BakedSockets` della timeline. Se un edit tocca le ossa nei frame attivi,
   la hitbox non segue la posa finche' non si rifa' il bake (`FGSTN_SocketBaker::BakeStandalone`, staleness via `BakeInfo`).
   L'MCP non controlla ne' rifa' il bake.
5. Motore geometrico (`geom_intent_ue.py`) valido solo per clip che corrono: gli assi del personaggio vengono dal moto
   del bacino nei frame 14-24; `lat_vec` puo' puntare a destra. Bug corretto solo per `arm_clr`; ancora da verificare
   `torso_bank`, `lat_widen`, `step_narrow`, `arm_pv_out` (righe 593, 626-628, 648). Su una mossa sul posto gli assi sono
   inaffidabili: per il combattimento oggi e' generico solo `follow_through`.
Da sistemare (piccoli)
6. `anim_register_variant` accetta id 30-39, che sono le righe Handoff delle varianti approvate 20/21 (w1=30, w2=31): il
   minimo giusto e' 40, o un controllo sulle righe gia' presenti.
7. SKILL `gstn-anim-cycle-cheap` vecchia: elenca 5 tool (sono 8), griglia fissa `grid_pivot_v84_116` (solo run-start),
   giudice "Gemini Neon" (crediti da verificare, regola 3) e critico Fable (regola 3: Fable solo dopo 2 errori di Opus).
8. `cache_log.py` non cablato (manca il punto dove il bridge passa `usage`); test `pwsh` da rilanciare sul desktop.
9. `docs/notes/anim-mcp-costo-velocita.md` non esiste su nessuno dei branch letti (ne' in main/integ/indice).
10. Skill account `gstn-move-animation`: le modifiche del 11-19 (`docs/notes/review/gstn-move-animation-edits-2026-11-19.md`)
    aspettano ancora l'owner; la skill non sa dell'MCP.

## 3. Piano di test su una mossa vera
Mossa consigliata: **dropkick K6, clip Settle** (`AS_OverlordDropkick_SettleK6`, branch `claude/project-thread-1akb6a`).
Motivo: la review K6 round 1 e' FAIL proprio su "press e assestamento troppo piccoli da leggere"; il handover
`dropkick-k6-2-start.md` chiede avambracci che si staccano dal petto 4-5 cm e ricadono per ultimi, testa che segue 2 frame
dopo il corpo. E' il caso esatto del follow-through (catene `arms_fingers` + `neck_head`). Il pitch del corpo (PEAK/UNDER)
resta a Cascadeur: l'MCP rifiuta bacino e gambe. Conta come giro di posa 2 di 3 del dropkick (regola 10).
Tratti d'identita' da controllare per primi: gambe unite e dritte, braccia conserte, linea Big Band, snap a13-a15.

| Passo | Dove | Modello | Costo | Cosa |
|---|---|---|---|---|
| T0 preparazione | cloud | Sonnet medium | ~2.5 $ | merge turn4+followthrough in un branch di test; tool `anim_capture_move` (wrapper di `pie_capture_timed.ps1 -Assert`, viste az180+az90+game, base e variante nella stessa sessione, mp4 committati); controllo bake (stale = FAIL o rebake); id variante >= 40; guardia che rifiuta i parametri geometrici su clip non run-start; SKILL cheap aggiornata |
| T1 esecuzione | fisso (editor chiuso, `_ue59` libero) | Sonnet medium | ~3.5 $, ~60-75 min | build; `dry_run`; `anim_run_intent` follow-through sul Settle; controlli 15b; rebake socket; cattura base K6 e variante, stessi frame, real + slow4x; commit in `docs/notes/reviews/dropkick-k6-ft/` |
| T2 review | cloud | Opus high giudice + critico Opus high (subagente cieco) | ~3.5 $ | regola 10: video e griglie, prima la vita, poi i tecnici |
Totale ~9.5 $; con un giro di correzione (T1+T2 ancora) ~16.5 $.

Criteri di PASS della SKILL (non della mossa)
- Il ciclo T1 e' guidato da Sonnet solo con i tool MCP: nessuno script di quaternioni scritto a mano, max 2 tentativi.
- Report follow-through OK: posa all'anchor identica (0.01 gradi), niente fuori finestra, solo ossa della catena, piedi fermi.
- Gate 15b OK (caviglie >= 10 cm, ossa <= 5%, tremolio gomiti/polsi, compenetrazioni: budget petto >= 0.4 cm) e
  conservazione <= 15%.
- Bake non stale dopo l'edit; ASSERT: OK su base e variante; A/B omogeneo (K6 contro K6+FT, stessi frame, dietro e lato).
- Review cloud: tratti d'identita' PASS, assestamento leggibile, nessun "grave" aperto del critico.
- L'intent JSON e' salvato accanto alla scheda, cosi' un rebuild Cascadeur non lo perde (si riapplica).
Se il test passa: merge in integrazione, compilazione sul PC, e la skill si dichiara finita. Se fallisce per "gioco"
o "strumento", fix nel tool (non conta come giro di posa).

## Decisione owner (2026-11-23 19:01Z): 1A 2A 3A, ma SOLO DOPO la critica di Opus al dropkick
- Ordine: 1) thread separato con la critica Opus del dropkick K6 (severita' massima), 2) T0 preparazione, 3) T1 + T2 sul
  Settle. L'intent di T1 (catene, massa, overshoot, finestra) si scrive dai difetti trovati dalla critica, non da questo piano.
- w3m7 archiviata: "catena tecnica OK, estetica non giudicata", nulla all'owner.

## T0: in piu', tutto cio' che fa sprecare di meno (richiesta owner)
1. Dry run obbligatorio: `anim_run_intent` rifiuta il run vero se non c'e' un dry_run con lo stesso intent hash.
2. Cancello prima della cattura (blocco duro in `anim_capture_move`): niente cattura senza FOLLOW_THROUGH OK, gate 15b OK,
   conservazione OK, bake non stale. Un fallimento qui costa secondi, non una sessione editor.
3. Soglia di leggibilita' numerica: se lo spostamento misurato della mano/testa nella finestra e' sotto una soglia
   (proposta 2 cm o 3 gradi), niente cattura: sarebbe FAIL "troppo piccolo da leggere" (come K6 r1).
4. Cache della base: la cattura base si riusa se hash clip + seq + cvar + viste + build sono uguali (manifest); si cattura
   solo la variante. Stesso intent hash gia' catturato = nessuna nuova cattura.
5. Build solo se cambia C++ (hash di Source/ e Plugins/*/Source); altrimenti si salta.
6. Una sola sessione editor per base, variante e tutte le viste; una cattura per giro.
7. Pacchetto review compatto: griglia fitta solo attorno ad anchor e FAF, max 8 immagini per vista; il revisore non riceve
   altro (regola 4).
8. Contatore tentativi per obiettivo nel manifest: al terzo il tool si rifiuta (regola 5).
9. Costi automatici: `cache_log` cablato e minuti desktop per passo scritti nel manifest, cosi' costi.csv si compila da solo.
10. Prefisso tool stabile (gia' fatto) per la cache dei prompt; risposte compatte opt-in per i passi ripetuti.
Costo T0 rivisto: ~3.5 $ (era 2.5 $). Totale test ~10.5 $, con un giro di correzione ~17 $.

## Decisioni per l'owner (risposta tipo "1A 2A 3A")
1. Mossa del test. **A (consigliata)** dropkick K6 Settle, follow-through braccia e testa: caso reale e aperto. B run-start
   w3 con A/B omogeneo: piu economico (~4 $) ma prova solo la locomotion, non il combattimento.
2. Preparazione. **A (consigliata)** thread cloud T0 prima del test (~2.5 $): senza, l'MCP non sa catturare una mossa di
   combattimento ne' controllare il bake. B test subito con gli script di cattura a mano: ~2.5 $ in meno, ma non prova la skill
   come la userebbe un modello economico.
3. Cattura w3m7. **A (consigliata)** archiviarla come "catena tecnica OK, estetica non giudicata", nulla all'owner. B rifarla
   ora come w3 contro w3m7 (~1.5 $, ~30 min fisso).
Applicato senza chiedere (regola 6): critico Opus high invece di Fable (regola 3 del 2026-11-23).
