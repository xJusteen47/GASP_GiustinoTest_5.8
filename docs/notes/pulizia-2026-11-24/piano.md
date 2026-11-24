# Piano di pulizia del repo e dell'harness (2026-11-24)

Thread cloud, Opus high, sola lettura sul repo. Niente è stato cancellato o spostato. Elenco completo dei branch con categoria e azione: `handoffs/pulizia-2026-11-24/branch-elenco.csv`.
Principio (come negli studi): **una fonte di verità per ogni tipo di informazione, e tutti gli strumenti, Claude o Antigravity, la leggono da lì.**

## 0. Cosa c'è oggi (misurato il 2026-11-24 01:45 UTC, `git fetch` + script in sola lettura)

- **183 branch remoti**, 5 PR aperte (n. 20, 21, 22, 25, 27), tutte del 1-3/11 e tutte verso `feature/gas-npp-5.8`.
- **Due tronchi**: `main` (PR #37 del 22/11) e `feature/gas-npp-5.8` (fermo al 16/11). `feature` NON è in `main` (1063 commit suoi, 6142 file diversi), ma CLAUDE.md dice ancora "Branch feature/gas-npp-5.8" e `Tools/CI/Cleanup-Branches.ps1` usa `feature` come riferimento: lo script approvato il 16/11 oggi darebbe risultati sbagliati.
- **`main` contiene 9 commit radice orfani** (snapshot del 17-21/11, es. `9fe083bd`): la storia è stata ricucita, quindi i branch nati prima del 22/11 non risultano "uniti" anche quando il contenuto è già dentro.
- `claude/anim-skill-t3` contiene `main` + `claude/integ-2026-11-23` (+155 commit): è già, di fatto, l'unico branch di integrazione.
- **Due script di potatura** (`Cleanup-Branches.ps1`, `Remove-MergedRemoteBranches.ps1`) con regole diverse; il secondo non è mai stato eseguito.
- **Registri doppi e divergenti**: `handoffs/INDICE.md` nel repo (10 righe) e nei project files (93 righe); `costi.csv` 88 contro 105 righe; `memoria-progetto.md` in entrambi; `docs/INDICE-PROGETTO.md` e `docs/STATO-ATTUALE.md` esistono solo su branch non uniti.
- **Stato clip in tre posti**: `docs/notes/anim-approvate.md` (9 righe), i tag `approved/*` (7) e i branch `claude/approved-runstart-*`. Non coincidono: mancano i tag di dair v9 e dropkick K7 (e il K7 per decisione owner non va pushato), w3 ha sia tag sia branch. Il pre-push guard rifiuta i tag, per questo restano "da creare".
- **Asset approvati non versionati**: jab v12 e ftilt v3.1 vivono solo sul fisso (`Content/AnimPipeline` non è in git). È il rischio più grosso del progetto.
- **Skill in 5 posti**: `.claude/skills/` (6 skill), `skills-public/`, `anim-skills/` nei project files, la junction di Antigravity in `~/.gemini/config/skills/` (punta ancora a `gstn-anim-cycle-cheap`, rinominata da T1) e la skill di account `gstn-move-animation` (ancora installata).
- **Regole in due versioni**: `AGENTS.md` (letto da Antigravity) rimanda a `regole-progetto-2026-11-22.md` "Regole 1-15"; le istruzioni di progetto attuali hanno 17 regole aggiornate il 24/11. Antigravity lavora quindi con regole vecchie: è la causa principale del disordine.

## 1. Una fonte di verità per tipo

| Informazione | Fonte unica | Chi scrive | Copie lette |
|---|---|---|---|
| Codice, tools, skill del progetto | `main` (via integrazione) | thread, via PR | nessuna |
| Regole | istruzioni di progetto = `docs/notes/regole-progetto.md` (senza data, una sola) | owner tramite coordinatore | CLAUDE.md e AGENTS.md rimandano lì, non copiano |
| Skill | `.claude/skills/` nel repo | thread, via PR | junction Antigravity e skill di account NON copiano: puntano lì o vengono tolte |
| Stato clip | `Tools/AnimPipeline/registro-clip.csv` (§4) | solo `animpipe` (comandi publish/approve) | tag `approved/*` creati dallo stesso comando; `anim-approvate.md` diventa vista generata |
| Handover | un file per thread in `docs/notes/handovers/` sul suo branch | il thread | nessuna |
| Indice thread e costi | **generati** dagli handover (righe `ESITO:`, `SUBAGENTI:`, `COSTO:`) con uno script | nessuno a mano | project files `handoffs/INDICE.md` e `costi.csv` = output dello script |
| Coda di lavoro | project files `handoffs/` del coordinatore | coordinatore | `handoffs/queue.json` nel repo si toglie |

Il motivo dell'indice generato: oggi ogni branch modifica `INDICE.md` e `costi.csv`, quindi ogni merge li fa divergere. Con un file per thread non c'è più un file condiviso da litigare.

## 2. Un solo tronco e un solo branch di integrazione

- Tronco: **`main`** (decisione 1). `feature/gas-npp-5.8` si archivia con tag `archivio/feature-gas-npp-5.8`; CLAUDE.md, AGENTS.md e `Cleanup-Branches.ps1` passano a `main`.
- Integrazione: **un branch per giorno di desktop**, `claude/integ-<data>`, nato da `main`, compilato sul fisso e chiuso con PR in `main` la sera stessa (regola 15h già lo chiede). Oggi è `claude/anim-skill-t3`: dopo la calibrazione T3 va in `main` con PR; non toccarlo prima.
- Ordine di merge dopo T3 (gli 11 branch con codice non ancora dentro, categoria E del CSV):
  1. `skill-anim-t0-inventory-2026-11-24` e `project-thread-vqce2q`, `nfhw4i`, `8esu46` (solo Tools, rischio basso)
  2. `docs-archivio-2026-11-23` (contiene bibliotecario e indice-progetto)
  3. `project-thread-qjfnc0` (getup r3, contiene rqxsn8), `3u6rt4` (rimbalzo Cr.L Aerial, mai compilato)
  4. `project-thread-7ogxes` (F throw, contiene y270ci), `oclodp` (skin, tocca .uproject)
  5. `m12-g7-g8` (contiene g0-g6 e p1-build-fix)
  6. `project-thread-5kb8vz` (contiene `content-reorg-apply`: 150 file Content, sposta asset) per ultimo, da solo, con editor chiuso e compilazione subito dopo.
- Dopo ogni gruppo: build con `Run-Timed.ps1 -Preset Build`; un gruppo che non compila si toglie e torna in coda (regola 5).

## 3. Potatura dei branch

Categorie in `branch-elenco.csv`: A 37 già dentro `anim-skill-t3`; B 22 sottoinsiemi di un altro branch; C 81 storia vecchia o pre-snapshot (tag `archivio/<nome>` poi elimina); D 26 solo documenti (si raccolgono in un commit "docs-sweep" sull'integrazione, poi elimina); E 11 con codice (§2); tenuti 5 + `main`.
- Restano: `main`, il branch di integrazione del giorno, `claude/artifacts`, il branch del thread in corso. I branch `approved-*` si eliminano quando il tag esiste.
- Le 5 PR aperte si chiudono con un commento "superata da main" (il loro contenuto è nei tag di archivio).
- Uno script solo: si tiene `Cleanup-Branches.ps1`, puntato su `main`, che legge il CSV e per C/D/E crea prima il tag `archivio/<nome>`; `Remove-MergedRemoteBranches.ps1` si elimina. Il guard blocca giustamente le cancellazioni ai thread: lo lancia l'owner con `-Apply` (un comando, ~5 min). Ogni branch resta recuperabile dal tag.
- Regola nuova per non tornare qui: **un thread finito con PR unita cancella il suo branch nella stessa PR** (opzione GitHub "auto delete head branches", da accendere nelle impostazioni del repo); i thread senza PR entrano nella potatura del venerdì insieme al bibliotecario.

## 4. Registro clip unico e publish delle animazioni

- `Tools/AnimPipeline/registro-clip.csv`, una riga per clip/versione: `mossa, versione, stato (WIP | review | PASS | approvata | bloccata | scartata), responsabile (thread o owner), asset UE, carta, commit, video review, data, parole owner`.
- Solo due comandi lo scrivono (si estende `animpipe approve` e `locks.py` che già esistono):
  - `animpipe publish <mossa> <ver>`: rifiuta se manca `ASSERT: OK` o la review livello 2 PASS; copia l'asset in `Content/AnimPipeline/Published/<mossa>/<ver>/` (cartella versionata in LFS), mette la riga a `PASS`.
  - `animpipe approve <mossa> <ver> --owner "<parole>"`: riga a `approvata`, crea il tag `approved/<mossa>-<ver>`, rigenera `anim-approvate.md`. Il guard si modifica per accettare **solo** `refs/tags/approved/*`.
- Asset approvati in LFS **con lock** (`git lfs lock`): nessuno, agente o persona, salva sopra una versione bloccata. È l'unica eccezione al "senza lock" di CLAUDE.md.
- Primo lavoro: pubblicare subito jab v12 e ftilt v3.1 dal fisso (oggi solo copia locale) e creare i tag mancanti (dair v9; K7 resta senza tag per decisione owner, lo stato lo dice il registro).

## 5. Antigravity: cosa legge per primo

`AGENTS.md` si riscrive come CLAUDE.md breve, che rimanda senza copiare: 1) `docs/notes/regole-progetto.md`; 2) `CLAUDE.md` (mappa del codice); 3) `docs/INDICE-PROGETTO.md` (cosa riusare); 4) `registro-clip.csv` (stato clip, mai scriverlo a mano); 5) il brief del suo task. Skill: junction `~/.gemini/config/skills/<nome>` -> `<repo>\.claude\skills\<nome>` per ognuna, generate da uno script `Tools/CI/Sync-AgentSkills.ps1` che prima toglie le junction rotte (oggi `gstn-anim-cycle-cheap`). Un hook di Antigravity o una riga in AGENTS.md: "se il tuo branch non parte dall'integrazione del giorno, fermati".

