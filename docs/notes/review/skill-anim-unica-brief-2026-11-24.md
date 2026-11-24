# Brief: skill di animazione unica (2026-11-24, thread "principi e skill", Fable; owner ha scelto l'opzione A)
OBIETTIVO: un solo pacchetto che lavora come la chat del w3 (veloce, per principi), non rovina le clip buone e
finisce sempre con la review finale completa. Unificare i branch di stanotte, non costruire altro di parallelo.

## Cause verificate (file e comando)
1. **Perché il w3 è riuscito** (`docs/notes/reviews/runstart-w3-c1|c2/passo-*/istruzione.json`, branch
   claude/runstart-generic-desktop; 8 passi in 35 min, `git log` 592e4d37..00c81ad3): una causa fisica dell'owner;
   istruzioni `livello: principio`; un pezzo per passo con lista `non_peggiorare`; tra i passi solo gate e prima/dopo,
   cattura una volta per ciclo; parametri con inviluppo morbido sopra una clip densa (w2), nessuna chiave nuova.
2. **Perché il polish ha "rovinato" (chiavi a scatti).** Il build da carta mette tangenti piatte su ogni chiave
   (`dropkick-k8-3-diagnosi.md`, 6blrlz). Il polish aggiungeva chiavi alla carta: ogni chiave in più è una fermata.
   K7: 7 chiavi in 16 frame, bacino `3 2 0 0 5...` cm/frame. Stima mia: è questo lo "step" visto dall'owner.
3. **Controlli sovrapposti e non tarati.** Quattro misure delle fermate con soglie diverse (`no_stop_gate` 3,5 cm su
   tutta la clip, falso positivo noto classe D in `fix-stop-checks.md`; `stop_and_go`; `life.py`; `hold_reason`),
   tutte bloccanti, `CALIB: INCOMPLETE 9/13`, nessuna provata in UE. Tetti "max 2 tenute, 6 frame" mai approvati.
4. **Due `gstn-anim-cycle-cheap/SKILL.md` diversi** (runstart 230ad65a: giudizio su 2 griglie; 7zhryd: INVARIANTI).
   Il check di fase braccio-gamba è scritto nelle skill ma non in `runstart_pivot_gates.py` (grep `oppos|phase|fase` = 0).

## Disegno
- **Tre file skill, un contratto.** `gstn-anim-cycle` (prende il posto di cheap: il giro e la tabella "chi esegue"),
  `gstn-anim-reviewer`, `gstn-anim-executor`. Contratto unico in `Tools/AnimPipeline/contract/` (invarianti,
  principi-review, indice-ricerca, rubric). "Cheap" diventa un profilo (DeepSeek flash, script, MCP), non una skill.
- **Due modi di lavoro.** AUTORE: mossa nuova dalle pose dell'owner, carta con `"curve": "spline"` (vjbryc).
  POLISH: migliorare una clip esistente con STRATI (parte, asse nel corpo, valore, frame di picco, entrata e uscita)
  sopra la clip densa, come il w3. Il polish non aggiunge mai chiavi alla carta e non ricostruisce una clip approvata.
- **Tre sguardi.** Passo: solo numeri e prima/dopo. Ciclo: una cattura e uno sguardo rapido (griglie + slow4x) che dice
  solo "altro ciclo" o "pronto", mai PASS. Finale: livello 2 completo (INV-01..13, `anim_review_pack`), una volta.
- **Il giro è quello del w3** (i 5 punti della causa 1), scritto in `gstn-anim-cycle` e uguale per tutti i profili.
- **Controlli in tre classi.** Fisici (regola 15b, conservazione 15%, blocco approvate): sempre bloccanti. Vita
  (`life.py`, unica fonte; `no_stop_gate` la chiama): bloccano solo le voci con taratura provata su approvate + stock,
  le altre sono misure mostrate al revisore. Principi (PR-01..20, metodo §13): giudizio motivato, mai soglia.
- **MCP delle pose per agenti flash (mai iniziato).** Tool `anim_layer`: lo strato qui sopra come JSON, assi dal
  corpo del personaggio (non dal moto del bacino, limite di oggi di `geom_intent_ue`), `dry_run`, snapshot, rollback.
  L'agente scrive l'intento, lo strumento fa i quaternioni. Riusa `compute_envelopes`, manifest, `anim_run_intent`.

## Thread (uno per checkpoint; nessuno modifica clip approvate)
- **T0, fisso, Haiku low, 1 $.** Elenco di ogni clip approvata con dove sta la copia (branch `claude/approved-*`,
  carta, .casc); righe mancanti in `anim-approvate.md` (w3, jab v12, ftilt v3.1, dair) portate al coordinatore;
  `Tools/CI/Test-GitGuard.ps1` verde. I tag `approved/*` non esistono: la guardia li blocca.
- **T1, cloud, Opus medium, 6 $.** Branch `claude/anim-skill-unica` da 7zhryd. Merge sgky92 (conflitti previsti:
  `animpipe/card.py`, `animpipe/checks.py`, `docs/architecture/anim-pipeline.md`), bazqb1 (`capture_move.py`),
  jlb6i3, runstart-generic-desktop (i due SKILL.md), 3ufz10. `no_stop_gate` punta a `life.py`. `gstn-anim-cycle`
  scritto, cheap rimosso. Check di fase scritto davvero. Test offline verdi tranne gli 11 d'ambiente già noti.
- **T2, cloud, Opus high, 5 $.** `anim_layer` + test offline + prova a secco con un modello flash su 3 istruzioni
  vere del w3 (c1 passo-02, passo-03, c2 passo-03): l'intento prodotto deve dare gli stessi numeri degli `esito.json`.
- **T3, fisso, Sonnet medium + subagenti Haiku, 8 $, ~90 min.** Unico thread di integrazione: build (GSTNCapture),
  test, dump mancanti e `life_calibrate.py` fino a `CALIB: OK`, build spline vero, `anim_layer` in UE, poi la PROVA.
- **T4, cloud, Opus high + critico cieco, 2,5 $.** Review finale livello 2 della prova. Solo dopo il PASS riparte il polish.
- **Prova consigliata:** dropkick K6 (buona) + 2-3 strati di polish, confrontata con K7 (rovinata) nello stesso video.
  Mostra proprio che il polish non rovina più. Render veloce: luce semplice contro non illuminata, stesso frame.

## Decisioni dell'owner: DECISE 2026-11-24 01:22 (Roma), "usiamo le A" = 1A 2A 3A. Testo per le regole 10 e 17 in
## `handoffs/proposta-regole-10-17-2026-11-24.md` (lo incolla l'owner; finché non è incollato vale questa decisione)
1. Tenute. A (consigliata): tenuta viva = strumento normale, dichiarata con motivo e guardata a video; difetto = tenuta
   morta e stop-and-go; i tetti (2 tenute, 6 frame) diventano numeri guida per l'occhio. B: ogni tenuta è eccezione.
2. Taratura. A (consigliata): un controllo nuovo nasce come misura e blocca solo con la prova di taratura
   nell'handover. B: blocca dal primo giorno (com'è oggi, con i falsi allarmi di stanotte).
3. Prova. A (consigliata): dropkick K6 a strati. B: turnaround 180 L.

## Non fare
Nuovi controlli o nuove skill parallele; toccare K7, w2, w3, v4r5; usare Blender PROTO in review (regola 6).
STIMA TOTALE: ~22-23 $ (T0-T4), più di quanto detto all'owner (15-18 $) perché l'MCP delle pose è da fare da zero.
SUBAGENTI: 0
COSTO di questo thread: ~9 $ (stima) | 0 minuti desktop | 0 token review
