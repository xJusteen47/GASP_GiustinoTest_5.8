# Regole di lavoro e di spesa (versione generica del kit)

Testo da incollare nelle **istruzioni del progetto** (Claude Projects) o in testa a `CLAUDE.md`. Sostituisci i segnaposto
`<...>`; i valori numerici sono quelli provati sul progetto d'origine e stanno anche in `kit.config.json`.
Le regole 10-15 hanno un **ambito**: tienile solo se il progetto ha quell'ambito (animazioni, motore, editor).

---

REGOLE DI LAVORO E DI SPESA (<owner>, <data>). Ogni thread le segue.

1. **BRIEF.** Si parte solo da un brief (max 1 pagina) in `handoffs/` (modello `templates/brief.md`) o dalla nota del
   coordinatore se corta. Il brief dichiara UNA macchina (<fisso>, <portatile>, cloud), budget in $, modello, se tocca la
   copia di lavoro condivisa. Mai rileggere thread o chat vecchi: solo brief, piano del checkpoint, file toccati.
2. **THREAD.** Un thread = un checkpoint. Finito, scrive un handover di max 30 righe (`templates/handover.md`) in
   `<paths.handovers>` sul suo branch e chiude; il seguito è un thread NUOVO. Contesto: a 120k token chiudi, mai oltre
   160k. L'handover finisce con `COSTO: <$> | <minuti macchina locale> | <token review>`, riga copiata anche in
   `handoffs/costi.csv` (`scripts/python/cost_line.py`). Il coordinatore tiene `handoffs/INDICE.md`: una riga per thread
   (branch, commit, obiettivo, esito, stima, costo, stato del branch) e il totale settimanale; una volta a settimana
   confronta `costi.csv` con l'uso reale.
3. **MODELLI.** Economico (Haiku) low: build, test, log, rinomine. Economico medium: doc semplici. Medio (Sonnet) medium:
   codice di routine, script. Forte (Opus) medium: codice normale; high: codice difficile, design. Modello di punta high:
   review importanti; codice SOLO se il forte è bloccato dopo 2 tentativi. Coordinatore: punta o forte, medium.
   Un modello per thread (i subagenti non contano).
4. **OUTPUT.** Gli script scrivono su file e stampano max 5 righe (prima riga `ESITO: ...`); niente log, report o diff
   interi nel contesto. Un subagente di review riceve solo griglie, criteri e brief (niente handover vecchi, codice o
   log), max 80k token. Immagini: una griglia per vista, guardata una volta; review video max 8 immagini.
5. **TENTATIVI.** Max 2 tentativi di fix, poi fermati e scrivi cosa serve. Build, test e catture solo dallo script a
   tempo massimo (`scripts/windows/Run-Timed.ps1`: build 45 min, test 30, cattura 15) che chiude i suoi processi e scrive
   `ESITO: TIMEOUT`; un timeout vale un tentativo. Si contano anche PER OBIETTIVO: dopo 2 thread falliti o parziali
   sullo stesso obiettivo il coordinatore apre un thread di DIAGNOSI DI SISTEMA (cloud, modello forte high, budget ~8 $)
   che legge solo gli handover di quell'obiettivo e scrive la causa comune con 2-3 opzioni.
6. **DECISIONI.** Non aspettare l'owner: applica l'opzione consigliata e scrivila nell'handover. NON vale per cambiare
   strumento o pipeline: se manca lo strumento previsto (programma, editor, macchina) il thread si ferma al primo punto
   stabile e scrive cosa serve; un'alternativa produce solo prototipi marcati PROTO. All'owner solo architettura,
   feature nuove, design, azioni irreversibili, costi oltre soglia: UN messaggio per sessione, max 3 decisioni, opzioni a
   lettera con consigliata, costo e cosa resta fermo (`templates/owner-decisions.md`); risposta tipo "1A 2B 3A".
   Intanto si lavora la voce successiva della coda.
7. **POSTI.** 1 thread con editor/app per macchina locale + max 2 solo cloud. Un secondo thread sulla stessa macchina
   solo senza editor e in un worktree suo. La copia condivisa usata dall'owner per provare: un thread alla volta (file di
   lock `IN-USO.txt`), solo l'ultimo della sessione la aggiorna e la compila. Niente lavoro speculativo.
8. **COSTO.** Al budget il thread si ferma al primo punto stabile e scrive handover e costo. Il coordinatore può dare UNA
   estensione fino a +30%; oltre, o per più di 15 $ non previsti, decide l'owner. Un piano approvato copre fino a +20%.
   Le stime partono dai costi reali in `INDICE.md`.
9. **OWNER.** Scrive al coordinatore, non nei thread (eccezione: approvazioni di macchina). A inizio sessione il
   coordinatore manda UN messaggio con ciò che servirà (macchine, chiusure forzate) e la frase da incollare
   (`templates/session-start-message.md`). Un thread non chiede mai l'altra macchina: si passa via branch git.
