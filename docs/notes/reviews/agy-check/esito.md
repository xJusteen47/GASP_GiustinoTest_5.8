Data: 2026-11-15. Sezioni poste ad Antigravity: A, B, C (solo domande). Sezione D: in attesa.
Ottenuto con il tool MCP agy-bridge `delegate` (modello Gemini 3.8 Flash (High), session 2f694cf7-ca2a-4177-a2ef-f503a3ea42b1). Testo sotto = risposta verbatim.
---

## A. Cosa sai eseguire

**1. Quali modifiche a una mossa esistente fai in modo affidabile, e quali no?**
* **Affidabili:** Modifiche numeriche e puntuali a chiavi esistenti su controller FK/IK specifici (es. rotazione di un arto, traslazione del bacino, correzione orientamento busto), retiming di chiavi sparse (allungare/accorciare anticipazione, hold o recovery nella card), correzione di scivolamento piedi (`PLANT_TOL`) e correzione tangenti (lineare per snap/smear, cubic/auto per curve morbide).
* **Non affidabili:** Istruzioni qualitative o vaghe ("rendilo più pesante", "migliora la posa"), risoluzione automatica di compenetrazioni multi-giunto complesse senza indicazione dell'asse o giunto da spostare, e difetti legati alle transizioni o al blend-out dell'AnimBP in-game.
* *Esempio concreto:* In Overlord Jab 1, affidabile portare il polso destro a f6 a `(-17.0, 78.0, 134.0)` e la mano sinistra a riposo sul fianco a `(23, 7.5, 97)`. Non affidabile: "risolvi l'accosciata a fine mossa", che dipendeva dal blend-out del Montage verso l'AnimBP e non dalle chiavi della clip.

**2. Di quante pose chiave e di quali informazioni hai bisogno per partire? Cosa ti mancava con le tre pose di jab 1 e jab 2?**
* **Requisiti per partire:** Almeno 4-5 pose fondamentali: 1) Frame 0 / Rottura dell'idle, 2) Carica massima / Chamber, 3) Contatto / Impatto estremo, 4) Overshoot / Apex del follow-through, 5) Recupero verso il FAF. Informazioni necessarie: frame esatti a 60 fps, coordinate o rotazioni di bacino, busto, arto attivo, arto secondario e piedi, più finestre di hitstop e FAF.
* **Cosa mancava con sole tre pose (Jab 1 e 2):** Mancava il breakdown di anticipazione (il pugno partiva piatto dall'idle senza avvitamento interno del busto), mancava la posa di overshoot dopo l'impatto (il braccio frenava in modo innaturale), mancava la guida dell'arco spaziale (interpolando in linea retta la mano attraversava il petto violando il clearance), e in Jab 2 mancava la gestione del passo (il corpo restava avanti di 28 cm al FAF).
* *Esempio concreto:* Con 3 pose in Jab 1, tra carica e colpo la mano destra compenetrava il torace di 3.5 cm; è stato necessario definire i breakdown a f1, f2, f4 e f5 per far compiere al braccio un arco pulito attorno al corpo.

**3. Quando ricevi "aggiungi più follow-through qui", cosa cambi di preciso: quali controlli, quali chiavi, quali tangenti? Aggiungi chiavi nuove? Quante?**
* **Controlli:** Effettore primario (`hand_r_fk_ctrl` / polso), rotazione del tronco (`spine_05`) e del bacino (`pelvis`), e assorbimento secondario ritardato di testa (`head_ctrl`) e mano libera.
* **Chiavi e tangenti:** Aggiungo tipicamente **1 sola chiave nuova (massimo 2)**: l'apice dell'overshoot 1-3 frame dopo l'impatto, ed eventualmente un settle intermedio. Sulla chiave di impatto mantengo una tangente in uscita ripida (senza frenata pre-contatto), mentre sulla chiave di overshoot imposto una tangente orizzontale/piatta per evitare rimbalzi spline.
* *Esempio concreto:* In Jab 1, dopo l'impatto a f6, a f7 e f8 (overshoot apex) il polso continua la corsa lungo l'arco passando da 40° a 46.5° a destra di Y (+4.1 cm a f7, +2.7 cm a f8), la testa subisce un rinculo (+2° mento a f8), e solo a f9 inizia il rientro.

**4. Quando ti serve un'istruzione in cm e gradi invece di una di principio?**
* Quando si toccano vincoli fisici o geometrici precisi: 1) Allineamento con il bersaglio e hitbox (per evitare che il colpo manchi la sagoma da una certa visuale), 2) Risoluzione di fallimenti bloccanti dei gate numerici (`clearance < 2.5 cm`, distorsione ossa, tolleranza piedi piantati `PLANT_TOL > 2.0 cm`, hitstop snap `> 10.0 cm`), 3) Quando un'istruzione di principio è fallita per due cicli consecutivi a causa di interpretazioni ambigue.
* *Esempio concreto:* In Jab 1, l'istruzione di principio "colpisci dritto davanti sul manichino" era ambigua; è servita l'indicazione misurata "mano non oltre 25 cm a destra dell'asse (X >= -25) e almeno 75 cm avanti (Y >= 75)" per toccare il bersaglio a 95 cm sia di lato sia da dietro.

