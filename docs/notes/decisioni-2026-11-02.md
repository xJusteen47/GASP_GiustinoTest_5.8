# Decisioni del mattino, 2 novembre 2026

Sei decisioni rimaste aperte dai thread della notte (M7 piggyback, spin/trapano, pipeline animazioni).
Per ognuna: contesto, opzioni, cosa implica ciascuna, opzione consigliata. Fonti: `docs/notes/handovers/m7-piggyback.md`
(M7-01, M7-02), `docs/notes/handovers/spin-drill.md` (SD-01, SD-02), `docs/notes/handovers/anim-pipeline.md` (checkpoint 1 e 2
nella storia git del file, fase B al tip di `claude/project-thread-s125cq`), `docs/architecture/plans/spin-drill-plan.md`,
`docs/architecture/combat-m7-piggyback.md`.

Nota: il thread "Implementare M7 G3 e G6 del piggyback" sta già facendo G3 (credito al partner) e G6 (uscite predette del rider,
seam S1 in GSTNLink) sul laptop. Non è in discussione qui.

Per rispondere basta un messaggio al coordinatore del tipo: `1A, 2 ok/ok, 3 confermo, 4 fermo/ok, 5 sì/codice/sì, 6 B`.

---

## 1. Abilitare GSTNPiggyback nel `.uproject`

**Contesto.** M7 ha consegnato il plugin GSTNPiggyback (mount, dismount, drop, kit di test, 15 test headless, 5 scenari harness
verdi a 0 % e 2 % di perdita) senza aggiungerlo alla lista plugin del `.uproject` committato: il file su feature non elenca
GSTNPiggyback, e nemmeno GSTNLink, GSTNDefence o GSTNSpinDrill. I thread li accendono localmente per build e test
(variante `uproject.pb`), poi ripristinano il file byte per byte. Il plugin è progettato per essere spento senza effetti:
niente tag, comando Mount senza binding, grab e M6 invariati.

**Opzioni.**
- **A. Resta OFF** (stato attuale). Costo zero. Il piggyback esiste solo nelle build dei thread; chi apre l'editor sul fisso non lo
  vede, non può provarlo in PIE, le mappe di test non lo caricano. Nessun rischio di regressione per M1-M6.
- **B. ON subito.** Un commit di una riga. Sblocca la prova in editor e i playtest del mount. Implica: la build dell'editor e la
  suite headless completa crescono (+15 test, un modulo in più da compilare); ogni modifica a GSTNLink o GSTNCombat deve passare anche
  con il piggyback acceso; i kit dei personaggi di gioco restano senza `CarrierProfile`/`RiderProfile` e la `FGSTN_MountRules` del
  game mode manca, quindi "acceso" significa ancora "ogni mount rifiutato" finché non si crea quel contenuto (lavoro di setup, mezza
  giornata di un thread Sonnet).
- **C. ON a fine G6**, insieme alle uscite predette del rider. Stessa riga di B, ma dopo che il comportamento di rete è quello che
  hai deciso (rider predetto, server che riavvolge il carrier). Evita di provare in editor una versione del mount che cambierà.

**Cosa cambia per gli altri thread.** Finché GSTNLink, GSTNDefence e GSTNSpinDrill restano fuori dal `.uproject`, accendere solo
GSTNPiggyback è incoerente: la regola vera da decidere è "quando i plugin M6/M7/spin entrano nel `.uproject` di default". Proposta:
tutti insieme a un gate di milestone (fine G6 e fine D10), con un commit solo e una build on/off di verifica.

**Consigliata: C** (ON a fine G6, insieme agli altri plugin di combattimento). Se vuoi provarlo subito in editor, B è a basso rischio
ma senza il contenuto dei kit non vedrai un mount riuscito.

---

## 2. Le due domande del checkpoint 1 della pipeline animazioni

**Contesto.** Il checkpoint 1 (spike sul Control Rig, branch `claude/project-thread-mkwsb0`) ha dato GO al rig `CR_Mannequin_Body`
del GASP sul mannequin UEFN a 60 fps e NO-GO a 62,5 fps: l'export di Unreal rifiuta frame rate non multipli o divisori di 30.
Il checkpoint 2 ha già applicato le risposte consigliate "senza owner" (regola 6) e la fase B è costruita sopra. Confermare
non costa nulla; cambiare idea ora costa un rifacimento.

**Domanda 2a: provare 62,5 fps cambiando il frame rate delle Project Settings, o restare a 60?**
- **Resta a 60** (applicato). I frame di gioco G diventano frame clip con `round(G x 0,96)`: ogni 25 frame di gioco uno viene
  condiviso da due frame clip. Costo zero. Rischio: una posa chiave può cadere a metà fra due frame di gioco; finora non si è visto
  nei controlli (contatto e FAF passano).
