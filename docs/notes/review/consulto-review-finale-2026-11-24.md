# Consulto: perché ogni "snellimento" toglie la review finale, e come impedirlo (2026-11-24, cloud, Opus high, sola lettura)
Fonti: OG = `Tools/AnimPipeline/contract/archive/gstn-move-animation_2026-11-15.md`; oggi = branch btrk7b 7af96c4a
(`gstn-anim-cycle-cheap/SKILL.md` "cheap", `gstn-anim-reviewer/project.md`, `capture_move.py`); confronto aeh8di 78b6f2f5; slowmo qsgab4 d7222a0a;
`decisioni/istruzioni-progetto-2026-11-15/20/23.md`; `handoffs/archivio/2026-11/anim-skill-riferimenti.md`.

## 1. Cosa è stato tagliato ogni volta (e quale funzione della review finale è sparita)
| Quando | Taglio (prova) | Funzione persa |
|---|---|---|
| 15/11 | Regola 4: "review video max 8 immagini", "una griglia per vista, guardata una volta" (istruzioni-15 r.7) | copertura di TUTTI i frame: la regola 10 dice "frame uno per uno", la 4 la limita a 8 |
| 15/11 | OG archiviata: spariscono "guarda TU le immagini grandi prima dei numeri" (OG r.465-468), mp4 normale + rallentato (OG r.430), tenute/fermate/cascata misurate (OG r.207-208, r.403-406) | il movimento non si guarda più, si guardano pose |
| 15/11 | Ciclo revisore/esecutore: "video in gioco solo inizio e fine" (riferimenti r.12) | il video in gioco diventa un controllo di partenza/arrivo, non il giudizio |
| 20/11 | Regola 4: "il subagente di review riceve solo griglie, criteri e brief" (istruzioni-20 r.7) | niente video al giudice |
| 23/11 | Cheap: "max 8 immagini fitte attorno ad anchor e FAF" (cheap r.19, r.41 p.7), "il revisore riceve SOLO le griglie, mai gli mp4" (r.30) | lo startup e il settle escono dalla finestra: qui stavano le fermate di K7/K8 |
| 23/11 | btrk7b p.3: blocchi "ereditati" dalla base non bloccano e il revisore "non fa FAIL" per loro (`capture_move.py` r.195; reviewer project.md r.73) | il settle K6 (fermo ~6 frame) non viene più giudicato da nessuno |
Risultato: K8 r2 è passato con a1-a5 congelati letti come "porta leggibile"; lo slowmo (5 immagini, ~1,5 $) ha trovato 6 difetti.

## 2. Perché succede (senza difendere le scelte)
- **Il costo si misura solo alla fine.** La riga COSTO conta "token review"; la regola 4 mette un tetto alle immagini. I numeri costano 0 token, quindi ogni snellimento ha gonfiato i controlli numerici (pulizia) e ristretto l'occhio, che è l'unico che vede la vita.
- **La review finale non era marcata come intoccabile.** Nessun testo diceva "questo non si taglia"; per chi snellisce era una voce di costo come le altre.
- **Ogni thread vede solo il brief** (regola 1, "mai rileggere thread vecchi") e non sa perché un pezzo c'era. La OG spiegava il perché (Terry, Ryu); la cheap ha tenuto i comandi e perso i motivi.
- **"Falso positivo" risolto spegnendo.** btrk7b p.3 ha visto 3 blocchi che falliscono uguali su K7 approvato e li ha resi non bloccanti, invece di chiedersi se il controllo era sbagliato o se K7 ha davvero quel difetto.
- **La cheap doveva essere una variante** ("non sostituisce le skill originali", cheap r.8) ed è diventata la strada unica.

## 3. Struttura a due livelli
**Livello 1, sommario, a 0 token o quasi (smaltisce subito).** Cancelli numerici bloccanti (regola 15b + `no_stop_gate` + pop/arm_jump), silhouette, scie, anteprima rapida di 1 griglia: scarta i giri sbagliati PRIMA della cattura. Un esito positivo del livello 1 vuol dire solo "vale la pena guardarlo", MAI PASS.
**Livello 2, review finale completa, non negoziabile.** Sempre, su ogni variante che può andare all'owner:
1. vita prima dei numeri, sul movimento: slowmo a 1 tick per cella su TUTTA la mossa (startup, attivi, recovery, settle), az90 + az180 + game;
2. schede velocità con frecce e scie + striscia velocità per frame (mani, piedi, bacino, testa): fermate e scatti visibili;
3. tratti d'identità (regola 13a) per primi; 4. confronto con la base, stessi frame, anche sulle parti ereditate;
5. critico cieco indipendente. Il tetto non è "8 immagini" ma "nessun frame scoperto" (stima 10-14 immagini, ~50-70k token, ~1,5-2,5 $ a review).
**Come proteggerlo.** a) Sezione `## INVARIANTI (non toccare per costo)` in cima a reviewer, cheap e contratto, con il perché di ogni voce. b) Test `tests/test_review_invariants.py`: fallisce se la sezione manca, se il pacchetto non ha slowmo dell'intera mossa + schede velocità + riga base, o se una regola della skill limita il livello 2 a un numero di immagini. c) Riga nelle istruzioni di progetto (regola 4 e 10): "il tetto immagini vale per il livello 1; il livello 2 è escluso da ogni ottimizzazione di costo; un controllo che dà falsi allarmi si corregge, non si ammorbidisce" (la incolla l'owner).
**Blocchi ereditati (btrk7b p.3), da correggere.** Un blocco che fallisce anche sulla base approvata non diventa "non bloccante" da solo. Due casi: il controllo misura male (si corregge la misura e si ritara su K7, con prova) oppure K7 ha davvero quel difetto (esenzione scritta in `anim-approvate.md` con le parole dell'owner, per mossa e frame). Senza uno dei due, blocca. Il livello 2 giudica comunque le parti ereditate (es. settle K6).

## 4. Risparmi già fatti che restano validi
Cancelli a 0 token prima della cattura, dry run con intent hash, cache della base, build solo se cambia il C++, una sessione editor, contatore tentativi, catture e build a Haiku/Sonnet, script con output su file, hitbox fuori dalla review, whiff prima degli hit, avvisi `pose_coherence`/`arm_jump`, critico senza handover vecchi. Non validi per il livello 2: tetto 8 immagini, "solo griglie mai mp4", finestra solo anchor/FAF, video solo inizio/fine, ereditati automatici.

## 5. Opzioni
- **A, solo protezione, ~2-3 $ cloud.** Invarianti nelle 3 skill + contratto, test `test_review_invariants.py`, correzione degli ereditati (esenzione esplicita o controllo ritarato), testo per le istruzioni di progetto. Il pacchetto resta quello di oggi finché non arriva B.
- **B (consigliata), ~5-6 $ cloud + 1 cattura già prevista.** A + gli strumenti dell'opzione A di aeh8di (`no_stop_gate` bloccante, fermate e striscia velocità in `motion_overlay`, slowmo dell'intera mossa nel pacchetto). Rende il livello 2 reale e il test lo verifica sul pacchetto vero (prova su K7/K8 già catturati: deve trovare i 6 difetti dello slowmo).
- **C, ~9-11 $.** B + metriche di vita della OG (`kb_motion`, branch m6s9y2) + taratura della rubrica sul video. Più completa, tocca tutte le mosse.
Costo ricorrente in più di B: ~1-1,5 $ per review rispetto a oggi; in cambio niente giri sprecati come K8 r2.