## 6. Esecuzione (dopo il PASS di T3, niente prima)

| Fase | Dove, modello | Cosa | Stima |
|---|---|---|---|
| P0 | fisso, Haiku low subagente | prima di tutto: `claude/anim-skill-t3` nella linea di integrazione (PR in `main`), poi aggiornare `_ue59` (segnare `IN-USO.txt`) e compilare con `Run-Timed.ps1 -Preset Build` il plugin `GSTNCapture` (già in t3 da bazqb1, abilitato nel .uproject). Prova: una cattura in video in tempo reale invece dell'export PNG per frame (oggi 96 dei 99 s per sequenza sono export PNG con GPU ferma), con `ASSERT: OK` | 1 $ + 30 min desktop |
| P1 | owner | PR `anim-skill-t3` -> `main`, accende auto-delete branch | 5 min |
| P2 | cloud, Sonnet medium | `regole-progetto.md` unico, CLAUDE.md/AGENTS.md, script indice da handover, docs-sweep (cat. D), `Cleanup-Branches.ps1` su `main` + CSV, guard per i tag `approved/*` | 4 $ |
| P3 | fisso, Haiku low subagente | merge E nell'ordine §2 con build dopo ogni gruppo | 3 $ + 60 min desktop |
| P4 | cloud, Opus medium | `registro-clip.csv`, `animpipe publish/approve`, test | 5 $ |
| P5 | fisso, Haiku | publish jab v12, ftilt v3.1, dair v9, tag; `Sync-AgentSkills.ps1` | 1 $ + 20 min |
| P6 | owner | `Cleanup-Branches.ps1 -Apply`, chiude 5 PR, toglie skill di account | 10 min |

