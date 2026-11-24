# INVARIANTI della review delle animazioni (non toccare per costo)

Owner 2026-11-24 (ambito: tutta la skill di animazione e la sua review): qualita' uguale alla skill OG
(`contract/archive/gstn-move-animation_2026-11-15.md`), nessun compromesso sui falsi positivi, si risparmia solo con
strumenti automatici e processi piu' rapidi, MAI togliendo passi di review o di giudizio. Consulto:
`docs/notes/review/consulto-review-finale-2026-11-24.md` (branch claude/project-thread-39uwfb).

Questa pagina e' la fonte. Le skill `gstn-anim-reviewer`, `gstn-anim-executor` e `gstn-anim-cycle` ne copiano gli
identificativi in una sezione `## INVARIANTI`; `Tools/AnimPipeline/tests/test_review_invariants.py` fallisce se una
sezione, un identificativo o un passo del pacchetto manca, o se una skill mette un tetto di immagini al livello 2.
Le cambia solo l'owner, con una decisione scritta. Chi snellisce la skill le lascia intatte.

## Due livelli
- **Livello 1, sommario (0 token o quasi):** cancelli numerici bloccanti (regola 15b, `no_stop`, `stop_and_go`,
  `animpipe check`), silhouette, scie, anteprima rapida. Serve a SCARTARE un giro prima della cattura o della review.
- **Livello 2, review finale completa:** sempre, su ogni variante che puo' arrivare all'owner. Il materiale lo prepara
  `gstn_anim_mcp/final_review_pack.py` (tool MCP `anim_review_pack`): `final/review_pack.json` con `complete: true`, o
  niente review.

## Gli invarianti (con il perche')
- **INV-01 Identita' per prima.** I 2-3 tratti d'identita' del brief (regola 13a) si giudicano prima di tutto; un tratto
  cambiato e' FAIL finche' l'owner non decide. Perche': senza, una variante "piu' bella" perde la mossa.
- **INV-02 Vita prima dei numeri, sul movimento.** Si guardano gli mp4 a velocita' reale e slow4x di base e variante
  PRIMA di leggere numeri o righe di check: anticipazione, arco, peso, follow-through, root coerente. Perche': K8 r2 e'
  passato sui numeri con a1-a5 congelati; lo slowmo (5 immagini) ha trovato 6 difetti.
- **INV-03 Slowmo di tutta la mossa nelle 3 viste.** Un tick per cella su TUTTA la finestra (startup, attivi, recovery,
  settle) in az90 + az180 + game, base sopra e variante sotto. Nessun frame scoperto (`coverage` nel pacchetto).
  Perche': la finestra solo anchor/FAF ha nascosto le fermate dello startup di K7/K8 e il settle K6.
- **INV-04 Schede velocita' con fermate e scie per OGNI clip** (startup, loop, settle, atterraggio), az90 + az180:
  una freccia per frame, pallini e bande STOP, striscia della velocita'. Perche': una fermata in una griglia sembra una
  "posa tenuta".
- **INV-05 Immagini grandi delle pose chiave**, per vista, base accanto alla variante, guardate prima dei numeri;
  percorsi delle ossa (scie); controlli di posa della OG (pugno visibile in %, gomito dietro dentro la sagoma, rotazioni
  di avambraccio e mano, ginocchia, piedi). Perche': OG passi 4-6.
- **INV-06 Confronto prima/dopo con la base**, stessi frame allineati al montage, ANCHE sulle parti ereditate (settle,
  loop). Perche': il settle K6 ereditato non l'ha piu' giudicato nessuno.
- **INV-07 Critico cieco.** Un subagente indipendente, che non ha editato la mossa, riceve solo pacchetto, criteri,
  brief e tratti d'identita' (`final/critico_brief.md`) e giudica senza vedere il verdetto del revisore.