**5. Quali principi di `docs/notes/anim-theory/` applichi con sicurezza e quali no? Li leggi a ogni sessione?**
* **Con sicurezza:** Rapporti di accelerazione e spacing charts (es. spacing crescente 35%/65% prima dell'impatto senza decelerazione pre-hitbox), regola del primo frame (f0 che rompe subito l'idle), gestione della posa di hitstop (nessun pop > 10 cm nel frame successivo), piedi piantati entro tolleranza e rientro al FAF.
* **Non con sicurezza:** Silhouette e leggibilità 2D dalla telecamera di gioco (richiedono giudizio visivo prospettico), archi spaziali complessi su catene FK tridimensionali (facili a generare cuspidi) e l'overlap organico delle parti secondarie (spesso risulta rigido o "a molla").
* **Lettura:** Non rileggo l'intero file da zero a ogni sessione: applico le regole codificate nei controlli di `checks.py` e nelle note di handoff, consultando le sezioni di teoria solo quando il revisore cita un principio specifico.
* *Esempio concreto:* Applicato con sicurezza lo spacing di colpo Williams in Jab 1 (f4->f5 44.9 cm, f5->f6 73.2 cm). Non applicato con sicurezza: la silhouette di Jab 2 vista da dietro a f42, dove il braccio che colpiva risultava interamente nascosto dal busto.

---

## B. Come lavori oggi

**6. Dove scrivi le chiavi: direttamente nel Sequencer o nella scheda di `animpipe` (`Tools/AnimPipeline/cards/`)? Con quali comandi?**
* Le chiavi le scrivo esclusivamente nella scheda JSON della mossa in `Tools/AnimPipeline/cards/<kit>/<move>/<move>.json` (blocco `"author": { "poses": { ... } }`).
* Comandi utilizzati:
  1. Modifica della scheda JSON (manualmente o via script generatore/scratch).
  2. `python -m animpipe author <card.json>` per compilare e validare le pose nel source take.
  3. `python -m animpipe build <card.json>` per rigenerare la LevelSequence in Sequencer, inserire chiavi/tangenti sul Control Rig ed esportare l'AnimSequence finale.
* *Esempio concreto:* Per Overlord Jab 1 ho modificato le pose in [overlord_jab1_v3.json](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/Tools/AnimPipeline/cards/overlord/jab/overlord_jab1_v3.json) e poi ho lanciato `python -m animpipe author` e `build`.

