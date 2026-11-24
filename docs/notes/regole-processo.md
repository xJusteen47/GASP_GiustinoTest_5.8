# Regole di processo (le legge il coordinatore; i thread solo la parte che li riguarda)

Decisione dell'owner 1A 3A del 2026-11-24. Raccoglie le parti di processo delle vecchie regole 1-9, 11, 14, 15 e 16 e aggiunge la forma dei thread e le guardie anti-regressione. Le istruzioni di progetto (una pagina) vincono su questo file.

## 1. Forma dei thread (3A)
- **Un thread per obiettivo**, non uno per checkpoint. Il thread tiene il proprio lavoro fino al risultato provato. Per il codice UE significa: scrive il codice, lo compila sul fisso con `Tools/CI/Run-Timed.ps1 -Preset Build -WaitMutex -NoHotReload` via Remote Control, esegue test o cattura e solo allora chiude. Il cloud senza PC va bene per letture, design, script Python e test offline.
- **Soglia ~1 $:** sotto, niente thread nuovo. Il coordinatore lo fa da sé o lo accoda al thread del suo obiettivo. Esempio da evitare: `anim-inventario` in 11 giri da 0,1 $.
- **Contesto:** nessuna chiusura fissa a 120k. Il thread lavora finché serve; a ~70% della finestra scrive nel thread un punto di ripresa (cosa ha escluso, cosa resta) e continua (l'harness compatta). Si apre un thread nuovo solo se l'obiettivo cambia.
- **Posti:** 1 thread con editor per macchina (fisso, portatile) e max 2 solo cloud. Un secondo editor sulla stessa macchina solo in un worktree suo, mai durante una cattura video. `_ue59` un thread alla volta (`_ue59/IN-USO.txt`); solo l'ultimo della sessione la aggiorna e la compila per il playtest. Un thread non chiede l'altra macchina: si passa via git.
- **Editor:** se l'ha aperto un thread e l'owner non ha modificato niente, il thread lo chiude senza chiedere.
- **Integrazione:** il codice fatto in cloud confluisce almeno una volta al giorno in un branch di integrazione compilato sul fisso.
- **Settimana senza lavoro sul metodo (SCADE il 2026-12-01):** finita la pulizia C1-C5, niente thread su regole, skill, harness o registri, salvo un guasto che blocca il lavoro. Si lavora sul gioco e sulle mosse.

## 2. Brief, handover, prove
- Si parte da un brief (max 1 pagina in `handoffs/`, o la nota del coordinatore se corta) con macchina, budget in $ e modello. Il thread può leggere i file e i log che servono; non rilegge chat vecchie intere.
- Handover breve nel branch (`docs/notes/handovers/`). Le ultime righe sono `ESITO: <PASS|PARZIALE|FAIL> <una frase>`, `SUBAGENTI: <quanti, lavoro, modello>` e `COSTO: <$ stimati> | <minuti desktop> | <token review>`. `INDICE.md` e `costi.csv` sono GENERATI da queste righe (decisione 2A del piano di pulizia): nessuno li modifica a mano.
- Ogni numero in un handover cita il file di log e il comando che lo conta. Chi parte da un handover ne ricontrolla uno in sola lettura.
- Gli script scrivono l'output intero su file e a schermo stampano un riassunto con il percorso del log. Il thread legge il log quando deve capire qualcosa: il riassunto non sostituisce il log.

## 3. Costi
- Ogni brief ha un budget in $. Al budget il thread si ferma al primo punto stabile e scrive handover e costo. Il coordinatore può dare UNA estensione fino a +30%; oltre, o per più di 15 $ non previsti, decide l'owner. Un piano approvato copre fino a +20%.
- Le stime partono dai costi reali nell'indice generato.
- Si risparmia con meno thread e meno rifacimenti, non con meno ragionamento (istruzioni, regola 3).

## 4. Owner e decisioni
- L'owner scrive al coordinatore. Eccezioni: approvazioni di macchina e conversazioni dirette che chiede lui.
- A inizio sessione il coordinatore manda UN messaggio con ciò che servirà (macchine, Antigravity, chiusura di editor e test) e la frase da incollare.
- Le domande all'owner si raccolgono: max 3 decisioni per messaggio, a lettera, con consigliata, costo e cosa resta fermo. Di notte niente notifiche.
- Se manca lo strumento previsto (Cascadeur, editor, salvataggio asset, macchina), il thread si ferma al primo punto stabile e scrive cosa serve. Un generatore alternativo produce solo prototipi marcati PROTO, mai in review né all'owner.
- Nessun "da confermare" resta solo in un handover: il coordinatore lo porta all'owner.

## 5. Strumenti
- Build, test e catture solo da `Tools/CI/Run-Timed.ps1` (build 45 min, test 30, cattura 15), che chiude i suoi processi e scrive `ESITO: TIMEOUT`. Un timeout vale un tentativo, salvo dopo `ESITO: OK`.
- GASP stock (ambito locomotion e ABP): prima di una deviazione chiedi all'Unreal Assistant come lo fa GASP stock e scrivi la riga in `docs/notes/gasp-divergenze.md` (cosa, perché, alternativa stock, costo per tornare indietro).
- Input: nei brief i tasti sono solo i default dello schema; il codice usa azioni.
- Review esterne: Neon via Antigravity torna per le review quando tornano i crediti, con la taratura cieca prevista; la review finale resta sotto le regole di `regole-animazione.md`.
- Copie degli artifact solo sul branch `claude/artifacts`.
- Thread di turno (opzionale): su ogni PC un thread lungo può prendere dalla coda del coordinatore una voce alla volta; una decisione bloccante parcheggia la voce e si passa alla successiva.

## 6. Anti-regressione (perché non ricapiti)
Il calo del novembre 2026 è nato da tante piccole aggiunte, ognuna ragionevole da sola (diagnosi in `handoffs/diagnosi-calo-qualita-2026-11-24.md`). Le guardie:
1. **Tetti di dimensione.** Istruzioni di progetto ≤ 5 KB; questo file ≤ 8 KB; `regole-animazione.md` ≤ 8 KB; memoria di progetto ≤ 40 file, ≤ 80 KB in tutto, `MEMORY.md` ≤ 40 righe di soli link. Il bibliotecario del venerdì misura (`wc -c`, `ls | wc -l`) e, se un tetto è superato, propone all'owner cosa togliere o unire. Non aggiunge testo per risolvere.
2. **Una regola entra, una esce.** Ogni regola o eccezione nuova sostituisce una esistente oppure porta una scadenza e un ambito. Il venerdì il bibliotecario toglie quelle scadute e lo scrive nel rapporto.
3. **Nessun tetto sul ragionamento.** Vietati limiti su log letti, effort, contesto, numero di tentativi di comprensione e materiale della review di livello 2. Se l'uso settimanale non basta, si riducono i thread o si propone all'owner di tornare a Max per le settimane pesanti.
4. **La memoria non è una bacheca.** Solo fatti durevoli: preferenze dell'owner, macchine, architettura, lezioni, clip bloccate, decisioni aperte. Lo stato del giorno va in `bibliotecario/STATO-ATTUALE.md`. Ogni memoria ha la data; il venerdì quelle oltre 30 giorni senza modifiche si rivedono (tenere, unire o archiviare in `handoffs/archivio/`).
5. **Indicatori settimanali** (il bibliotecario li mette nel rapporto del venerdì):
   - thread aperti al giorno (allarme sopra 15);
   - quota di thread sul metodo (allarme sopra 25%);
   - quota di ESITO con "non compilato" o "non provato" (allarme sopra 30%);
   - KB delle istruzioni e della memoria.

   Un allarme va all'owner con 2-3 opzioni, non diventa una regola nuova da solo.