- **INV-08 Il livello 1 scarta, non promuove.** Un esito positivo dei controlli automatici vuol dire solo "vale la pena
  guardarlo". PASS viene solo dal livello 2.
- **INV-09 Nessun tetto di immagini sul livello 2.** Il tetto e' "nessun frame scoperto" (stima 10-14 immagini piu' gli
  mp4). I tetti di costo (regola 4, "8 immagini", "una griglia guardata una volta") valgono solo per il livello 1. Un
  pacchetto grande resta intero e si scrive nell'handover.
- **INV-10 Un falso allarme si corregge, non si ammorbidisce.** Un controllo che sbaglia si corregge nella misura e si
  ritara su una versione approvata, con la prova nell'handover. Nessun interruttore lo spegne. Un blocco che fallisce
  identico sulla base approvata BLOCCA lo stesso: o il controllo misura male (si corregge), o la base ha quel difetto
  (si corregge in una variante, oppure l'owner scrive un'esenzione nella sezione "Esenzioni dei controlli" di
  `docs/notes/anim-approvate.md`, con le sue parole, per base, blocco e riga esatta). Un controllo NUOVO nasce come
  misura e blocca solo dopo la prova di taratura (owner 2A 2026-11-24: passa le approvate e lo stock, boccia un caso
  noto cattivo): per le metriche di vita la prova e' `life_gate.json`, scritto da `life_calibrate.py --write-gate`.
  Perche': i quattro controlli delle fermate della notte bloccavano senza taratura e davano falsi allarmi.
- **INV-11 Metriche di vita e controlli di posa della OG** fanno parte del livello 2: impact spacing, tenuta + rinculo,
  cascata, arc jitter, fermate (numeri dagli strumenti, giudizio dall'occhio).
- **INV-12 Tenute vive si', tenute morte e stop-and-go no** (owner 1A 2026-11-24, sostituisce "le fermate sono
  eccezioni"). Una tenuta VIVA (bacino o testa continuano a muoversi, `hold_alive`) e' uno strumento normale (Sakurai),
  dichiarata CON il motivo scritto: nella carta `life_holds [{frames, why}]`, oppure `why`/`hold_reason` sulla posa con
  `"hold": true`, o `hold_reasons {frame: motivo}` per `holds`; nella cattura `no_stop.hold_reasons {clip: motivo}`.
  Senza motivo la tenuta non vale (blocco `hold_reason`, sempre). I difetti sono la tenuta MORTA (un blocco di cemento)
  e lo stop-and-go. 2 tenute e 6 frame (12 su uno smash) sono numeri guida per l'occhio (`hold_guides`), non blocchi.
  La dichiarazione NON salta il giudizio: la review guarda ogni tenuta sul video e la boccia se non si legge come voluta.
- **INV-13 I principi si considerano, uno per uno.** Ogni voce di `contract/principi-review.md` (principi del corpo,
  dei picchiaduro e note di Sakurai della OG, con il documento accanto) e' scritta in `final/principi.md` col metodo
  §13: intento, previsione, osservato (frame e vista), scostamento e se e' voluto, giudizio; oppure `non pertinente` col
  motivo. Sono PRINCIPI, non regole (owner 2026-11-24): uno scostamento voluto e motivato resta accettato; il controllo
  `principi_review.py check` verifica che il ragionamento ci sia, mai che un numero rientri. Senza `PRINCIPI: OK` niente
  PASS, per il revisore e per il critico cieco. Perche': la skill citava i documenti "solo la sezione che stai per
  citare" e le note di Sakurai non erano nominate da nessuna skill, quindi nessuno li controllava davvero.

## Esenzioni
Solo l'owner. Formato della riga in `docs/notes/anim-approvate.md`, sezione `## Esenzioni dei controlli`:
`| <asset base> | <blocco> | <riga esatta di animpipe check> | owner <data>: "<parole dell'owner>" |`.
`stop_and_go` e `hold_reason` non si esentano mai.