- **Prova 62,5**. Un mezzo thread Sonnet (mezza giornata, ~$3-5) per cambiare `Default Frame Rate` nelle Project Settings, riprovare
  export e import, verificare che Sequencer, montage e `bake_sockets.py` (che assume 60) non si rompano. Vantaggio: mappa 1:1 fra
  frame di gioco e frame clip, niente 0,96. Rischio alto: tutte le clip GASP esistenti sono a 30/60, i tool di Epic e il
  retargeting assumono multipli di 30; probabile fallimento di nuovo in export.

**Domanda 2b: accettare 1 cm di errore su colonna e ginocchia nel round trip delle mosse vecchie?**
Il round trip (animazione densa -> bake sui controlli -> AnimSequence) riproduce mani, testa, bacino, piedi e pugno entro 0,06 cm,
ma `spine_05` sbaglia 0,96 cm e i polpacci 0,58 cm, perché la colonna del rig è una IK spline e il ginocchio ha un pole vector.
Il brief chiedeva 0,5 cm.
- **Accetta 1 cm** (applicato). Vale solo per le mosse nate dense (jab v11/v12, Ganondorf v3.1) portate sul rig; le mosse nuove
  nascono direttamente sul rig, quindi non hanno round trip. Un centimetro sulla colonna non è visibile in gioco a camera distante.
- **Esigi 0,5 cm.** Serve un rig con FK puro sulla colonna, o un passaggio di correzione post-bake: 1-2 giorni di lavoro Opus
  (~$20-30), per un beneficio invisibile. Non consigliato.

**Consigliata: 60 fps, 1 cm accettato** (confermare le due scelte già applicate).

---

## 3. Confermare: stretch del rig OFF e soglia lunghezza ossa 0,1 cm

**Contesto.** Il controllo `bone_lengths` boccia un build se un osso cambia lunghezza di oltre 0,1 cm durante la clip. Sul rig,
alcuni controlli hanno interruttori di stretch (braccia, gambe, collo) che allungano le ossa quando l'effettore IK è lontano:
anche la jab v11 densa falliva (max clavicola 1,29 cm; la sorgente stessa falliva già a 0,98). Nel checkpoint 2 il coordinatore
ha scelto "stretch spento, soglia 0,1 tenuta"; la fase B l'ha applicato (`lock_lengths`: traslazioni locali fisse sul bake,
tolto fino a 1,56 cm sul collo) e ora entrambe le mosse passano.

**Opzioni.**
- **A. Stretch OFF + 0,1 cm** (applicato). Le ossa restano rigide come il personaggio in gioco: niente braccia che si allungano
  per raggiungere un bersaglio, un difetto molto visibile nei picchiaduro. Costo: quando una posa dell'owner chiede un raggio
  maggiore dell'arto, il pugno non arriva al punto voluto e il controllo di contatto lo segnala; la posa va corretta a mano.
- **B. Stretch ON + soglia "non peggio della sorgente".** Più tollerante, le pose arrivano sempre a destinazione, ma si accetta
  stretch fino a ~1,5 cm e il controllo smette di essere un invariante: diventa un confronto relativo. Rischio: stretch che si
  accumula senza che nessuno lo veda.
- **C. Stretch OFF + soglia 0,5 cm.** Via di mezzo: tollera il rumore del bake IK spline senza lock. Oggi non serve, perché con
  `lock_lengths` si passa già a 0,1.

**Consigliata: A, confermare.** Costo zero. Se in futuro una posa dell'owner richiede stretch espressivo (squash and stretch
tipo cartoon), si riapre per quella mossa.

---

## 4. Pose della jab e braccia di Ganondorf (fase B)

**Contesto.** La fase B ha ricostruito jab v12 e Ganondorf ftilt v3.1 con il motore unico (`animpipe build`). Jab: 6/6 controlli
bloccanti passati, ma al frame di contatto il braccio di guardia (pugno posteriore) scatta indietro con il busto e il pugno gira
di 36° in un frame. Due tentativi mirati (togliere la torsione della mano, poi mano e braccio) non l'hanno tolto: i 36° vengono
dallo swing del braccio di guardia, non dalla torsione. Quindi serve una scelta di posa, non un fix tecnico. Ganondorf: il gomito
dietro è rientrato nel profilo (fuori per 13 frame invece di 31) e l'avambraccio sinistro non entra più nel fianco (+4,1 cm di
clearance invece di -4,0), ma all'A/B cieco il braccio sinistro al frame 10 visto di fronte "si chiude a triangolo verso il busto".
Il lavoro di animazione è in pausa finché non rispondi.

