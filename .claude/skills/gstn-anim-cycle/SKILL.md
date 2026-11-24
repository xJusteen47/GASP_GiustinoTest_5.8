---
name: gstn-anim-cycle
description: Il giro di lavoro delle animazioni GASP_Giustino (mossa nuova dalle pose dell'owner = AUTORE, migliorare una clip esistente a strati = POLISH), uguale per ogni profilo di esecutore (Claude, DeepSeek flash con script e MCP, Antigravity), con passi a numeri, uno sguardo rapido per ciclo e la review finale completa una volta. Usala per ogni lavoro di animazione insieme a gstn-anim-reviewer e gstn-anim-executor; sostituisce gstn-anim-cycle-cheap.
---

# gstn-anim-cycle

Il giro che lega `gstn-anim-reviewer` (decide UNA istruzione alla volta, non edita) e `gstn-anim-executor` (applica,
non giudica). Una sola skill per tutti: "cheap" non e' piu' una skill ma un PROFILO (§5). Nata il 2026-11-24 (owner,
opzione A del thread "principi e skill"; brief `handoffs/skill-anim-unica-brief-2026-11-24.md`) per unire i branch della
notte e lavorare come la chat del w3: veloce, per principi, senza rovinare le clip buone, con la review finale completa.

## INVARIANTI (non toccare per costo)
Owner 2026-11-24 (ambito: tutta la skill di animazione e la review): qualita' uguale alla OG, nessun compromesso sui falsi
positivi, risparmi solo da strumenti automatici. Fonte, con il perche' di ogni voce: `Tools/AnimPipeline/contract/invarianti-review.md`.
Le cambia solo l'owner; chi snellisce questa skill lascia intatta questa sezione (`tests/test_review_invariants.py` lo controlla).
Ogni profilo risparmia SOLO sull'authoring e sul livello 1; il livello 2 (review finale) e' lo stesso per tutti.
- INV-01 identita' per prima. INV-02 vita prima dei numeri, sul video reale + slow4x.
- INV-03 slowmo di TUTTA la mossa, 1 tick per cella, az90 + az180 + game, nessun frame scoperto.
- INV-04 schede velocita' con fermate e scie per OGNI clip. INV-05 immagini grandi delle pose chiave per vista.
- INV-06 prima/dopo con la base, stessi frame, anche le parti ereditate. INV-07 critico cieco indipendente.
- INV-08 il livello 1 scarta, non promuove. INV-09 nessun tetto di immagini sul livello 2 (tetto = nessun frame scoperto).
- INV-10 un falso allarme si corregge, non si ammorbidisce; i blocchi ereditati bloccano (esenzione solo dell'owner);
  un controllo nuovo nasce come misura e blocca solo con la prova di taratura (owner 2A).
- INV-11 metriche di vita e controlli di posa della OG nel livello 2.
- INV-12 tenute (owner 1A 2026-11-24): una tenuta VIVA (bacino o testa continuano a muoversi) e' uno strumento normale,
  dichiarata con motivo scritto e giudicata a video; i difetti sono la tenuta MORTA e lo stop-and-go. 2 tenute e 6 frame
  sono numeri guida per l'occhio, non blocchi.
- INV-13 i principi si CONSIDERANO uno per uno, non si applicano come un automa (owner 2026-11-24): ogni voce di `Tools/AnimPipeline/contract/principi-review.md` scritta in `final/principi.md` (intento, previsione, osservato con frame e vista, scostamento e se e' voluto, giudizio, o `non pertinente` col motivo); niente PASS senza `PRINCIPI: OK` di `principi_review.py check`, anche per il critico cieco.
Materiale del livello 2: `anim_review_pack` (`final/review_pack.json`, `complete: true`, altrimenti niente review).

## 1. Due modi di lavoro
- **AUTORE** (mossa nuova dalle pose dell'owner, Cascadeur Step o scheda): carta in `Tools/AnimPipeline/cards/`, sempre
  con `"curve": "spline"` (build con chiavi spline per frame: le tangenti piatte su ogni chiave sono lo stop-and-go di
  K7/K8). Le tenute volute si dichiarano nella carta (`life_holds [{frames, why}]` o `hold` + `why`/`hold_reason`).
- **POLISH** (migliorare una clip che esiste, anche approvata, sempre in una VARIANTE): STRATI morbidi sopra la clip
  densa, come il w3. Uno strato = parte, asse nel corpo del personaggio, valore, frame di picco, entrata e uscita
  (inviluppo `compute_envelopes`). Il polish **non aggiunge mai chiavi alla carta** e non ricostruisce la clip dalla
  carta: ogni chiave in piu' su una carta sparsa e' una fermata (diagnosi `dropkick-k8-3`). Strumento: `anim_apply_intent`
  / `anim_run_intent` oggi; `anim_layer` (thread T2, assi dal corpo, `dry_run`, snapshot, rollback) quando c'e'.
  Ogni passo si annulla con `anim_rollback`. Una clip approvata (`docs/notes/anim-approvate.md`, branch `claude/approved-*`)
  non si tocca mai: si lavora sulla sua copia variante.

## 2. Il giro (metodo del w3, uguale per tutti i profili)
Perche' il w3 e' riuscito (8 passi in 35 min, `docs/notes/reviews/runstart-w3-c1|c2/passo-*/istruzione.json`):
1. **Una causa fisica** per ciclo, detta dall'owner o trovata dal revisore sul video (es. "il peso del torso cade avanti e
   le gambe lo recuperano"), scritta in testa al ciclo.
2. **Istruzioni a livello `principio`** (`contract/instruction.md`): intento, principio con la sezione del documento,
   `verifica`; il controllo lo sceglie l'esecutore. Le soglie dei principi si CONSIDERANO (INV-13), non si applicano.
3. **Un pezzo per passo** (`trunk`, `arm_r`, `arm_l`, `legs`, `head`, `feet`), con la lista `non_peggiorare` che porta
   avanti i valori dei passi precedenti.
4. **Tra i passi solo numeri**: cancelli a 0 token e prima/dopo; nessun modello guarda immagini tra un passo e l'altro.
5. **Una cattura per ciclo**, poi lo sguardo di ciclo (§3); mai catture a ogni passo.
Max 2 tentativi per obiettivo (regola 5), max 3 giri di posa (regola 10), poi decide l'owner.

## 3. Tre sguardi
| Sguardo | Quando | Cosa | Esito possibile |
|---|---|---|---|
| Passo | dopo ogni istruzione | `esito.json`, cancelli, prima/dopo numerico, `non_peggiorare` | avanti / rollback |
| Ciclo | una volta per ciclo, dopo la cattura | griglie + slow4x della finestra, misure di vita `WOULD FAIL` | "altro ciclo" o "pronto": **mai PASS** (INV-08) |
| Finale | una volta, quando il ciclo dice "pronto" | livello 2 completo: `anim_capture_move final_round=True` + `anim_review_pack`, revisore + critico cieco | PASS / FAIL (solo PASS va all'owner) |

## 4. Controlli in tre classi
- **Fisici, sempre bloccanti:** regola 15b (caviglie >= 10 cm, punte nel range stock, ossa <= 5%, tremolio, compenetrazioni),
  conservazione (un fix che riduce ampiezza, velocita' o root motion oltre il 15% e' FAIL), blocco delle approvate,
  `hold_reason`, ASSERT della cattura.
- **Vita** (`animpipe/life.py`, unica fonte): bloccano solo le metriche con la prova in `life_gate.json` (2A); le altre
  sono misure mostrate al revisore. Una misura `WOULD FAIL` non si ignora: il revisore la guarda a video e la cita.
- **Principi** (`contract/principi-review.md`, PR-01..20, metodo §13): giudizio motivato, mai soglia.

## 5. Chi esegue (profili)
| Profilo | Authoring | Passi e cancelli | Sguardo di ciclo | Review finale |
|---|---|---|---|---|
| standard | Claude Sonnet/Opus via MCP `gstn-anim-mcp` o carta | script a 0 token | Opus medium | Opus high + critico cieco Opus high |
| cheap | DeepSeek flash (intento JSON via MCP; fallback: solo lo script `.py`, §6) | script a 0 token (`Invoke-RunStartPipeline.ps1`, `Run-Timed.ps1`) | Sonnet/Opus medium | uguale a standard |
| Antigravity | Gemini (pose e intento) | script a 0 token; git lo fa la sessione Claude | Neon/Pro se ci sono crediti | uguale a standard |
La review finale non cambia col profilo: la vecchia variante "2 griglie .jpg finali" (branch claude/runstart-generic-desktop
230ad65a) era uno sguardo di ciclo, non una review finale (INV-08, regola 10). Fable solo dopo 2 fallimenti di Opus.

## 6. Strumenti
- **Authoring semantico via MCP `gstn-anim-mcp` (preferito per modelli `flash`/Sonnet):** 11 tool in ordine fisso (prefisso stabile per la cache dei prompt), sopra il motore UE 5.9. Risposte compatte (<= 5 righe, dettagli in file) con `compact: true`.
  1. `anim_inspect(asset, frame_range)`: altezze, angoli, clearance, stato gate (dump `joints_*.json`).
  2. `anim_apply_intent(src, dst, intent_json, dry_run, allow_geom)`: modifica per intent JSON con snapshot; `follow_through` (catene braccia/testa, massa, overshoot, ancora hit/faf) e' generico; i parametri geometrici (`arm_clr_deg`, `torso_bank_deg`, ...) sono SOLO per clip run-start (nome con `RunStart`), altrimenti rifiutati salvo `allow_geom=true` (gli assi vengono dal moto del bacino: sbagliati sulle mosse sul posto).
  3. `anim_run_intent(src, dst, intent_json)`: snapshot + intent + gate in un colpo, PASS/FAIL; registra il rapporto nel manifest.
  4. `anim_gate_check(asset, ref_asset)`: `runstart_pivot_gates` + conservazione <= 15% + `clipcheck`.
  5. `anim_capture_ab(...)`: cattura A/B del run-start (`mv_run_start_v5`, `grid_pivot_v84_116`): solo locomotion.
     Il gate run-start stampa anche la misura di fase braccio-gamba (`phase_arm_leg` in `gates_<TAG>.json`, r > 0 =
     opposizione; w2 0,63, w3 0,67): non blocca finche' non c'e' la prova di taratura (2A).
  6. `anim_register_variant(asset, variant_id >= 40)` / `anim_unregister_variant`: solo chooser run-start; 30-39 sono le righe Handoff delle varianti approvate 20-29.
  7. `anim_rollback(asset)`.
  8. `anim_capture_move(kit, sequences, variant_runs, ..., fast=True, final_round=False)`: **cattura di una mossa di combattimento** (montage da abilita'), base + variante in UNA sessione editor (`pie_capture_timed.ps1 -Assert`, `PIE_CAPTURE_RUNS`), viste `az90,az180,game`, output `docs/notes/reviews/<move>/<round>/` (mp4 real + slow4x, una griglia per vista con riga base sopra e variante sotto, fitta attorno ad `anchor` e `faf`, piu' il pacchetto del livello 2 in `final/`: slowmo di tutta la mossa nelle 3 viste, schede velocita' di ogni clip, pose chiave grandi, brief del critico cieco). Con `dry_run=true` restituisce solo cancello e piano. Cattura veloce (`fast=True`, motore GSTNCapture, finestra attorno alla pressione, base dalla cache) solo per gli sguardi di ciclo; `final_round=True` (il giro che va alla review finale) cattura TUTTA la mossa con la catena completa e le run hitbox (INV-03, nessun frame scoperto). **Pose da carta** (animpipe author/build/check, senza `anim_run_intent`): nella variante `"card": "Tools/AnimPipeline/cards/.../x.json"` al posto di `intent_hash`; il rapporto e' il `animpipe check` PASS piu' recente di carta e build (`card_report`). Ogni run puo' avere il suo `"kit"` (base e variante in una sessione) e il suo `"assert"`. Scrive sempre `gate_report.json` (anche se FAIL) e `alignment.json`: griglie e video allineati all'inizio del montage di ogni run. Hitbox: una run con `"hitbox_view": true` (cvar `gstn.Combat.Debug.Hitboxes 1`) resta FUORI dalla review dell'animazione e ha i suoi video con le hitbox disegnate in `<out>/hitbox/`, solo per l'owner (forme loggate per frame, `capture_overlay.py`; serve un editor compilato con `GetDebugHitboxShapes`). Griglie e video di review restano puliti.
  9. `anim_silhouette_check(asset, frame_range, views, base_asset, out)`: silhouette per vista (az90, az180, az315, game). "Illeggibile" = **palla informe**: nessuna estremita' esce dal torso di >= 12 cm e il contorno e' tondo (circolarita' >= 0,65); la riga FINDING dà la metrica (sporgenza massima, circolarita'). Le estremita' vicine al torso sono solo elencate (un tuck non e' illeggibile per questo). PNG + `silhouette.json` in `out` (la cartella della review, da committare) o in Saved.
  10. `anim_motion_overlay(asset, frame_range, views, base_asset, out, holds)`: scie di polsi/caviglie/testa/bacino con frecce di velocita' (una per frame) e tangenti; segnala rotture d'arco (> 60 gradi), pop (velocita' > 3x i vicini) e **FERMATE** (tutto il corpo < 3,5 cm/frame fuori dalle tenute dichiarate: pallino rosso sulle scie, banda rossa nella **striscia della velocita'** per frame sotto le scie, linea nera = corpo intero, tratteggio = soglia); PNG + JSON. `anim_capture_move` le genera da solo per OGNI mossa (`velocity_<run>_az90/az180.png`).
  - Gli ultimi due si leggono dai dump `joints_*.json` (per le mosse di combattimento: `clip_joints_dump_ue.py`, non ancora provato in UE) e **i loro PNG vanno nel pacchetto di review** insieme alle griglie.
- **Percorso mossa di combattimento (ordine dei cancelli, tutti a 0 token, falliscono in secondi):**
  1. `anim_apply_intent ... dry_run=true` (obbligatorio: l'intent hash va nel manifest `Saved/AnimPipeline/manifest/<asset>.json`);
  2. `anim_run_intent` vero: FOLLOW_THROUGH report OK + conservazione OK + gate OK = PASS (altrimenti `anim_rollback`);
  3. rilancia lo script hitbox/bake se il `.uasset` e' cambiato (`Saved/AnimR2/*hitboxes*.json` con lo sha256 corrente);
  4. `anim_capture_move dry_run=true`: controlla soglia di leggibilita' (>= 2 cm o >= 3 gradi su mano/piede/testa nella finestra, altrimenti "troppo piccolo da leggere", niente cattura) e contatore tentativi (max 2 per obiettivo, il 3o e' rifiutato);
  5. `anim_silhouette_check` + `anim_motion_overlay` (offline, gratis);
  6. `anim_capture_move` vero: la base si riusa dalla cache se clip, sequenza, cvar, viste e build (hash di Source/ e Plugins/*/Source) sono uguali; stesso intent hash gia' catturato = nessuna nuova cattura. Build solo se `manifest.needs_build()` (hash C++ diverso dal timbro).
  Il revisore riceve PRIMA il pacchetto del livello 2 `final/review_pack.json` (ordine: mp4 reale e slow4x, slowmo di tutta la mossa az90/az180/game, schede velocita' di ogni clip, pose chiave grandi; tenute dichiarate da giudicare a video), poi il pacchetto di `pack.json`, in quest'ordine: **riquadro slowmo dello startup** (`slowmo_<seq>_az90.png`, un tick per cella, base sopra e variante sotto), **2 schede velocita'** (az90, az180), poi **tutte** le griglie; in piu' gli **mp4 slow4x** di base e variante (`pack.json` `slowmo_mp4`), da guardare prima dei numeri. Il riquadro slowmo vale come campioni in sequenza se il revisore non legge i video. Niente della review finale si toglie per stare nelle 8 immagini: se il pacchetto supera 8 (`over_budget`), resta intero e si scrive nell'handover (owner 2026-11-23/24: stessa qualita' della OG, la velocita' si guadagna negli strumenti, non tagliando il giudizio).
- **Cancello anti-fermata (`no_stop`) e vita:** ogni clip elencata in `clips` della run (startup, loop, settle, atterraggio:
  elencarle TUTTE, anche quelle ereditate dalla base) passa `gstn_anim_mcp/no_stop_gate.py`, che chiama `animpipe/life.py`
  (unica fonte di fermate e metriche di vita; la usa anche `animpipe check`). Tenute: `no_stop.holds {clip: "a-b"}` con
  `no_stop.hold_reasons {clip: motivo}`; senza motivo la tenuta non vale (`hold_reason`, blocca sempre). Ogni metrica
  (`stops`, `still_runs`, `hold_alive`, `stop_sync`, `cascade`, `impact`, `anticipation`) BLOCCA solo se e' in
  `Tools/AnimPipeline/life_gate.json`, scritto solo da `python life_calibrate.py --write-gate` su un giro completo
  (owner 2A); finche' non c'e' la prova e' una misura `WOULD FAIL ...` che il revisore legge. Un falso allarme si
  corregge in `life.py` e si ritara, non si spegne (INV-10).
- **Fallback Authoring Python (`run_deepseek` — `deepseek-v4-pro` / `deepseek-flash`):** scrive/modifica **solo** lo script `.py` di posa in `Tools/AnimPipeline/` dentro una sparse worktree (`Tools/CI/New-DeepSeekWorktree.ps1`) + `python -m py_compile`.
  - Timeout corto: `90–120s`, `disable_search_tools=true` (disabilita `Glob`/`Grep` e letture `Saved/`/`Content/` per evitare stalli su repo UE grandi).
  - Non deve lanciare `Save-Assets.ps1`, catture GPU o `ffmpeg` dentro la sessione LLM.
  - Per task >120s via MCP usare `async_mode=true` + `bridge_job_status` oppure `cli.py` direttamente.
  - Nei prompt di esclusione EULA è sicuro scrivere *"Non toccare UE_5.9 / Plugins/GASNPP"*, ma restano vietati accessi reali a path Epic.
- **Pipeline deterministica (`0` token, ~2.5 min):**
  - Eseguire `Tools\AnimPipeline\cards\runstart\Invoke-RunStartPipeline.ps1` (passando `-WorktreeDir` se si parte da una sparse worktree, che usa automaticamente `Tools\CI\Run-SparseAnimStep.ps1` per eseguire headless su `GASP_Giustino2_ue59` e riportare `.uasset`, `joints_*.json`, `gates_*.json` e griglie `.jpg`/`.mp4` nella worktree lasciando pulito `_ue59`).
  - L'ordine deterministico (hard gate bloccanti a 0 token) è: authoring UE → `runstart_clip_joints_ue.py` → `runstart_pivot_gates.py` (`GATES: OK`) → `clipcheck` + `Conservation Guard (<= 15%)` → `runstart_batch_captures.ps1` (`ASSERT: OK`) → `runstart_ab_video.py`.
  - **Viste e artifact obbligatori:** mantenere **sempre** `az180,az90,game` (**MAI** togliere `az180`: un gomito "ad ala" a 90° passa inosservato in `az90`), generare la griglia fitta sul tratto critico (`grid_pivot_v84_116.jpg`, passo 4 frame) e i video `_slow4x.mp4`.
- **Giudizio (regola 3 del 2026-11-23):** giudice **Opus high** + **critico cieco Opus high** (subagente indipendente, mai chi ha editato; [critico-estetico.md](../../../docs/notes/review/critico-estetico.md) + [rubric.md](../../../Tools/AnimPipeline/contract/rubric.md) + [principi-animazione-corporea.md](../../../docs/notes/anim-theory/principi-animazione-corporea.md)), **una sola passata** sul pacchetto completo del livello 2 (`final/review_pack.json`, `complete: true`). **Fable solo dopo 2 fallimenti di Opus.** Gemini Neon solo se i crediti sono confermati. Prima la vita (leggibilita', silhouette), poi i tecnici.
- **I 10 controlli anti-spreco:** (1) dry run obbligatorio con stesso intent hash; (2) cancello prima della cattura (report PASS, hitbox fresche); (3) soglia di leggibilita'; (4) cache della base e dell'intent gia' catturato; (5) build solo se cambia il C++; (6) una sola sessione editor per base, variante e tutte le viste; (7) pacchetto review generato da strumenti a 0 token (`anim_review_pack`): il risparmio e' nel prepararlo, mai nel tagliarlo (INV-09); (8) max 2 tentativi per obiettivo nel manifest; (9) costi automatici: minuti per passo e `usage` nel manifest (`manifest.record_step`, `manifest.costi_line` produce la riga di `handoffs/costi.csv`, la aggiunge il coordinatore); (10) prefisso tool stabile + risposte compatte.

## 7. Non fare
- Aggiungere chiavi alla carta per rifinire (POLISH lavora a strati); toccare clip approvate (K7 resta il caso rovinato
  da confrontare, w2, w3, v4r5 restano bloccate).
- Promuovere da numeri o griglie (INV-08); tagliare la review finale per costo (INV-09, regola 17).
- Nuovi controlli bloccanti senza prova di taratura (2A); spegnere o ammorbidire un controllo che sbaglia (INV-10).
- Usare Blender PROTO o un generatore alternativo in review (regola 6).