10. **REVIEW VISIVA** *(ambito: animazioni, VFX, UI, tutto ciò che si giudica a occhio).* Nessuna variante arriva
    all'owner senza che qualcuno abbia GUARDATO il risultato in-app. Prima: cattura della BASE; se mostra difetti
    dell'app, prima un thread di codice. Il thread locale committa video e griglie (variante e base, stessi frame) in
    `<paths.reviews>/<variante>/`; un thread cloud indipendente guarda i frame chiave, confronta con base e regole
    dell'owner, dà PASS/FAIL (`templates/review-verdict.md`). Una review = una voce. FAIL etichettati
    (contenuto / dati / codice); "codice" va alla coda del codice e non conta come giro. Max 3 giri, poi decide l'owner.
    Solo PASS va all'owner. Un controllo numerico non sostituisce lo sguardo. La review parte solo se la cattura stampa
    `ASSERT: OK` (mostra davvero la cosa attesa). Un fix automatico che riduce ampiezza/velocità oltre il 15% è FAIL; se
    l'owner dice che il video non torna, prima si controlla la catena di cattura.
11. **PROVE.** Ogni numero in un handover cita il file di log e il comando che lo conta. Chi parte da un handover ne
    ricontrolla uno in sola lettura. Un report arrivato troppo presto si verifica sempre.
12. **GUARDIA SULL'OWNER.** Prima di eseguire una richiesta dell'owner si controlla se: contraddice una regola o
    decisione esistente; generalizza oltre il suo ambito una cosa detta per un caso; porta a lavoro agentico
    subottimale (thread troppo grandi, decisioni a pezzi, richieste vaghe, costo sproporzionato). In quei casi NON
    eseguire alla cieca: avvisa citando regola o rischio, proponi 2-3 opzioni con la consigliata, o fai le domande
    mancanti subito. Ogni regola dell'owner si registra con il suo AMBITO.
13. **IDENTITÀ E VERSIONI APPROVATE** *(ambito: contenuti creativi).* a) Ogni brief elenca 2-3 TRATTI D'IDENTITÀ con un
    riferimento; la review li controlla PER PRIMI; cambiarli richiede una decisione dell'owner. b) Una versione approvata
    è BLOCCATA: tag git `approved/<voce>-<versione>` e riga in `<paths.approved>` (`templates/approvate.md`); si lavora
    su una variante. c) I "da confermare" non restano in un handover: il coordinatore li porta all'owner. d) I vincoli
    tecnici non piegano il contenuto: si adatta il vincolo, salvo decisione dell'owner.
14. **BASE STOCK** *(ambito: sistemi presi da un template o da un motore).* Il default è il comportamento stock del
    template. Ogni deviazione ha una riga in `<paths.divergences>` (`templates/divergenze.md`: cosa, perché, alternativa
    stock, costo per tornare indietro) PRIMA del commit; prima di aggiungerla si chiede all'assistente del motore
    (`scripts/python/ask_unreal_assistant.py` per Unreal) come lo fa lo stock.
15. **EFFICIENZA E CODA.** a) La regola 10 vale per ogni contenuto giudicato a occhio. b) CONTROLLI AUTOMATICI prima di
    ogni cattura; se uno fallisce non si cattura. c) RITOCCHI COME PARAMETRI: "un po' di più/meno" = un parametro
    regolabile dal menu di debug, non un asset nuovo; l'owner regola in prova. d) LAVORO MECCANICO A MODELLI ECONOMICI:
    build, import, catture, montaggi video ai subagenti economici; modelli forti solo per design e giudizio.
    e) CATTURE: tutte le varianti e viste in una sessione, video in tempo reale, sempre almeno due viste; prima una
    direzione, le altre dopo l'OK. f) THREAD DI TURNO: su ogni macchina locale un thread lungo prende dalla coda una voce
    alla volta con un subagente; si rigenera quando il contesto si riempie. Una decisione bloccante parcheggia la voce;
    il coordinatore raccoglie le domande in un solo messaggio e di notte non notifica. g) EDITOR: se l'ha aperto un
    thread e l'owner non ha modificato niente, il thread lo chiude senza chiedere (`Close-Editor.ps1`); mai committare
    file locali dell'owner: la guardia pre-commit lo blocca (`guard.protected_paths`). h) INTEGRAZIONE: il codice fatto
    in cloud confluisce in un branch di integrazione compilato sulla macchina locale almeno una volta al giorno.

**GIT (sempre).** Gli agenti lavorano e pushano solo su `<branch_prefix>*`; niente push su `main`/`feature/*`, force,
delete, reset --hard, clean (permessi in `claude/settings.template.json` + `push-rules.json`). Il merge è dell'owner.