Totale stimato circa 14 $ (21 $ con i fix del §8), 110 minuti di fisso, 15 minuti dell'owner.

## 7. Decisioni per l'owner

**DECISO dall'owner il 2026-11-24 01:48 UTC: "1A,2A,3A"** (ambito: repo, harness e asset approvati). Esecuzione P1-P6 dopo la review cieca dair v9 sul fisso e il PASS di T3; il via lo dà il coordinatore.

1. Tronco. **A (consigliata)** `main` unico, `feature/gas-npp-5.8` archiviato con tag. B tenere `feature` come tronco (bisogna riportarci 241 commit di `main`).
2. Indice e costi. **A (consigliata)** generati dagli handover, nessuno li scrive a mano. B restano scritti a mano ma solo nei project files (il repo non li ha più).
3. Asset approvati. **A (consigliata)** cartella `Published/` in LFS con lock, publish obbligatorio. B solo registro e tag, asset sul fisso come oggi (resta il rischio di perdere jab v12 e ftilt v3.1).

In attesa: via del coordinatore dopo la review cieca dair v9 sul fisso.

## 8. Altre perdite di tempo (richiesta owner 2026-11-24 01:52 UTC)

Fonti: handover dei branch recenti e dei project files (459 file), `handoffs/tempi-import-cattura-2026-11-23.md` (giro 2 di K8: 25 min = 6 import + 9 cattura + ~10 attese), `skill-anim-unica-t3/t4.md`, `Tools/CI/Run-Timed.ps1` su t3. Dove c'è scritto "stima" il numero non è misurato.