**7. Come leggi le modifiche fatte dall'owner in Unreal? Distingui le sue chiavi dalle tue e riesci a non toccarle? Che cosa è `user_tweaked_keys.json` e quando lo scrivi?**
* **Come leggo le modifiche:** Attraverso il layer additivo "Manuale" in Sequencer. `animpipe build` genera una sezione additiva ("Manuale") sopra la base; prima del rebuild, il sistema estrae quelle chiavi tramite `animpipe capture` e le salva in `cards/<kit>/manual/<move>.json`.
* **Distinzione chiavi:** Se l'owner usa il layer "Manuale", le sue modifiche restano separate e additive. Se invece l'owner modifica direttamente la traccia base di Control Rig in Sequencer, `animpipe build` cancella e ricrea la sequenza, distruggendo le modifiche dell'owner.
* **`user_tweaked_keys.json`:** È un dump JSON grezzo (presente nella root del progetto) contenente le tracce e i valori di trasformazione estratti da Sequencer (`MovieSceneControlRigParameterSection_...`). Viene scritto da script di dump/ispezione per congelare lo stato delle modifiche manuali dell'utente prima di alterare la pipeline.
* *Esempio concreto:* In [user_tweaked_keys.json](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/user_tweaked_keys.json) compaiono le modifiche ai controller della gamba sinistra (`thigh_l_fk_ctrl`, `calf_l_fk_ctrl`) a f9 salvate durante un sessione manuale in editor.

**8. Quante chiavi usi per mossa? Scrivi mai una chiave per ogni frame? In quali casi?**
* **Uso normale:** Tra le 8 e le 15 chiavi sparse per una mossa da 30 frame; 15-22 chiavi per mosse più lunghe (50+ frame).
* **Una chiave per ogni frame:** SÌ, è successo in Jab 1 e Jab 2 durante il round 2 (`overlord_jab_v3_round2_notes.md`).
* **In quali casi:** Quando l'interpolazione del Control Rig in Unreal diverge dai calcoli dello stick-solver offline di `author.py`, oppure quando ci sono fallimenti severi nei check di fidelity/clearance e si tenta di forzare la traiettoria "fissando" matematicamente ogni singolo frame (in Jab 1 r2 sono stati scritti tutti i frame da 0 a 22; in Jab 2 r2 da 0 a 26). È un anti-pattern che annulla la sparsità e rende la clip non modificabile dall'owner.
* *Esempio concreto:* In `overlord_jab1_v3.json` (r2), i frame 0-22 contengono ciascuno una chiave di posa completa per forzare la curva ed evitare derive dell'in-between.

**9. Quali controlli lanci da solo dopo una modifica? Cosa non puoi vedere (immagini, video)?**
* **Controlli automatici eseguiti da solo:**
  1. `animpipe author` (controllo sintassi e lunghezze ossee `bone_lengths`).
  2. `animpipe check` (check bloccanti: compenetrazioni `clearance`, picco portata `contact_faf`, piedi piantati `planted`, scatto post-colpo `hitstop_snap <= 10 cm`, mani chiuse/aperte).
  3. `animpipe fidelity` (scostamento dalle pose owner di riferimento <= 0.5 cm / 1°).
  4. `animpipe verify --req=<file>` (verifica numerica dei requisiti di angolo, delay e ampiezza).
* **Cosa NON posso vedere:** Non posso vedere il playback video a 60 fps in tempo reale (percepire il "peso" o il "ritmo" biologico a velocità reale), né posso valutare la qualità estetica della silhouette 2D da varie angolazioni prospettiche senza l'analisi visiva del revisore.
* *Esempio concreto:* Verifico in autonomia che a f6 la mano destra disti almeno 2.6 cm dal busto (`clearance PASS`); non posso invece vedere che da dietro il braccio è nascosto dal busto durante l'hitstop in gioco.

**10. Sai produrre una griglia prima/dopo sugli stessi frame e un PASS/FAIL per una verifica numerica (`animpipe verify`)?**
* **Griglia prima/dopo:** SÌ. Uso `clip_grid.py` per renderizzare catture lit a viste multiple dell'AnimSequence compilata in Unreal, oppure `animpipe poses --take=<nuova> --over=<vecchia>` per generare uno sheet stick-figure sovrapposto prima/dopo sugli stessi frame.
* **PASS/FAIL numerico:** SÌ. `animpipe verify` (`Tools/AnimPipeline/animpipe/verify.py`) è già implementato: riceve un JSON di requisiti (`angle`, `delay`, `amplitude`), misura le ossa dal dump `_legbones.json` e stampa `PASS`/`FAIL` con i valori misurati per ciascuna voce.
* *Esempio concreto:* Lanciando `python -m animpipe poses cards/overlord/jab/overlord_jab1_v3.json --over=OverlordJab1V2_Build` si ottiene l'immagine comparativa con la vecchia versione in grigio sotto la nuova a colori; lanciando `animpipe verify` con un req sull'angolo del busto a f17 viene stampato `PASS torso_f17: 18.2 deg (want 10..25)`.

