# Bibliotecario della documentazione: giro del 2026-11-27

Secondo giro. Branch `claude/docs-bibliotecario-2026-11-27`, creato da `main` ca2a8952 (il branch del 23-11 è già unito in main).
Niente è stato cancellato o spostato; nessun file di GASP o locale dell'owner toccato; nessun artefatto modificato.
Toccati solo: `docs/INDICE-PROGETTO.md`, `docs/STATO-ATTUALE.md` (blocco "righe smentite"), questo file, l'handover e i due file
generati `handoffs/INDICE.md` / `handoffs/costi.csv` (rifatti con `Tools/CI/Build-Indice.py`, mai a mano).

## Rapporto in 10 righe
1. GRAVE (corretto il 27-11 sera): la copia pubblicata `Content/AnimPipeline/Published/dair/v9/AM_OverlordDAirV9.uasset` è cambiata dopo la pubblicazione (98dab88a, riordino del 24-11: risalvata per i riferimenti). Lo sha256 di oggi (6d4fe627, dal puntatore LFS) non è quello di `publish.json` (dc945baf), e il controllo `regress.py` non se ne accorge perché la riga del registro (`AS_OverlordDAirV9 + AM_OverlordDAirV9`) non viene risolta in file. Lo spostamento dell'AS invece è già coperto dalla prova ossa (scelta B del 24-11, `docs/notes/approved-dumps/moves/`, commit e87ae944): il gate non è in FAIL, come avevo scritto per errore nella prima versione. Decisione 1.
2. Dal 23-11 sono entrate in main 20 PR (#38-#59): M12 completo, presa al collo e spin di Ganondorf, MCP animazione, riordino Content 2b, capsula greybox, lanci, kit dei personaggi. Main è stato compilato il 24-11 (1937 test); dopo il 27-11 nessun handover dice di una build di main intero.
3. I tag approvati sul remoto sono 9 e coincidono con `docs/notes/anim-approvate.md`. Il 29w3 c'è come `approved/runstart-w3`; `STATO-ATTUALE` §3 era rimasto indietro (segnato).
4. `STATO-ATTUALE` ha due copie che dicono cose diverse: quella nei project files (26-11) ha lo stato del giorno ma le decisioni vecchie; quella nel repo ha le decisioni giuste ma niente stato del giorno. Decisione 2.
5. Le regole 1-16 riassunte in `STATO-ATTUALE` §1 sono state sostituite il 24-11 dalle 8 regole di `docs/notes/regole-progetto.md`. `CLAUDE.md` è già giusto; `STATO-ATTUALE` e `BRIEFING-ANTIGRAVITY.md` del repo no.
6. `CLAUDE.md:71` dice che `feature/gas-npp-5.8` è archiviato con il tag `archivio/feature-gas-npp-5.8`: il tag non esiste e il branch è ancora sul remoto. Decisione 3.
7. Gli artefatti sono 23, non 11: uno è nuovo (piano "tutto il gameplay nel rollback", 25-11), 12 non erano mai stati indicizzati, 3 vecchi non compaiono più nell'elenco. L'indice ora li elenca tutti.
8. Documenti in `docs/` senza un indice: 152 su 200 (erano 130 su 167); uno solo non lo cita nessuno (`mesh-pipeline-opzioni.md`). Le voci degli indici puntano a file esistenti, tranne `mappa-cartelle.md` (solo su branch).
9. 36 branch non uniti hanno lavoro di questa settimana; 13 documenti stanno solo lì (sezione 6 dell'indice). 7 handoff vecchi nei project files non li cita più nessuno (sezione C).
10. `handoffs/INDICE.md` e `costi.csv` si generano dagli handover (decisione 2A del 24-11): li ho rifatti con lo script e non ho toccato a mano la vecchia copia nei project files, ferma al 24-11.

## Decisioni per l'owner (max 3)
1. **dair v9 dopo lo spostamento.** L'owner ha scelto A (tag nuovo `approved/dair-v9-2b`) il 27-11 su una premessa sbagliata (gate in FAIL): il tag non chiude il buco vero, che è il controllo della copia pubblicata. Riproposta con le opzioni corrette nel thread; nessun tag creato finché non risponde.
2. **Una sola copia di STATO-ATTUALE.** A (consigliata): il repo tiene le decisioni e i project files tengono solo lo stato del giorno (§0 e coda del fisso), con una riga in cima a ciascuna che rimanda all'altra; le decisioni vecchie nella copia dei project files vengono tolte (~0,5 $, coordinatore). B: tutto nei project files e il repo rimanda lì (0,3 $). C: lasciare due copie (0 $, ma un thread legge la versione sbagliata).
3. **`feature/gas-npp-5.8`.** A (consigliata): da un PC si crea il tag `archivio/feature-gas-npp-5.8` su e79e401a, come dice `CLAUDE.md`; il branch resta finché non decidi tu di cancellarlo (~0,1 $). B: si corregge solo `CLAUDE.md` scrivendo "esiste ancora, non usarlo" (~0,1 $). Cancellare il branch non si può annullare: non lo propongo.

## A. Documenti orfani
Metodo: ogni `.md` sotto `docs/` (esclusi `handovers/`, `reviews/`, `archivio/`: 200 file) cercato per nome in `docs/INDICE-PROGETTO.md`,
`docs/DOVE-METTO-MANO.md`, `docs/notes/audit-codice.md`, `CLAUDE.md`, `docs/notes/README.md`, `Plugins/README.md`, `Artifacts/README.md`,
`docs/STATO-ATTUALE.md`. **152 non compaiono in nessun indice** (elenco completo con le citazioni: rifare con lo script in fondo).

**Orfano vero (nessuno lo cita):** `docs/notes/mesh-pipeline-opzioni.md` (ricerca del 23-11). Proposta: citarlo in `INDICE-PROGETTO` §5.13 accanto alle ricerche skin.

**Nati questa settimana e non indicizzati (21)** (tra parentesi quanti altri file li citano):
`docs/architecture/plans/mmo-scalabilita.md` (2), `docs/architecture/verbs-vs-commands-guide.md` (3), `docs/notes/anim-mcp-costo-velocita.md` (3),
`anim-skill-test-piano.md` (1), `assistant/2026-11-21_pivot_overlord.md` (2), `audit-hit-generici-2026-11-23.md` (1), `audit-stop-2026-11-24/tabella.md` (1),
`bibliotecario/handoffs-INDICE-2026-11-23.md` (2), `dropkick-k7-skill-test-brief.md` (1), `greybox-capsula.md` (3), `mesh-pipeline-opzioni.md` (0),
`mosse/auto-cancel-landing-proposta.md` (1), `pulizia-2026-11-24/brief-checkpoint.md` (2), `pulizia-2026-11-24/piano.md` (16),
`review/consulto-review-finale-2026-11-24.md` (2), `review/proposta-regole-10-17-2026-11-24.md` (2), `review/proposta-regole-4-10-2026-11-24.md` (1),
`review/skill-anim-unica-brief-2026-11-24.md` (7), `review/skill-og-vs-oggi-2026-11-23.md` (1), `skin-lyra-confronto.md` (3), `tempi-import-cattura-2026-11-23.md` (4).
Proposta: `mmo-scalabilita.md`, `verbs-vs-commands-guide.md`, `greybox-capsula.md` e `auto-cancel-landing-proposta.md` vanno in `INDICE-PROGETTO`
(sono documenti di sistema o di design vivi); `pulizia-2026-11-24/`, `review/proposta-regole-*` e i consulti sono storia: archivio.

Resta valida la proposta del 23-11: `docs/architecture/` intero come tabella in `INDICE-PROGETTO` §7; spike report e doc-deltas di M3-M8 in archivio.

## B. Voci degli indici senza file
Metodo: ogni percorso tra backtick o link in `INDICE-PROGETTO`, `DOVE-METTO-MANO`, `audit-codice`, `CLAUDE.md`, `docs/notes/README.md`,
`Plugins/README.md`, `STATO-ATTUALE`, cercato nel repo (per nome del file, così valgono i prefissi brevi `Core/`, `NP/`, `Prj/`).
- `docs/INDICE-PROGETTO.md:119` (ora riga spostata, §5.2) `docs/notes/mappa-cartelle.md`: esiste solo su `claude/project-thread-f4ovn9`.
- `docs/STATO-ATTUALE.md` §1 `decisioni/istruzioni-progetto-2026-11-15/20.md`: è nei project files (`decisioni/`), non nel repo; va detto.
- `docs/STATO-ATTUALE.md` §1 `docs/notes/regole-progetto-2026-11-20.md`: spostato in `docs/notes/archivio/`.
Nessun altro percorso rotto.

## C. Handoff vecchi da archiviare (project files, proposta: nessuno spostamento fatto)
Criterio: file in `handoffs/` (82 voci) modificati entro il 20-11 che nessun documento del repo, `CLAUDE.md` o la memoria cita. Candidati **7**,
destinazione `handoffs/archivio/2026-11/` (come il 23-11, reversibile): `camera-platforming-design-handover.md`, `clip-isolation-1.md`,
`dash-ganon-1-handover.md`, `dossier-riferimento-2026-11-19.md`, `run-stages-leggibili-1-handover.md`, `run-start-frantic-1-handover.md`,
`vfx-teoria-handover.md`. Gli altri 15 vecchi sono ancora citati e restano.

## D. Contraddizioni
| # | Cosa | Dove (file:riga) | Contro | Proposta |
|---|---|---|---|---|
| D1 | Copia pubblicata cambiata e controllo cieco | `Content/AnimPipeline/Published/dair/v9/AM_OverlordDAirV9.uasset` (98dab88a; sha 6d4fe627 contro `publish.json` dc945baf) | regola 5 (approvate bloccate); `Tools/AnimPipeline/capdet/regress.py:111-122` non risolve l'asset `AS_... + AM_...` del registro | decisione 1 |
| D2 | Regole in vigore | `docs/STATO-ATTUALE.md` §1 (regole 1-16) | `docs/notes/regole-progetto.md:1` (8 regole del 24-11) | riscrivere §1 (coordinatore, ~0,3 $) |
| D3 | Tag 29w3 e approvate nuove | `docs/STATO-ATTUALE.md` §3 ("29w3 NON sul remoto") | `git ls-remote --tags`: `approved/runstart-w3`, `jab-v12`, `ftilt-v31`, `dair-v9` | allineare §3 a `anim-approvate.md` |
| D4 | Due STATO-ATTUALE | repo `docs/STATO-ATTUALE.md` | project files `bibliotecario/STATO-ATTUALE.md` (N.S, RB, verdetti 23-26, rete diversi) | decisione 2 |
| D5 | Briefing agenti esterni | `docs/BRIEFING-ANTIGRAVITY.md:13` cita `regole-progetto-2026-11-20/22.md` (ora in archivio) e le regole 10/16 vecchie | copia nei project files del 26-11 ("fino al reset di venerdì 28-11") | portare nel repo la copia del 26-11 o rimandare a lei |
| D6 | Tag di archivio | `CLAUDE.md:71` | `git ls-remote`: niente `archivio/feature-gas-npp-5.8`, branch `feature/gas-npp-5.8` vivo | decisione 3 |
| D7 | File locali dell'owner nel repo | `Content/GSTN/Data/Locomotion/DA_Locomotion_Default.uasset`, `Content/GSTN/Locomotion/StockBase/CHT_GSTN_StockMover_PoseMatch.uasset` tracciati in main | regola 5 "mai committare file locali dell'owner (es. DA_Locomotion_Default)", `CLAUDE.md:65` | non nuovo (tracciati da prima del 21-11), spostato il 24-11 con l'owner (3A). Da confermare la frase: "mai committare modifiche", non "il file non sta in git" |
| D8 | Rete 1v1 | `docs/STATO-ATTUALE.md` §2 "rollback misto" | `docs/architecture/tag-verbi-simulazione-rollback.md:332` decisione 25-11 "niente misto" | riscrivere la riga |
| D9 | Indice dei costi | prompt di questo giro (passi 6 e F: aggiungere righe a mano in `costi.csv`) | decisione 2A del 24-11 e `CLAUDE.md:73` (generati, mai a mano) | ho seguito la 2A; aggiornare il prompt della routine |
| D10 | Test (ancora aperta dal 23-11) | `docs/architecture/testing.md:7,10,38` (94 test, UE 5.8) | 1937 test (24-11), UE 5.9 | lavoro meccanico <0,5 $ |
| D11 | Server dedicato (ancora aperta) | `docs/notes/input-azioni-mappa.md:170` 3A "server dedicato per il ranked" | `server-dedicato-ricerca.md`, decisione 23-11 | segnarla superata |
Chiuse dal 23-11: regole in `CLAUDE.md` (ora puntano a `regole-progetto.md`), branch base `main` in `CLAUDE.md`, tag 29w3 (esiste come `runstart-w3`).

## E. Artefatti
Elenco con lo strumento Artifact (`list`, 27-11): 23 dell'owner + 1 condiviso da altri ("Epub Feed", non del progetto). Nessun artefatto modificato.
- **Nuovo:** "Piano: tutto il gameplay nel rollback" (Doc, 25-11): vivo, è il piano della decisione "niente misto".
- **Cambiati il 23-11** (data di aggiornamento): Mosse di esempio, Architettura plugin, Networking (aggiornati), Guida mosse Blueprint, Analisi regole, Cose da fare a mano (segno ARCHIVIO). Architettura e Networking vanno riletti dopo la decisione del 25-11.
- **Mai indicizzati (12), proposta:** ARCHIVIO per Piano CI GSTN, Skill e riferimenti animazione, Roadmap milestones GASP, Mappa classi GSTNCombat, Moveset stile Smash – Manichino; "storia delle pose" per le 5 pagine Ganondorf Jab/FTilt e Jab Legibility Review; da verificare Guida editor hitbox GSTN e Wall Movement Mode (confronto con `wall-movement.md`); Guida Unreal cristallo e god rays = ricerca di stile, tenere.
- **Spariti dall'elenco:** Strumenti AI, Pipeline agentica, Depth marker (erano ARCHIVIO). Se non li ha cancellati l'owner, va capito chi.
La tabella completa è in `docs/INDICE-PROGETTO.md` sezione Artefatti.

## F. handoffs/INDICE.md
Dal 24-11 (decisione 2A) `handoffs/INDICE.md` e `handoffs/costi.csv` nel repo si generano con `python Tools/CI/Build-Indice.py` dagli handover di
`docs/notes/handovers/`. Rifatti in questo giro (con questo handover dentro). Lo script legge solo gli handover in main: i 13 handover che stanno
solo su branch non uniti (sezione 6 dell'indice) entrano quando il branch viene unito.
Le copie nei project files (`handoffs/INDICE.md` del 23-11, `handoffs/costi.csv` del 24-11) sono ferme e non le ho toccate a mano: proposta,
un rimando in cima a ciascuna verso la copia generata nel repo (lo decide il coordinatore).

Script usati (nello scratchpad del thread, non committati): elenco orfani e voci senza file per nome di file; branch uniti = SHA presente nella storia di `origin/main`.