**Domanda 4a: pugno posteriore della jab.**
- **Fermo al mento.** La guardia segue il petto ma la mano resta al mento; cambio di posa al frame 1. Lettura: jab "da boxe",
  guardia sempre chiusa, coerente con la tua regola "pugni chiusi, gomito dietro dentro il torso". Costo: un edit nella scheda
  (mezz'ora Sonnet, ~$1), rebuild 10 s. Rischio: meno sensazione di forza al contatto (meno contro-rotazione del busto visibile).
- **Scatto stile Mario.** Si accetta che la guardia scatti indietro col busto al contatto, come nel riferimento Mario jab1. Più
  forza percepita (priorità 1 della tua lista: peso e forza), ma il pugno dietro si vede "saltare" di 36° in un frame: a camera
  distante e con hitstop di 1-2 frame probabilmente non si nota, a camera vicina sì. Costo zero (è lo stato attuale).
- **Via di mezzo.** Scatto del busto ma mano che ruota in 2-3 frame invece di 1: già provato in parte (il tentativo 2 non ha
  migliorato), quindi richiederebbe pose intermedie tue, non un fix dell'agente.

**Domanda 4b: nuove braccia di Ganondorf OK?** Griglia `ganon_ftilt_v31_poses_ab.png` (build sopra sorgente) nei project files.
- **OK.** Si chiude Ganondorf v3.1 sul rig e si passa al giudizio in gioco.
- **No, correggi il braccio sinistro.** Un edit alla scheda per ridurre l'abduzione al frame 10 (il "triangolo" verso il busto),
  rischiando di riportare l'avambraccio nel fianco. Mezz'ora Sonnet, ~$1, poi nuova griglia da guardare.

**Consigliata: 4a fermo al mento** (coerente con le tue regole di guardia; la forza la danno hitstop e trail in gioco, non lo scatto
del pugno dietro), **4b OK** con verifica in gioco nel prossimo thread (montage + hitstop + camera Follow), che è comunque il passo
successivo e costa meno di un altro giro di griglie.

---

## 5. Domande SD-02 del trapano (spin/drill D9 parte 1)

**Contesto.** D9 parte 1 è su feature (13cc565): il latch del trapano è un tipo di link GSTNLink (`Combat.Link.Type.Drill`, ruoli
Driller/Drilled, regola `UGSTN_LinkRule_Drill`, tipo `FGSTN_DrillLinkType` registrato in codice a PostEngineInit). 950/0 headless.
Il thread si è fermato con tre domande; il prossimo thread spin (D9 parte 2: bore, lancio, pop; poi D10 bersagli mobili) parte
quando il laptop si libera dal thread G3/G6.

**5a. Anchor per-latch: il driller sta a offset fisso (0, 0, 100) sopra la radice del bersaglio. Serve un seam in GSTNLink?**
- **Sì, chiedi il seam** (offset per link nello stato del link, scritto dall'iniziatore al latch). Il driller resta dove la punta
  ha colpito: su un prop grande o un nemico grosso si vede la differenza, e D10 (bersagli mobili) ne ha bisogno per far seguire il
  driller al punto di contatto. Costo: una modifica a GSTNLink (plugin oggi congelato per D9/D10, posseduto dal thread laptop
  attivo): mezza giornata Opus medium (~$5-8), test headless, e deve essere coordinata con il seam S1 che il thread G6 sta già
  facendo in GSTNLink. Il piano organizzativo diceva già che il seam S1 di G6 va disegnato "per servire anche l'anchor per-latch
  dello spin": questa è la strada naturale.
- **No, offset fisso per ora.** D9 parte 2 e D10 procedono sull'offset; l'anchor arriva dopo. Rischio: D10 misura le correzioni
  per latch su un bersaglio mobile con un placement che poi cambierà, quindi le misure andranno rifatte.

**5b. `FGSTN_DrillLinkType` come asset (`DA_LinkType_Drill` via script di setup) o come codice?**
- **Codice** (stato attuale, come il mount di M7: `FGSTN_MountLinkType` registrato a PostEngineInit). Nessun asset da creare con
  l'editor aperto, nessun contenuto da versionare, i test headless non dipendono da Content. I designer non possono regolare
  leash, pending frames o ruoli senza ricompilare.
- **Asset.** Coerente con `UGSTN_LinkTypeDefinition` di M6 (grab e latch sono asset). Regolabile dai designer, visibile nel
  Content Browser. Costo: uno script di setup eseguito con l'editor libero (un'ora Sonnet), Content versionato nel plugin
  (come fatto per GSTNPiggyback), più un fallback in codice se l'asset manca. Rischio: due tipi di link del piggyback e del
  trapano fatti in modo diverso dai tipi M6.
- **Entrambi**: codice ora, asset quando l'editor è libero, con il codice come fallback. È ciò che M7 ha fatto per il mount.

**Consigliata: codice per D9/D10, asset a fine milestone** se vorrai regolare i parametri da editor. Non blocca nulla.

**5c. "Non trapanabile" = tag `Combat.Immunity.Drill` fino a D12?**
D12 porta i bridge con GSTNParts e GSTNLevelObjects, dove "trapanabile" diventerà penetrazione contro durezza del materiale.
- **Sì, tag fino a D12.** Semplice, già fatto, i test lo usano. Rischio minimo: a D12 il tag resta come override esplicito
  ("mai trapanabile") sopra la regola di durezza.
- **No, modella la durezza adesso.** Anticipa D12 dentro D9: un attributo di durezza sul bersaglio e una soglia di penetrazione
  sul trapano. Costo: un giorno Opus (~$10-15) e dipendenza da M4b parts, non ancora chiusa. Non consigliato.

**Consigliata: sì, tag fino a D12.**

---

## 6. Test instabile `latch_big_enemy_host_latches_client` (SUITE_LINK)

**Contesto.** Lo scenario harness fa colpire da host un nemico grosso che si aggancia a un client. Il controllo "al massimo una
correzione per colpo nella finestra d'attacco" conta i passi del contatore di correzioni del client: limite 4, misurato 3-4-4
prima del rebase sullo spin e 3-5 dopo; fallisce in 3 run su 5. Il triage SD-02 ha escluso lo spin (con `RehitIntervalFrames 0`
la chiave dell'anello è identica byte per byte; nessun commit spin tocca file fuori da GSTNSpinDrill nelle run che non lo
abilitano): è un flake di margine, il test era già al limite. Nessuna modifica al codice. La domanda è quale criterio adottare
prima di toccare il limite.

**Opzioni.**
- **A. Conteggio per colpo.** Il test smette di contare i passi totali nella finestra e conta le correzioni per ogni colpo,
  con limite 1-2 per colpo. Più preciso e più vicino all'intento ("una correzione per colpo"). Costo: modifica allo scenario
  in `Tools/GASNPPTest` (2-3 ore Sonnet, ~$2-3) e un giro di harness 0 % (editor, ~20 min). Rischio: il contatore attuale non è
  attribuibile a un colpo preciso senza esporre il frame del colpo nel report, piccola aggiunta al recorder.
- **B. Distribuzione su 10 run.** Si lanciano 10 run dello scenario (~3-4 ore di editor sul laptop, nessun codice), si guarda
  la distribuzione (3, 4, 5?) e si alza il limite a un valore con margine (per esempio 6) solo se la distribuzione è stabile.
  Costo quasi solo macchina. Rischio: si "allarga il limite" senza capire perché il margine è peggiorato dopo il rebase; se c'è
  una regressione vera (M7 sospetta i seam D6/D7, SD-02 lo esclude), la nasconde.
- **C. Entrambi, nell'ordine B poi A.** Prima i dati (B, gratis), poi il criterio per colpo (A) con il limite tarato sui dati.

**Consigliata: C**, con B fatto dal thread spin la prossima volta che il laptop è libero (il ciclo dell'harness è già nel suo
flusso) e A nel thread che toccherà il recorder. Finché non è deciso, il test resta nella suite e il 18/19 è accettato come noto.

---

## Riepilogo delle raccomandazioni

| # | Decisione | Consigliata |
|---|---|---|
| 1 | GSTNPiggyback nel .uproject | C: ON a fine G6, insieme a GSTNLink/Defence/SpinDrill |
| 2 | Checkpoint 1 anim | Confermare 60 fps e 1 cm su colonna/ginocchia |
| 3 | Stretch rig e soglia ossa | Confermare OFF + 0,1 cm |
| 4 | Pose jab e Ganondorf | 4a pugno fermo al mento; 4b OK, giudizio in gioco |
| 5 | SD-02 trapano | 5a sì seam anchor (via S1 di G6); 5b codice ora, asset dopo; 5c tag fino a D12 |
| 6 | Test instabile latch | C: 10 run prima, poi conteggio per colpo |

Costo di questo thread: Fable, ~15 chiamate, ~45k contesto, circa $2-3.