**11. Puoi lavorare leggendo una istruzione alla volta da un file e scrivendo l'esito in un file, senza altro testo dall'owner?**
* **Sì.** È la modalità operativa ideale: leggo il file `istruzione.json`, applico la modifica alla scheda, eseguo `build` e `check`/`verify`, genero l'immagine di verifica e scrivo un file strutturato `esito.json` contenente esito numerico (PASS/FAIL), metriche prima/dopo e path della griglia, terminando senza attendere chat.
* *Esempio concreto:* Leggo l'istruzione `{"chiave": 6, "parte": "arm_r", "modifica": "wrist Y += 5cm"}`, modifico la card, lancio `build`, misuro la distanza dal bersaglio con `verify`, salvo la griglia in `sheets/grid_j1_step1.png` e scrivo l'esito formattato in `esito.json`.

**12. Cosa c'è di non committato nel tuo worktree?**
* **Non verificabile tramite comando `git`:** Il comando `git status` fallisce perché il file `.git` del checkout punta al path del worktree principale (`.git/worktrees/GASP_Giustino2_ue59`), che si trova all'esterno della cartella di lavoro sandbox.
* **Stato verificato sul filesystem:** Risultano modificati/non tracciati localmente:
  - Root: [user_tweaked_keys.json](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/user_tweaked_keys.json) (146 KB).
  - In `Tools/AnimPipeline/cards/overlord/jab/`: le card `overlord_jab1_v3.json`, `overlord_jab2_v3.json`, i relativi backup (`.r0`, `.r1`, `.r2a`), i rendering stick (`overlord_jab1_v3_r2b_stick.png`) e le note di round 1 e round 2.
  - In `Tools/AnimPipeline/cards/overlord/dropkick/`: le card e i plan del dropkick (`overlord_dropkick_startup.json`, `overlord_dropkick_loop.json`, `overlord_dropkick_land.json`).
  - In `sheets/raw/`: i dump json di readback e legbones (`OverlordJab1V3_Build_legbones.json`, ecc.).
  - In `docs/notes/reviews/`: le cartelle `overlord-jab-v3-r1/` e `overlord-dair-v9-blendout/`.
* *Esempio concreto:* [overlord_jab1_v3.json](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/Tools/AnimPipeline/cards/overlord/jab/overlord_jab1_v3.json) ha dimensione di 40 KB rispetto ai 7 KB della versione base ed è modificato localmente.

---

## C. Critica del piano

**13. Quale dei dieci punti del piano NON riusciresti a rispettare, e perché?**
* **Il Punto 9** (*"L'owner deve poter modificare pose e chiavi da Unreal e ritrovarle rispettate al passo dopo"*).
* **Perché:** L'architettura attuale ha la card JSON come unica sorgente di verità: `animpipe build` cancella e rigenera `LS_<name>` in Sequencer. Se l'owner sposta controlli sulla traccia base anziché sul layer additivo "Manuale", al build successivo la modifica viene cancellata. Inoltre, non esiste un parser bidirezionale infallibile che converta le curve di Control Rig dell'editor nelle pose sparse della card senza introdurre scarti o alterare altre parti.
* *Esempio concreto:* Se l'owner apre `LS_OverlordJab1`, abbassa il gomito a f6 sulla traccia base e salva, al successivo `animpipe build overlord_jab1.json` la LevelSequence viene rigenerata dalla card e il ritocco dell'owner scompare.

