# Stato attuale del progetto (fonte unica di verità)

Per una persona o un agente che NON vede la chat del progetto né la memoria. Prima versione: 2026-11-23, bibliotecario
(branch `claude/docs-bibliotecario-2026-11-23`). Il coordinatore la aggiorna a ogni decisione dell'owner; il giro del
venerdì la controlla contro il repo. Ogni riga ha **data** e **fonte**. "Memoria" = memoria di progetto del coordinatore
(non leggibile da fuori: per questo la riga è copiata qui). Dove la fonte non basta c'è **da confermare**.
Se questo file contraddice un altro documento, vale questo file (o il messaggio dell'owner più recente), e la
contraddizione va segnalata nel rapporto del bibliotecario.

Pagine sorelle: `docs/INDICE-PROGETTO.md` (cosa esiste nel codice), `docs/BRIEFING-ANTIGRAVITY.md` (per agenti esterni),
`docs/notes/archivio-proposta.md` (ultimo rapporto del bibliotecario).

## 1. Regole di lavoro in vigore (sintesi; testo pieno = istruzioni di progetto, aggiornate 2026-11-23)
Fonte: istruzioni di progetto del coordinatore (regole 1-16, 15-11 → 20-11 → 23-11). **Superano**
`docs/notes/regole-progetto-2026-11-20.md` e `-2026-11-22.md` (che dicono Antigravity coordinatore e DeepSeek esecutore) e
l'artefatto "Analisi regole di lavoro" (15-11).
| # | In breve | Data |
|---|---|---|
| 1 | Si parte da un brief (max 1 pagina, `handoffs/`): UNA macchina, budget $, modello. Prima del codice si consulta `docs/INDICE-PROGETTO.md` e si dichiara cosa si riusa; a fine lavoro si aggiorna la voce toccata. | 23-11 |
| 2 | Un thread = un checkpoint. Handover max 30 righe in `docs/notes/handovers/` sul suo branch, ultime righe `SUBAGENTI:` e `COSTO: <$> \| <min desktop> \| <token review>`, copiate in `handoffs/costi.csv`. A 120k token chiudi. | 23-11 |
| 3 | Modelli: Haiku build/test/log; Sonnet codice di routine; Opus medium codice; Opus high codice difficile, design, pose. Fable solo dopo 2 errori di Opus sullo stesso obiettivo. Coordinatore Sonnet/Opus. Antigravity/Neon solo per review quando tornano i crediti. | 23-11 |
| 4 | Script: output su file, max 5 righe a schermo. Review max 8 immagini. | 20-11 |
| 5 | Max 2 tentativi di fix; build/test/catture solo da `Tools/CI/Run-Timed.ps1` (45/30/15 min). Dopo 2 thread falliti su un obiettivo: thread di diagnosi, non un terzo tentativo. | 20-11 |
| 6 | Le decisioni reversibili si applicano (la consigliata) e si scrivono nell'handover. Mai cambiare strumento o pipeline da soli. All'owner: max 3 decisioni per messaggio, a lettera ("1A 2B"). | 20-11 |
| 7 | 1 thread con editor per macchina + max 2 solo cloud. `_ue59` un thread alla volta (`_ue59/IN-USO.txt`). | 23-11 |
| 8 | Al budget ci si ferma al primo punto stabile; una estensione +30% dal coordinatore; oltre decide l'owner. | 20-11 |
| 9 | L'owner scrive al coordinatore, non ai thread (eccezione: approvazioni di macchina). Un thread non chiede mai l'altra macchina: si passa via git. | 20-11 |
| 10 | Nessuna variante d'animazione va all'owner senza video in-game GUARDATO e review cloud indipendente (PASS/FAIL, max 3 giri di posa). Prima la vita, poi i numeri. | 20-11 |
| 11 | Ogni numero in un handover cita log e comando. | 20-11 |
| 12 | Prima di eseguire una richiesta dell'owner, controllare che non contraddica regole o decisioni né generalizzi oltre il suo ambito. | 15-11 |
| 13 | Ogni mossa ha 2-3 tratti d'identità; una versione approvata è BLOCCATA (tag `approved/<mossa>-<versione>` + riga in `docs/notes/anim-approvate.md`): si lavora su una variante. | 15-11 |
| 14 | GASP stock è il default; ogni deviazione ha una riga in `docs/notes/gasp-divergenze.md` prima del commit. | 20-11 |
| 15 | Ritocchi "un po' di più/meno" = cvar o additivo nel menu GSTN Debug, non clip nuove; controlli automatici prima di ogni cattura; catture da dietro E di lato; mai committare file locali dell'owner. | 21-11 |
| 16 | Il codice usa AZIONI di input (Enhanced Input/comandi), mai tasti fisici; i tasti nei brief sono solo default rimappabili. | 23-11 |
Git (memoria, 22-11): push solo su `claude/*`; vietati push su `feature/*` e `main`, force, cancellazione di branch, `reset --hard`, `clean`. Il branch di base è `main` (non `feature/gas-npp-5.8`, che `CLAUDE.md:69` cita ancora: da correggere).

## 2. Gioco, kit e notazione
| Cosa | Stato | Data, fonte |
|---|---|---|
| 4 kit: **Overlord**, **BirdKnight**, **FlameFury**, **Voyager**. Solo Overlord ha lavoro vero. Skin cosmetiche usabili con ogni kit; almeno 2 armi di supporto e 1 super a tema per personaggio. | vivo | 23-11, memoria "ROSTER"; `anim-inventario/anim-inventario.xlsx` (branch `claude/anim-inventario-2026-11-23`) |
| Corporature: Overlord = tipo Ganondorf (molto largo); BirdKnight = robusto intermedio, è anche lo scheletro base invisibile; Voyager e FlameFury = "ragazzi" proporzioni Link OOT. | vivo | 23-11, memoria "SKIN PROPORTIONS"; `docs/notes/skin-proporzioni-analisi.md` (branch `claude/project-thread-4pj7kf`) |
| Personaggio base = agile stile Link; stile visivo "Ganondorf dentro Astro Bot". | vivo | memoria (data da confermare) |
| **Notazione mosse** (vale per TUTTE le nostre mosse; i nomi Smash sono solo riferimenti): F.L avanti leggero, N.L neutro leggero (= jab), Cr.L / Cr.F.L accovacciato; "strong" = S (F.S, N.S), "special" = Sp (F.Sp, Cr.F.Sp); in aria si aggiunge "Aerial"; senza direzione = neutro; in aria "giù" = Cr. (dair = Cr.N.L Aerial). Non definiti: down tilt/smash a terra, Cr.S/Cr.Sp, jab 2/3. | vivo | 23-11, `docs/notes/notazione-mosse.md` (branch `claude/anim-inventario-2026-11-23`) |
| Overlord: F.S = fendente verticale (rif. forward smash di Ganondorf); N.S = fendente orizzontale con curva regolabile (rif. ftilt di Ike); F.L Aerial = dropkick (braccia CONSERTE, gambe unite, aria spavalda in caduta); Cr.N.L Aerial = stomp. Frame data di riferimento: Smash Ultimate. | vivo; da confermare se N.S sostituisce ftilt v3.1 | 23-11, memoria "OVERLORD MOVES" |
| Presa (Overlord, bersagli umanoidi): tiene il nemico AL COLLO con una mano, può camminare e saltare (salto più basso), posa a terra con il tasto schivata (RB, **da confermare**); lanci N., F., Cr., Cr.F. (F. = come il lancio F di Zetterburn in Rivals 2); tempo massimo di presa regolabile. Pose in attesa dei riferimenti dell'owner. | in corso, codice non compilato | 23-11, memoria "GRAB"; branch `claude/grab-link`, `docs/notes/grab-link-verifica-editor.md` |
| Tasti di default (Xbox, rimappabili, regola 16): X leggero, A salto, Y special, B presa/cambio kit, RT strong, LT accovacciata, RB schivata, LB scudo, L3 dash. | **da confermare L3**: oggi nel codice L3 = run boost e il dash non ha tasto pad | memoria "DESIGN"; `docs/notes/input-azioni-mappa.md:53-54` (branch `claude/project-thread-uvefj5`) |
| Rete 1v1: all'inizio niente server dedicato; host (listen server) o P2P via Epic Online Services. | ricerca chiusa, decisioni aperte | 23-11, `docs/notes/server-dedicato-ricerca.md` (branch `claude/ricerca-server-dedicato`) |
| Camera libera di default; rendering deferred senza Lumen + TSR (`gstn.Render.Profile 0-4`); VFX spenti di default e in pausa. | vivo | memoria (21-22/11) |
| Tutti i kit hanno la stessa sensazione di movimento via `DA_Locomotion_Default` (file LOCALE dell'owner, mai committato). Locomotion = GASP Mover stock (`gstn.Loco.Base 1`). | vivo | memoria; `docs/INDICE-PROGETTO.md` §5.3 |

## 3. Versioni approvate e bloccate (non sovrascrivere: si lavora su una variante)
| Mossa | Versione | Tag | Data, fonte |
|---|---|---|---|
| Run start (Overlord) | v4r5 (sicurezza) | `approved/runstart-v4r5` (sul remoto; da ri-taggare "opzione A", serve force e l'ok dell'owner) | 20-11, `docs/notes/anim-approvate.md` |
| Run start (Overlord) | **v4r7** | `approved/runstart-v4r7` (sul remoto) | 20-11, `docs/notes/anim-approvate.md` |
| Run start generico | w1 (Variant 20) | `approved/runstart-w1` (sul remoto) | 21-11, `docs/notes/anim-approvate.md` |
| Run start generico | w2 (Variant 21, default) | `approved/runstart-w2` (sul remoto) | 21-11, `docs/notes/anim-approvate.md` |
| Run start generico | **29w3** | `approved/runstart-v29w3` **NON sul remoto** (push fallito; va fatto da un PC) | 23-11, memoria "STATE"; branch `claude/runstart-lock-2026-11-23` |
| Turnaround / cambi direzione 21-26 | verdetti dell'owner su 23, 24, 25 (+TorsoLean) e 26 **ancora aperti**; v27 180 L: owner chiede braccia che guidano la svolta, una direzione alla volta | — | memoria "RUN START", "TURNAROUNDS" |
Elenco completo e regole: `docs/notes/anim-approvate.md`. Verifica: `git ls-remote --tags origin 'approved/*'`.

## 4. Sospeso (non iniziare senza parola dell'owner)
| Cosa | Data, fonte |
|---|---|
| Prova Unreal Canvas (mesh da 3 viste), nemici/minion completi con guida designer, filtro ZZZ (squash/stretch additivo) | 23-11 12:26, memoria "OWNER HOLD" |
| Migrazione proporzioni skin (segue l'architettura skin) | 23-11, memoria |
| Prototipo cosmetica modulare (16-26 $): ricerca chiusa, decisioni 1A 2A confermate, aspetta il via | 23-11, `docs/notes/skin-cosmetici-modulari.md` (branch `claude/skin-cosmetici-ricerca`) |
| Pose dei lanci della presa: aspettano i riferimenti dell'owner | 23-11, memoria "GRAB" |
| VFX in generale | memoria "STYLE" |

## 5. In corso
| Cosa | Dove | Data |
|---|---|---|
| Integrare i branch cloud e compilare main sul portatile (main non compilato dal merge del 22-11) | thread "Integrate cloud branches..." | 23-11 |
| MCP animazione v2 e cattura della variante w3m7 (fisso) | branch `claude/anim-mcp-v2-turn4`, `claude/anim-mcp-followthrough` | 23-11 |
| Riordino di `Content/`: fase 1 (mappa) fatta, fase 2 sul fisso con l'owner presente | `docs/notes/content-riorganizzazione.md` (branch `claude/content-riorganizzazione`) | 23-11 |
| Mappa input e piano P0-P5 (un solo canale di input in rete) | `docs/notes/input-azioni-mappa.md` (branch `claude/project-thread-uvefj5`) | 23-11 |
| Prove visive di fisica (lanci di nemici e oggetti visti in video) | memoria "PHYSICS VISUAL TESTS": approvato 1A 2A, non iniziato | 23-11 |
| Dropkick K6 giro 3 (portatile), getups (attendono "ok salva") | memoria "QUEUE" | 21-23/11 |

## 6. Cambiato negli ultimi 7 giorni (17-23/11)
- 23-11: regole 1, 3, 7 aggiornate e regola 16 nuova (istruzioni di progetto). Fable ridimensionato.
- 23-11: nuovi documenti: indice del progetto, audit del codice, mappa input, riordino Content, cosmetica skin, proporzioni skin, ricerca rete 1v1, presa al collo, inventario animazioni e notazione mosse.
- 23-11: run start 29w3 approvata (tag da pushare). Antigravity senza crediti: coordina Claude.
- 22-11: PR #37 porta in main S1 corsa, haptics, M12 p1, M10/M11 (mai compilati insieme). `anim-process.md` superato.
- 21-11: run start w1/w2 approvate; regola 15 (efficienza animazioni, thread di turno); M10/M11 finiti.
- 20-11: regole riscritte dopo l'audit; v4r5/v4r7 approvate; regola 14 (GASP stock).

## 7. Da confermare (lista per l'owner)
1. RB = posa a terra del nemico preso? (memoria GRAB: "credo")
2. L3 = dash (tuo schema) o run boost (codice di oggi)?
3. N.S di Overlord sostituisce ftilt v3.1 o è una variante?
4. Ri-tag v4r5 con force (opzione A) e push del tag 29w3: chi lo fa e quando.
5. Verdetti sui turnaround 23, 24, 25 (+TorsoLean), 26.
6. `regole-progetto-2026-11-20/22.md` vanno segnate come superate? (vedi decisione 3 del rapporto)
7. Data della scelta "personaggio base agile stile Link" (fonte solo in memoria).