| # | Perdita | Tempo perso | Fix | Costo |
|---|---|---|---|---|
| 1 | Cattura PNG per fotogramma, 3 viste lette dalla GPU (~7 fps) | ~3,5 di 9 min a giro | P0: GSTNCapture in tempo reale | in P0 |
| 2 | Ogni giro ricattura cose che non cambiano: 2 run su 5 sono K7 approvata, 1 serve solo all'owner | ~3-4 min a giro (stima) | unire `nfhw4i` (capdet 2: cache per hash della clip, già scritta e testata) e ricatturare la base solo se cambia l'hash | gruppo 1 di P3 |
| 3 | Avvii dell'editor separati per import, montage e cattura, e 5 Play/stop | ~5 min a giro (stima; 19-24 s a caldo, minuti se ricompila shader dopo un cambio branch) | un'unica sessione editor per giro (import + montage + cattura) tramite MCP; `_ue59` non cambia branch durante una serie di giri; DDC condivisa fra worktree | 4 $ cloud + 30 min fisso |
| 4 | L'editor non si chiude dopo `QUIT_EDITOR` -> TIMEOUT di 15 min che vale un tentativo (skid, PiePerf, preset di cattura) | 10-15 min per caso, più un tentativo bruciato | Run-Timed legge `ESITO:` dal driver e chiude l'editor subito; un TIMEOUT dopo `ESITO: OK` non conta come tentativo | 1,5 $ |
| 5 | Driver Python con errore di sintassi scoperto a editor avviato (una notte: un'ora a vuoto) | fino a 60 min | PreCheck di default in Run-Timed: `py -m py_compile` di ogni .py passato (il gancio `-PreCheck` esiste già ma è facoltativo) | 0,5 $ |
| 6 | Passo headless lanciato dal progetto sbagliato: dump dair di T3 statico perché il worktree lifecal non ha lo scheletro, un giro di taratura perso | un giro intero | PreCheck: l'asset chiesto esiste nel progetto del comando; i passi UE di animazione girano solo su `_ue59` (scritto in skill e AGENTS.md) | 0,5 $ |
| 7 | Attrito di permessi: tag `approved/*` rifiutati dal guard, cancellazioni rifiutate, push FF negato dal classificatore, catture ferme ore ad aspettare "Allow" sul portatile | ore di calendario | guard accetta `refs/tags/approved/*`; regole allow in `.claude/settings.json` per Run-Timed e salvataggi headless in `_ue59`; il messaggio unico di inizio sessione (regola 9) elenca anche gli Allow del portatile | 1 $ |
| 8 | Modello locale sul 4080 che occupa VRAM mentre si cattura -> catture lente o TIMEOUT | un tentativo per caso | PreCheck di cattura rifiuta se un processo LLM locale usa la GPU (nvidia-smi) | 0,5 $ |
| 9 | Il cloud non legge i .uasset e il fetch di 183 branch è lento: fatti "INFERITI" (inventario clip) e linee non clonate | lavoro rifatto | potatura (P6), `GIT_LFS_SKIP_SMUDGE=1` nei thread cloud, dump JSON dei metadati asset committati da `animpipe publish` | in P4/P6 |
| 10 | `costi.csv` con colonne mescolate (token nella colonna minuti, 105 righe con schemi diversi): il controllo del venerdì non è affidabile | il controllo costi | indice e costi generati (2A) con controllo dello schema | in P2 |
| 11 | Documenti di avvio pesanti o vecchi: CLAUDE.md manda a `takeover-state.md` (963 righe) e a `feature/gas-npp-5.8`; `INDICE-PROGETTO.md` non è in `main` | contesto sprecato in ogni thread | CLAUDE.md punta a `INDICE-PROGETTO.md` in `main`; `takeover-state.md` va in archivio | in P2 |

I fix 4, 5, 6 e 8 sono un solo lavoro: PreCheck di default in `Run-Timed.ps1`, che si fa in P2 (aggiunge ~3 $ alla fase). Il fix 3 è il più grande e va dopo P0, quando si vede quanto resta della cattura con il video in tempo reale. Nuovo totale stimato: circa 21 $.

**APPROVATO dall'owner il 2026-11-24 01:56 UTC ("Va benissimo")**: piano completo con i tre blocchi di fix (1 PreCheck di default + chiusura automatica dell'editor su `ESITO:`; 2 sessione editor unica via MCP + cache capdet; 3 GSTNCapture compilato in `_ue59`). Tag e "Allow" restano dell'owner, raccolti in un solo messaggio dal coordinatore. Via all'esecuzione: coordinatore, dopo la review cieca dair v9.