**14. Dove fallirebbe più spesso questo ciclo? Indica i tre casi peggiori, con una mossa di esempio.**
1. **Conflitto tra vincoli geometrici opposti (coperta corta):** Risolvere un vincolo rompe un altro vincolo o gate (es. allungare il pugno per toccare il dummy porta l'estensione del braccio oltre il limite consentito, oppure ruotare il corpo per la vista da dietro nasconde la sagoma di lato).
   * *Esempio:* In **Overlord Jab 1**, per far toccare la mano sul petto del manichino a f6 a 95 cm di distanza è stato necessario sbilanciare il bacino in avanti, provocando la flessione non voluta del ginocchio anteriore e riducendo l'angolo di inclinazione del pugno da 40° a 29°.
2. **Deriva dell'in-between su chiavi sparse:** Il revisore chiede di correggere il passaggio intermedio su un frame non chiave; senza aggiungere chiavi, toccare le tangenti dei frame vicini crea overshoot e compenetrazioni indesiderate altrove.
   * *Esempio:* In **Overlord Jab 2**, tentare di eliminare il frame "morto" a f3 modificando solo le tangenti di f0 e f6 ha fatto sprofondare la mano sinistra dentro la coscia durante il caricamento.
3. **Difetti di sistema (AnimBP/Montage) scambiati per difetti di posa:** Il revisore rileva un problema nel video in gioco che non risiede nella clip ma nel blend-in/out del Montage o nelle transizioni di stato. L'esecutore modifica le pose a vuoto senza poter risolvere il difetto.
   * *Esempio:* In **Overlord Jab 1 r1**, l'accosciata a terra a fine mossa (v45) era causata dal blend-out anticipato del Montage che svelava la posa di locomotion sottostante dell'AnimBP: iterare sulle chiavi di recupero della clip non ha risolto il problema.

**15. I sette campi dell'istruzione: quale ti manca per eseguire senza interpretare, e quale è inutile?**
* **Campo mancante:** Lo **spazio di coordinate e il controller esatto** (es. `upperarm_r_fk_ctrl.Rotation.Pitch` o `hand_r: component loc [X, Y, Z]`), insieme all'intervallo di frame interessato. Indicare solo `parte: braccio_r` costringe l'esecutore a tirare a indovinare se muovere la spalla FK, il gomito, o il target IK.
* **Campo inutile:** **`principio`** (es. "principio 2: spacing accelerato"). Per l'esecutore è un'informazione concettuale ridondante: una volta definiti `intento`, `modifica` numerica e `verifica`, l'esecutore deve solo calcolare e applicare le coordinate, non interpretare la teoria.
* *Esempio concreto:* Ricevere `parte: gamba_sinistra, modifica: arretra di 10 cm` è ambiguo (muovo `foot_l_ik_ctrl` o ruoto `thigh_l_fk_ctrl`?); indicare `foot_l_ik_ctrl.Location.Y -= 10` rende l'esecuzione immediata e deterministica.

**16. Chiavi sparse e tetto di 8 chiavi aggiunte: è realistico per follow-through, overlap e rientro? Che numero proporresti?**
* **Realismo:** È realistico per mosse brevissime (un Jab da 15-20 frame). **Non è realistico** per mosse articolate o lunghe (un Jab 2 da 53 frame, o un Dropkick aereo con atterraggio), dove l'affondo delle gambe, il recupero a passi, l'overshoot del braccio e l'overlap della testa avvengono su frame sfasati.
* **Proposta:**
  - Non un tetto fisso assoluto di 8 chiavi per ogni mossa, ma un tetto legato alla durata: **massimo 1 chiave ogni 4-5 frame di animazione** (es. tetto di 8 chiavi per mosse under 30f; tetto di 16-18 chiavi per mosse da 50-60f).
  - Tetto all'aggiunta: **massimo 3 chiavi aggiunte per singolo ciclo**, distribuite sulle sole catene interessate, vietando categoricamente di passare al baking di ogni frame.
* *Esempio concreto:* In Jab 2 (53 frame), solo per gestire il passo in avanti (f9), l'overshoot (f11), l'avvio del disimpegno (f16), il rientro del piede sinistro (f19), del piede destro (f23) e il ritorno al FAF (f26) servono già 6 chiavi dedicate solo alle gambe; 8 chiavi totali lascerebbero a secco braccia e busto.

**17. Modifiche dell'owner da Unreal: qual è il modo più sicuro per rileggerle e non sovrascriverle? Cosa si rompe oggi?**
* **Modo più sicuro:**
  1. L'owner lavora esclusivamente sul layer Sequencer `Manuale` (additivo) o su una sequenza duplicata dedicata (`LS_<move>_Owner`).
  2. Prima di qualunque modifica dell'esecutore, uno script automatico estrae il delta delle chiavi dell'owner e lo sincronizza in `cards/<kit>/manual/<move>.json`.
  3. Il comando `build` fonde deterministicamente la scheda base con il delta del layer manuale.
* **Cosa si rompe oggi:** Se l'owner ritocca direttamente la traccia base di Control Rig in Sequencer, il primo lancio di `animpipe build` pialla completamente la sequenza rigenerandola dalla card JSON, cancellando ogni lavoro manuale non salvato nel layer additivo.
* *Esempio concreto:* Se l'owner ruota a mano la mano destra a f9 su `LS_OverlordJab2` sulla sezione base e salva l'asset in Unreal, al rebuild dalla card la traccia viene rigenerata dai valori della card e la posa dell'owner è persa.

**18. Se `animpipe build` ricostruisce la sequenza dalla scheda e tu scrivi nel Sequencer, chi vince? Come eviteresti il conflitto?**
* **Chi vince:** **Vince `animpipe build` (la scheda JSON).** Il codice di `ue_rig.py` ricrea o resetta i canali del Sequencer in base ai dati della card.
* **Come evitare il conflitto:**
  1. **Regola fondamentale di Single Source of Truth:** io (esecutore) non scrivo MAI chiavi direttamente in Sequencer; modifico solo la scheda JSON della mossa (`cards/...json`).
  2. Il Sequencer è un artefatto di sola compilazione.
  3. Se l'owner ha apportato modifiche in editor, il ciclo impone un passaggio preventivo di importazione (`animpipe ingest-owner`) che aggiorna la card prima di applicare la nuova istruzione.
* *Esempio concreto:* Invece di usare API di Sequencer per settare le chiavi durante i turni, modifico `overlord_jab1_v3.json` e lascio che sia `animpipe build` a produrre la LevelSequence corrispondente in maniera riproducibile.

**19. Quali controlli faresti tu e quali lasceresti al revisore, per non fare lo stesso lavoro due volte?**
* **Controlli all'ESECUTORE (matematici, deterministici, da gate):**
  - Integrità scheletro: allungamento ossa (`bone_lengths`), compenetrazioni minime (`clearance`), stato dita/fist.
  - Vincoli fisici/pipeline: scivolamento piedi (`PLANT_TOL <= 2 cm`), hitstop snap (`<= 10 cm`), FAF raggiunto, picco reach nella finestra attiva.
  - Verifica esatta dei requisiti espressi nell'istruzione via `animpipe verify` (angoli, ampiezze, delay).
  - Se uno di questi fallisce, l'esecutore corregge da solo prima di produrre la griglia.
* **Controlli al REVISORE (visivi, qualitativi, di gameplay):**
  - Leggibilità della silhouette 2D dalla telecamera di gioco e da 45°.
  - Percezione del peso e contrasto di ritmo nei video in gioco (1x e slow-motion).
  - Giudizio stilistico complessivo e allineamento con i riferimenti AAA.
  - Individuazione di anomalie di sistema a monte (glitch AnimBP, blend-in/out scorretti del Montage).
* *Esempio concreto:* L'esecutore verifica numericamente con `checks.py` che il piede destro non scivoli più di 0.08 cm durante il caricamento; il revisore guarda il video da dietro e giudica se l'impatto trasmette abbastanza violenza visiva.

**20. Che cosa aggiungeresti al piano che qui manca?**
1. **Comando di Rollback istantaneo a inizio ciclo:** La possibilità di ripristinare la card all'ultimo checkpoint funzionante (`animpipe revert`) se due istruzioni consecutive degradano la mossa.
2. **Gate di conformità AnimBP/Gameplay prima del ciclo di pose:** Se nel video iniziale in gioco si nota un difetto di blend del Montage o un frame freeze di sistema (es. l'accosciata a fine mossa o il salto a v35 in Jab 2), il ciclo di animazione deve essere **sospeso** fino alla correzione della catena di playback in UE; altrimenti si iterano pose su fondamenta errate.
3. **Tracciamento degli archi di movimento (Motion Trails) nella griglia:** Inserire nella griglia di verifica non solo i fotogrammi statici ma anche il tracciato 3D dell'arco dell'effettore (`animpipe trails`), che evidenzia immediatamente derive e cuspidi senza doverle dedurre da frame isolati.
4. **Regola di escalation da principio a misura:** Se un'istruzione di principio fallisce o produce un risultato inatteso al primo tentativo, il revisore ha l'obbligo di convertirla al secondo tentativo in istruzione misurata (specificando controller, asse e valore numerico esatto).
* *Esempio concreto:* Se sul Jab 1 fosse stato presente il gate AnimBP, ci si sarebbe fermati subito davanti all'accosciata a v45, identificando il blend-out a 4 frame del Montage prima di spendere due round di calcolo sulle chiavi di rientro del personaggio.

---
[agy-bridge] model: Gemini 3.8 Flash (High) | session: 2f694cf7-ca2a-4177-a2ef-f503a3ea42b1 (use follow_up to continue)


## Le due domande aggiunte (21 e 22), via agy-bridge `follow_up`, stessa sessione, risposta verbatim

**21. Punto 9: puoi lavorare solo su una copia della sequenza e lasciare che le tue modifiche rientrino per differenza? Sai leggere da una sequenza chiavi sparse e tangenti per controllo e dire cosa è cambiato rispetto a un'altra? Cosa fai già di simile?**
* **Lavorare su `LS_<mossa>_Agy` e rientrare per differenza:** Sì. Posso duplicare o aprire `LS_<mossa>_Agy`, modificare solo le chiavi e le tangenti richieste sui canali `MovieSceneScriptingFloatChannel` senza mai toccare `LS_<mossa>_Build` né scrivere a mano nella card, e lasciare che `ue-import` calcoli il diff e aggiorni la scheda. Il comando unificato `ue-import` non è ancora presente nel mio checkout (**non provato** end-to-end), ma tutte le primitive che gli servono sono già nel codice.
* **Leggere chiavi sparse, tangenti e calcolare il diff tra due sequenze:** Sì. Da ogni canale float di Control Rig leggo frame (`frame_number.value`), valore (`get_value()`), modo di interpolazione (`get_interpolation_mode()`), modo di tangente (`get_tangent_mode()`), pendenze (`get_arrive_tangent()`, `get_leave_tangent()`) e pesi (`get_arrive_tangent_weight()`, `get_leave_tangent_weight()`). Confrontando i dizionari di due sequenze (`LS_<mossa>_Owner` o `_Agy` contro `LS_<mossa>_Build`) individuo chiavi aggiunte/tolte, scarti di valore oltre soglia e variazioni di tangente/peso per singolo controllo.
* **Cosa faccio già di simile oggi:**
  1. In [ue_rig.py](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/Tools/AnimPipeline/animpipe/ue_rig.py#L206-L218) (`_read_manual`), [ue_tangent_test.py](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/Tools/AnimPipeline/ue_tangent_test.py#L36-L40) e [ue_dump_user_keys.py](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/Tools/AnimPipeline/ue_dump_user_keys.py#L15-L23) leggo già chiavi sparse, tangenti e pesi dalle sezioni di Sequencer in Unreal.
  2. In [cascadeur.py](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/Tools/AnimPipeline/animpipe/cascadeur.py#L245-L292) (`casc-import` e `casc-diff`) faccio già il rientro per differenza nella scheda contro un readback di riferimento (`--ref`): ricalcolo solo le parti che cambiano oltre `0.5 cm / 1°`, blocco se una chiave `owner`/`approved` è stata toccata e lascio il resto della scheda identico byte per byte.
* *Esempio concreto:* Con [ue_dump_user_keys.py](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/Tools/AnimPipeline/ue_dump_user_keys.py) e [user_tweaked_keys.json](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/user_tweaked_keys.json) ho estratto da `LS_GanonFTiltV31_Build_VarD` le chiavi modificate dall'utente su `upperarm_r_fk_ctrl`, `lowerarm_r_fk_ctrl` e `hand_r_fk_ctrl` ai frame `20, 23, 25, 27, 32, 37`, e con `ue_tangent_test.py` ho verificato il readback esatto di `leave_tangent_weight` (`0.6`) e `arrive_tangent` (`0.03`) su una LevelSequence.

**22. "Uniforma": l'owner ha cambiato una chiave e una curva; ti si chiede di adattare solo le chiavi vicine non sue. Come lo faresti?**
* **Procedura in 4 passi (su `LS_<mossa>_Agy` o sulla scheda dopo `ue-import`):**
  1. **Lettura del delta owner e dei vincoli:** Leggo sui canali interessati la chiave modificata dall'owner al frame $f_{\text{owner}}$: ricavo lo spostamento $\Delta V = V_{\text{owner}} - V_{\text{old}}$ e le nuove tangenti/pesi ($T_{\text{in}}, T_{\text{out}}$). Marco $f_{\text{owner}}$ e tutte le altre chiavi owner come intoccabili, e individuo le chiavi non-owner comprese tra $f_{\text{owner}}$ e le chiavi owner di confine ($f_{\text{prev\_anchor}}$ prima, $f_{\text{next\_anchor}}$ dopo).
  2. **Adattamento dei valori delle chiavi vicine non-owner:**
     - Se la chiave vicina $f_k$ faceva parte di una tenuta o di un overshoot relativo a $f_{\text{owner}}$ (es. fine hitstop o apice di follow-through), traslo $f_k$ dello stesso $\Delta V$ (mantenendo l'offset relativo originale $V(f_k) - V(f_{\text{owner}})$).
     - Se $f_k$ è un breakdown intermedio di rientro verso $f_{\text{next\_anchor}}$, applico un decadimento proporzionale nel tempo: $\Delta V_k = \Delta V \cdot \frac{f_{\text{next\_anchor}} - f_k}{f_{\text{next\_anchor}} - f_{\text{owner}}}$, così da evitare gradini o inversioni di moto.
  3. **Raccordo delle tangenti sulle chiavi vicine:** Senza toccare $T_{\text{out}}$ su $f_{\text{owner}}$, regolo `arrive_tangent` (e peso) sulla prima chiave non-owner successiva $f_k$ (e `leave_tangent` su quella precedente) in modo che la pendenza abbia lo stesso segno della rampa tra $f_{\text{owner}}$ e $f_k$ e non generi un rimbalzo (overshoot spline) contro la curva scelta dall'owner.
  4. **Controllo e griglia:** Eseguo `build` + `check` (verificando che `hitstop_snap`, `clearance` e monotonia del segmento passino) e produco la griglia prima/dopo sui frame del tratto raccordato.
* *Esempio concreto:* In `overlord_ftilt_v31_varD` ([overlord_ftilt_v31_varD.pre_user_harmonize.backup.json](file:///C:/Users/jacop/Documents/Unreal%20Projects/GASP_Giustino2_ue59/Tools/AnimPipeline/cards/overlord/ftilt/overlord_ftilt_v31_varD.pre_user_harmonize.backup.json) -> `varD`), se l'owner a `f10` (chiave owner attiva) apre `upperarm_r_fk_ctrl.Rotation.Y` di `-15°` e inclina la tangente in uscita per dare più frustata, lascio `f10` e `f27` (recovery owner) intatti, sposto le chiavi non-owner di tenuta/rilascio vicine (`f11`, `f14` di `-15°`, e `f20`, `f23` con decadimento a `-8°` e `-4°`) e allineo la `arrive_tangent` di `f11` alla pendenza d'uscita di `f10` per non creare un rimbalzo a `f12`.

---
[agy-bridge] model: agy default | session: 2f694cf7-ca2a-4177-a2ef-f503a3ea42b1 (use follow_up to continue)
