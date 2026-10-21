# Domande aperte per le milestone M4–M10 e per la modularità

Domande che un ingegnere del software farebbe prima di implementare ogni milestone, sul **comportamento desiderato**
(feeling di gioco, regole, casi limite, priorità). Ogni domanda indica perché conta e, quando c'è, l'ipotesi che la
specifica ha già adottato: se l'ipotesi va bene, basta rispondere "ok".

Rispondi pure in ordine sparso, anche solo con il numero della sezione e della domanda (es. "M6.3: tasto dedicato").
Le risposte vanno poi riportate nelle specifiche e in combat.md.

| Sezione | Specifica |
|---|---|
| Modularità e tag | [modularity-and-tags.md](modularity-and-tags.md) |
| M4 Colpi | [combat-m4-hits.md](combat-m4-hits.md) |
| M5 Editor | [combat-m5-editor.md](combat-m5-editor.md) |
| M6 Difesa e link | [combat-m6-defence-links.md](combat-m6-defence-links.md) |
| M7 Piggyback | [combat-m7-piggyback.md](combat-m7-piggyback.md) |
| M8 Minion | [combat-m8-minions.md](combat-m8-minions.md) |
| M9 Adattatore GAS standard | [combat-m9-stock-gas.md](combat-m9-stock-gas.md) |
| M10 Oggetti tenuti e lanciati | [combat-m10-held-objects.md](combat-m10-held-objects.md) |


---

## Domande su modularità e tag (per il proprietario)

Documento di riferimento: docs/architecture/modularity-and-tags.md.

1. Vuoi adottare subito i Game Feature plugin e le Experience, oppure più avanti (dopo M7)?
   Perché conta: decide se la pawn data, gli ability set e l'experience entrano ora nel lavoro di M3 o restano una migrazione successiva.
   Ipotesi nella specifica: preparare solo i punti di estensione ora (sezione 8.5), adottare Game Feature e Experience dopo il primo spike (passo S-K).

2. Quali funzionalità devono poter essere attivate o disattivate per modalità (piggyback, minion, oggetti tenuti, difesa, grab)?
   Perché conta: ciò che cambia per modalità va in Game Feature; ciò che è uguale ovunque resta plugin sempre attivo.
   Ipotesi nella specifica: difesa e link come plugin opzionali, piggyback e held come feature per modalità, minion per modalità.

3. Una feature che modifica la simulazione può essere attivata solo al caricamento della mappa (mai a partita in corso)?
   Perché conta: attivare a metà partita cambia lo stato di sync e rompe il rollback.
   Ipotesi nella specifica: sì, solo al caricamento dell'experience, prima del possesso dei pawn.

4. Altre persone (modder, collaboratori) estenderanno i plugin?
   Perché conta: se sì servono API pubbliche stabili, versioning e documentazione più rigorosa; se no si può rifattorizzare liberamente.
   Ipotesi nella specifica: no, sviluppatore singolo; API pubbliche comunque minime.

5. I plugin dovranno essere riusati in altri progetti o pubblicati (Marketplace/Fab)?
   Perché conta: richiede zero dipendenze dal modulo di progetto, licenze chiare, e il fork GASNPP diventa un vincolo (non redistribuibile in modo semplice).
   Ipotesi nella specifica: riuso in altri tuoi progetti sì, vendita no.

6. Quanto deve essere rigido il controllo automatico (script sui Build.cs, lint dei tag)?
   Perché conta: troppo rigido rallenta un solo sviluppatore, troppo lasco lascia rientrare le dipendenze sbagliate.
   Ipotesi nella specifica: prima solo avvisi, poi una regola alla volta diventa bloccante (passi S-B, S-C, S-H, S-L).

7. In quale lingua e con quale stile vanno i nomi dei tag?
   Perché conta: i tag sono nei dati e nei redirect, cambiarli dopo costa.
   Ipotesi nella specifica: inglese, PascalCase, radice per dominio (Combat., Input., Command.), mai il nome del plugin.

8. Per gli eventi non di simulazione (UI, audio, missioni) usiamo il Gameplay Message Router di Lyra o un sottosistema nostro?
   Perché conta: il plugin del motore potrebbe non essere disponibile in 5.8 come in Lyra; uno nostro è piccolo ma da mantenere.
   Ipotesi nella specifica: da decidere dopo verifica; mai usare messaggi dentro la simulazione.

9. Vuoi rinominare i tag movimento del progetto (State.Movement.*, Ability.Movement.*) o lasciarli?
   Perché conta: la radice State. è generica e confligge con Combat.State.*; rinominare richiede redirect e tocca i Blueprint.
   Ipotesi nella specifica: Ability.* resta, State.Movement.* da decidere con te (bassa priorità).

10. Il wall movement e i salti devono diventare un plugin (GSTNMovement) riusabile fuori da questo gioco?
    Perché conta: oggi sta nel modulo di progetto; estrarlo è il passo più invasivo (S-I).
    Ipotesi nella specifica: sì, ma in una pausa tra due milestone.

11. Il modulo di progetto deve smettere di dipendere pubblicamente da GSTNCombatNP e GSTNCombatCore?
    Perché conta: è il principale difetto di modularità; correggerlo può generare errori di compilazione da sistemare.
    Ipotesi nella specifica: sì, passo S-H, dipendenze private.

12. Quando abilitiamo FastReplication dei tag (oggi disattivata)?
    Perché conta: risparmia banda ma richiede che server e client abbiano lo stesso insieme di plugin e tag.
    Ipotesi nella specifica: più tardi, quando l'insieme dei plugin è stabile.

13. Quali tag possono creare i designer da soli (comandi, bottoni, stati, cue, tipi di minion, tipi di oggetto) senza C++?
    Perché conta: definisce cosa va in file ini per plugin e cosa resta nativo; vale come eccezione di CLAUDE.md per gli ini dei plugin (comandi di piggyback, tipi di minion, tipi di oggetto tenuto) e sostituisce le domande analoghe per singola milestone.
    Ipotesi nella specifica: comandi, bottoni, identità di kit/arma/super, status, cue, `Minion.Type.*`, `Held.Type.*` in ini del plugin proprietario; tutto ciò che il codice legge è nativo, dichiarato dal plugin proprietario (anche i tag standard di difesa e link).

14. Vuoi una mappa di test minima e un test automatico per ogni plugin, e una build CI con ciascun plugin opzionale disattivato?
    Perché conta: è l'unico modo di dimostrare che "ogni plugin funziona da solo"; ha un costo di manutenzione.
    Ipotesi nella specifica: sì per i plugin opzionali e per i moduli NP; i moduli Core con soli test Automation.

15. Il punto di estensione del combat (frammenti di stato, hook di tick, registro dell'inbox) va costruito una volta sola prima di M6, anche se ritarda M3/M4?
    Perché conta: cambia il formato di rete dello stato; farlo dopo costringe a rifare serializzazione e test.
    Ipotesi nella specifica: sì, con test propri, e M3 prepara già i punti (sezione 8.5).

16. La documentazione per plugin (README) va tenuta in inglese come il resto, e con quale livello di dettaglio?
    Perché conta: ogni plugin deve avere docs leggibili da una persona; lo standard fissa il lavoro per ogni nuovo plugin.
    Ipotesi nella specifica: inglese, README breve + documento di architettura in docs/architecture/.

---

## M4 — Hits: domande al proprietario del progetto

Riferimenti: `combat-m4-hits.md` (qui "M4 §n") e `combat.md` (qui "combat §n").

### A. Modello di danno e knockback

**1. Il gioco usa danno a percentuale (stile Smash), a HP, o entrambi (es. percentuale per i giocatori, HP per i nemici PvE)?**
Perché conta: decide `DamageMode`, il modello di knockback di default e la lettura di UI/bilanciamento; le due modalità danno un "feel" opposto.
Ipotesi nella specifica: `DamageMode` è un'opzione per regole di gioco (M4 §3 Combat rules); default `PercentScaling` Smash-like, `Fixed` per i giochi a HP (M4 §3 Knockback model; combat §1).

**2. Nel PvE un nemico comune deve poter essere lanciato via come un giocatore, o solo subire stagger/hitstun senza launch?**
Perché conta: con 40+ nemici il launch pieno costa in rollback, in leggibilità visiva e in controllo della folla.
Ipotesi nella specifica: stessa pipeline per giocatori e IA (combat §1), nessuna distinzione per tipo di nemico; solo `KnockbackResistance`/`Poise` negli attributi (M4 §4).

**3. Boss e nemici pesanti: armatura, super-armor o resistenza al knockback sono previste in M4, e con quale soglia?**
Perché conta: lo stato `Armor` esiste ma servono valori e regole (soglia in danno o in knockback?) per non rendere i boss inutilizzabili o banali.
Ipotesi nella specifica: `Armor` = niente knockback/hitstun sotto `ArmorThreshold`, il danno si applica (M4 §3 Hurtboxes); il rapporto tra `Poise` e `Armor` non è definito.

**4. Esiste un limite superiore al knockback/launch (o un KO per percentuale/blast zone), e cosa succede quando un personaggio "muore" (KO)?**
Perché conta: M4 emette l'evento `KO` (revisione 2026-10-18) ma non dice quando scatta né cosa fa il gioco dopo (respawn, spettatore, game over).
Ipotesi nella specifica: nessuna; si cita solo l'evento `Combat.Event.KO`.

### B. Hitstop e sensazione dei colpi

**5. Quanto deve durare e "sentirsi" l'hitstop: ci sono valori target in frame per un jab, un colpo medio e uno forte? Deve essere uguale per attaccante e vittima?**
Perché conta: sono le costanti `HitstopPerDamage`, `HitstopBase`, `MaxHitstop` e i moltiplicatori; sono il cuore del game feel.
Ipotesi nella specifica: `floor(Damage × HitstopPerDamage + HitstopBase)` con tetto `MaxHitstop`, moltiplicatori separati per attaccante e vittima (M4 §3); nessun valore numerico.

**6. Durante l'hitstop la vittima può fare SDI: in M4 è davvero in scope, o si rimanda a M6 insieme a tech/shield?**
Perché conta: il testo si contraddice (SDI "oltre lo spostamento di base" è fuori scope in M4 §1, ma M4 §7 lo descrive con `MaxSDIPerHit`); cambia lavoro e test.
Ipotesi nella specifica: SDI base (spostamento sul piano, N volte per colpo) in M4 §7; tuning avanzato in M6 (M4 §1).

**7. Durante il proprio hitstop, il giocatore deve poter bufferizzare input e quali (attacco, salto, dodge)? E la vittima in hitstun può fare qualcosa (es. DI, ma anche dodge/uscite)?**
Perché conta: definisce "reattività" e potenziale di combo infinite/escape; influisce su cosa registra il buffer.
Ipotesi nella specifica: i press durante l'hitstop sono registrati, l'età del buffer esclude i frame di hitstop (M4 §8); nessuna regola su azioni in hitstun oltre DI/SDI.

**8. Il DI deve essere solo sul piano di movimento (come deciso), o anche in verticale/3D per colpi che lanciano in aria?**
Perché conta: in 3D il DI solo orizzontale può limitare molto la sopravvivenza in aria rispetto al 2D.
Ipotesi nella specifica: DI solo orizzontale, verticale invariata (M4 §3 DI; "as decided").

**9. Angoli di lancio: per un attacco 3D la direzione "giusta" è la facciata dell'attaccante, il vettore hitbox→vittima o la velocità della hitbox, e può cambiare per colpo?**
Perché conta: determina la sensazione di "controllo" del lancio e la combinabilità dei colpi; `Sakurai3D` e `AutoLink` hanno comportamenti diversi.
Ipotesi nella specifica: `YawSource` per hitbox, `SpecialAngle` opzionale; non c'è un default consigliato per i colpi comuni (M4 §3 Hit data).

### C. Schivate (dodge)

**10. Regole delle schivate: durata dell'intangibilità, frame di vulnerabilità finale (recovery), costo (cooldown, stamina, meter) e se si può schivare durante hitstun?**
Perché conta: roll, spot dodge e air dodge sono in scope (M4 §1) ma la specifica definisce solo il meccanismo `HurtboxModifier`, non le regole.
Ipotesi nella specifica: nessuna sul costo/anti-spam; l'intangibilità viene dai tracciati `Intangible` della timeline.

**11. L'air dodge può essere usata più volte in aria, e lascia il personaggio in stato "helpless"?**
Perché conta: incide molto sul recupero e sul gioco in verticale 3D, e va allineato ai salti aerei del M3.
Ipotesi nella specifica: nessuna.

### D. PvE, squadre e fuoco amico

**12. Nel co-op PvE il fuoco amico è attivo di default? Se sì, quanto è "serio" (danno pieno) rispetto a una versione leggera (niente hitstun, danno ridotto)?**
Perché conta: scelta di design più visibile del progetto; determina i valori delle varianti per relazione `Ally`.
Ipotesi nella specifica: `bFriendlyFire` per modalità; esempio di variante alleati con forma dimezzata e senza hitstun (M4 §3, §10; combat §12.3).

**13. Gli attacchi di supporto (cure, buff, ecc.) su alleati usano la stessa pipeline di hit (e quindi hitbox/hitstop), o sono esclusi dalla pipeline di danno?**
Perché conta: combat §1 cita "heals on allies"; M4 non dice se una cura è un `Hit` con hitstop o un effetto separato.
Ipotesi nella specifica: nessuna; la pipeline ha solo `TeamFilter` e varianti per relazione (M4 §3).

**14. I nemici si colpiscono tra loro (fuoco amico IA, area/boss che colpisce i minion)? E i colpi dei nemici che toccano oggetti/ambiente?**
Perché conta: con 40+ nemici un hit tra IA moltiplica costo e caos; serve per la tabella `HittableRelationships`.
Ipotesi nella specifica: il resolver restituisce Self/Ally/Enemy/Neutral; `NoTeam` = Neutral (M4 §6.3); la politica sui Neutral non è decisa.

### E. Lag compensation ed equità

**15. In PvE co-op la lag compensation (`bUseLagCompensation`, `MaxRewindMs`) deve essere attiva di default? Quale massimo di rewind è accettabile per i colpi contro nemici IA?**
Perché conta: un rewind alto fa colpire "dietro l'angolo" per chi subisce, ma un rewind nullo fa mancare colpi che sul client sembravano ok.
Ipotesi nella specifica: opzione per modalità; le IA non rewindano mai, ma il bersaglio sì (M4 §9); nessun valore per `MaxRewindMs`.

**16. Quando un giocatore subisce un colpo, accettiamo che arrivi come correzione (rollback ~RTT/16 frame, "ti colpisce con ritardo"), o si vuole che M4 anticipi il colpo sul client locale (forward prediction)?**
Perché conta: è la decisione di feel più rischiosa per il giocatore colpito (movimento che scatta indietro); la specifica rimanda la scelta.
Ipotesi nella specifica: forward prediction spenta in M4, ogni hit sul giocatore locale è una correzione (M4 §13; combat §12.7).

**17. Se il colpo predetto dall'attaccante viene poi smentito dal server (il bersaglio era intangibile/aveva schivato), cosa deve vedere il giocatore: effetti annullati subito, o lasciati finire?**
Perché conta: flash e hitstop visivi già mostrati e poi annullati possono sembrare un bug; definisce la tolleranza di presentazione.
Ipotesi nella specifica: la presentazione locale si ferma in anticipo quando la voce sparisce da `HitTargets` (M4 §9); non si dice cosa accade all'hitstop già predetto sull'attaccante.

### F. Presentazione e scambi

**18. Quando due attacchi si colpiscono nello stesso frame (trade), vince il più forte (priorità/clank) o entrambi subiscono il colpo?**
Perché conta: M4 §1 dice che i trade funzionano ma "clank/priority" è nei dati e `bClank` nel contesto, senza uno step dedicato in M4.
Ipotesi nella specifica: nessuno step di clank in M4; trade semplice con entrambi che colpiscono (M4 §7, §10 `scenario_hit_trade`).

**19. Cue e feedback dei colpi (flash, scintille, suono, camera shake, numeri di danno): quali sono obbligatori in M4 e quali possono rimandare?**
Perché conta: la presentazione sulla vittima interpolata è dichiarata essenziale in PvE, ma i contenuti minimi non sono elencati.
Ipotesi nella specifica: flash sul materiale, cue locale, pausa mesh (M4 §9); nessun feedback sul giocatore colpito (camera, controller).

### Lacune nella specifica

- Il testo non dice cosa succede a una hitbox contro più bersagli nello stesso frame oltre il "una volta per gruppo": manca un limite di bersagli per colpo (importante con 40+ nemici) e un ordine di risoluzione deterministico per l'attaccante.
- `HitTargets` ha 8 voci: non è definito il comportamento quando si superano (colpo multiplo su folle, sostituzione, overflow).
- Contraddizione sull'SDI: fuori scope in M4 §1 ma descritto in M4 §7.
- Nessun valore di partenza per le costanti del modello di knockback, hitstop, DI e SDI (peso, `HitstunFactor`, `TumbleThreshold`, `MaxHitstop`): l'implementatore deve inventarli.
- Il KO è un evento ma non c'è definizione della condizione (HP a 0? blast zone?), né di cosa fare dopo.
- Cure/buff agli alleati e `HitType` diversi da Normal (Grab/Projectile/Unblockable) sono nei dati ma non hanno comportamento in M4; non è chiaro se vadano ignorati o rifiutati dal validatore.
- Regole per colpire bersagli con stati speciali (già in hitstun/tumble, in hitstop, già a terra, sul muro, in dodge) e relativa gestione di combo/hitstun decay non sono descritte.
- Non è specificato cosa succede se l'attaccante muore, viene colpito o viene cancellato nello stesso frame in cui colpisce (hit già consegnato vs. annullato).

---

## M5 — Strumenti editor: domande al responsabile del progetto

### Workflow del designer

1. Chi userà l'editor nella pratica: solo tu, o anche altri (artisti, designer esterni)? Con quale esperienza di Unreal?
   Perché conta: decide quanto investire in guide, messaggi d'errore e protezioni contro azioni distruttive.
   Ipotesi nella specifica: un designer tecnico che conosce Unreal ma non il codice.
2. Le mosse le vuoi creare più spesso come asset condivisi (`UGSTN_CombatTimeline`) o inline nel Blueprint dell'abilità?
   Perché conta: l'editing inline in Blueprint è la parte più fragile (CDO, recompile); se serve poco, può essere sola lettura.
   Ipotesi nella specifica: asset consigliati; inline supportato con lo stesso editor, con ripiego "estrai in asset".
3. Quanto è importante vedere i frame data in stile picchiaduro (startup/active/recovery) rispetto a un editing libero?
   Perché conta: determina se le fasi derivate e il "first hit frame" sono centrali o solo informativi.
   Ipotesi nella specifica: sempre visibili, solo lettura, convenzione "primo frame attivo = indice + 1".
4. Vuoi poter copiare/incollare tracce o mosse intere tra timeline diverse, o basta copiare le chiavi?
   Perché conta: il copia/incolla di tracce intere cambia il modello dati dell'appunti e i controlli di compatibilità.
   Ipotesi nella specifica: solo chiavi (via testo), niente copia di tracce intere nella prima versione.

### Anteprima e hitbox

5. L'anteprima deve mostrare solo la posa (montage) o anche il movimento del personaggio (root motion, traccia Movement)?
   Perché conta: simulare il movimento nell'editor è approssimativo rispetto a Mover e può ingannare.
   Ipotesi nella specifica: solo una polilinea approssimata, attivabile; la verità resta il gioco.
6. Preferisci modificare le hitbox soprattutto con gizmo in viewport o numericamente? Serve l'auto-key (crea la chiave
   spostando il gizmo)?
   Perché conta: cambia le priorità tra gizmo (rischio S4) e pannello dettagli, e il comportamento di default.
   Ipotesi nella specifica: gizmo + auto-key attivo di default, valori numerici sempre disponibili.
7. L'anteprima deve mostrare un confronto con una vittima di prova (knockback, hitstun calcolati dal modello)?
   Perché conta: richiede un piccolo "banco di prova" con peso e percentuale, utile ma non gratuito.
   Ipotesi nella specifica: solo una riga di calcolo del knockback nel pannello dei dettagli, nessun manichino vittima.

### Baking dei socket

8. Il bake deve essere sempre manuale, automatico al salvataggio, o automatico quando cambia il montage?
   Perché conta: il bake automatico evita dati vecchi ma può rallentare il salvataggio e sporcare i commit.
   Ipotesi nella specifica: manuale, opzione "bake al salvataggio" spenta di default; dati vecchi bloccano il cook come errore.
9. Cosa deve succedere se il montage cambia lunghezza dopo che hai disegnato la mossa: avviso e basta, o adattare
   automaticamente `TotalFrames`?
   Perché conta: un adattamento automatico può spostare finestre già tarate senza che te ne accorga.
   Ipotesi nella specifica: mai automatico; avviso con pulsante "Adatta ai frame del montage" annullabile.

### Hurtbox

10. Le hurtbox generate dal Physics Asset devono essere un punto di partenza da ritoccare a mano, o rigenerabili sempre?
    Perché conta: decide se la rigenerazione deve preservare le modifiche manuali (più complessa).
    Ipotesi nella specifica: rigenerazione che conserva le modifiche manuali per nome; "sostituisci tutto" esplicito.
11. Tutti i personaggi seguono una convenzione di nomi ossa sinistra/destra e un asse "avanti" comuni?
    Perché conta: lo specchio L/R funziona solo con regole di nomi affidabili.
    Ipotesi nella specifica: coppie configurabili (`_l/_r`, `Left/Right`, `.L/.R`), niente deduzioni; ossa senza coppia segnalate.

### Validazione

12. Quali errori devono impedire il salvataggio o il cook e quali sono solo avvisi (es. sovrapposizione di hitbox, montage di
    lunghezza diversa)?
    Perché conta: troppi errori bloccanti frustrano il lavoro iterativo, troppo pochi lasciano passare dati rotti.
    Ipotesi nella specifica: errore = intervalli fuori range, bake vecchio, slot ambigui; il resto avviso.
13. La validazione completa dei progetti (tutti i kit, armi, preset) va eseguita in CI o solo a mano nell'editor?
    Perché conta: la CI richiede un comando headless e tempi di esecuzione accettabili.
    Ipotesi nella specifica: pannello a mano e comando `-run=DataValidation` disponibile, CI facoltativa.

### Debug

14. Quali informazioni di debug in PIE contano di più: input display, stato del combattimento, ultime hit, o la traccia per frame?
    Perché conta: ordina il lavoro (M5.7/M5.8) e decide se vale il costo del Rewind Debugger.
    Ipotesi nella specifica: input display e Gameplay Debugger prima; il Rewind Debugger solo se lo spike S3 lo permette.
15. L'input display serve anche ai giocatori (modalità allenamento, build di sviluppo condivise) o solo a te in editor?
    Perché conta: un uso in gioco richiede aspetto curato e modalità di attivazione diverse.
    Ipotesi nella specifica: strumento di sviluppo, overlay con CVar, non incluso nella Shipping.

### Struttura in plugin e tag

16. Va bene che editor e debug siano due plugin separati e disattivabili (l'editor lavora anche senza il plugin debug)?
    Perché conta: ogni separazione costa dipendenze dinamiche e test in più, ma ti dà moduli davvero indipendenti.
    Ipotesi nella specifica: due plugin (`GSTNCombatEditor`, `GSTNCombatDebug`), nessuno dipende da loro; editor -> debug solo in modo opzionale.
17. Il plugin editor deve funzionare anche se in futuro usi solo `GSTNInput` o un adattatore GAS standard (M9)?
    Perché conta: stabilisce se l'editor dipende da `GSTNCombatNP` o solo da Core/GAS.
    Ipotesi nella specifica: dipende da NP (per il tick rate e gli accessori); da rivedere con M9.
18. I tag delle hurtbox (Head, Torso, Arm...) sono sufficienti, o hai già una tassonomia delle zone del corpo per danni o reazioni?
    Perché conta: i tag nativi sono costosi da cambiare dopo che gli asset li usano.
    Ipotesi nella specifica: sei tag nativi in Core, estendibili da ini di progetto; il lato (sinistra/destra) non è un tag.
19. I tag per gli eventi e i test dell'editor vanno nei plugin o restano solo nel contenuto di test del progetto?
    Perché conta: tag nei plugin vincolano chi li riusa; nell'ini del progetto restano facili da cambiare.
    Ipotesi nella specifica: nel contenuto di test del progetto (ini), non nei plugin.

---

## M6 — Difesa e link: domande al proprietario del progetto

Specifica di riferimento: docs/architecture/combat-m6-defence-links.md (le ipotesi indicate sono quelle scritte lì).

### Scudo e parry

1. Lo scudo deve essere limitato (barra che si consuma e si rompe) o sempre disponibile in PvE co-op?
   Perché conta: cambia il rischio nei combattimenti contro boss e il bilanciamento di tutte le mosse nemiche.
   Ipotesi nella specifica: limitato, con barra `ShieldHealth`, rigenerazione ritardata e rottura con stordimento lungo.

2. Lo scudo è direzionale (arco davanti al personaggio) o a bolla a 360°?
   Perché conta: l'arco rende rilevante il posizionamento, la bolla è più semplice da leggere per il giocatore.
   Ipotesi nella specifica: arco di 140° sul piano di movimento, direzione fissata alla pressione (nessuna rotazione).

3. Il parry è una finestra nei primi frame della pressione dello scudo o ha un tasto dedicato?
   Perché conta: decide quanto è facile fare parry per sbaglio e quanto pesa la ricompensa.
   Ipotesi nella specifica: 5 frame iniziali dello scudo, con blocco se si ripreme subito dopo aver rilasciato.

4. Cosa guadagna chi fa parry e cosa perde l'attaccante (stordimento, vantaggio, barra EX, riflessione proiettili)?
   Perché conta: stabilisce se il parry è una scelta rischiosa o la risposta standard.
   Ipotesi nella specifica: difensore libero prima, attaccante stordito per frame configurabili per classe di colpo, guadagno di barra; nessuna riflessione.

5. Boss e attacchi speciali possono essere non parabili o non bloccabili? Le super sono bloccabili?
   Perché conta: evita che un'abilità cardine del boss sia annullata da un parry o da uno scudo.
   Ipotesi nella specifica: flag per singolo colpo; i finalizzatori delle super sono non bloccabili, gli altri colpi sì con grande danno allo scudo.

6. Lo scudo è disponibile in aria e su parete?
   Perché conta: in un platform fighter 3D la difesa aerea cambia molto il gioco verticale.
   Ipotesi nella specifica: solo a terra; in aria e su parete si usa lo schivata.

7. In PvE i colpi dei nemici comuni consumano lo scudo come quelli dei giocatori?
   Perché conta: i minion in massa potrebbero rompere lo scudo in un attimo.
   Ipotesi nella specifica: moltiplicatore per relazione/tag configurabile nelle regole, valori da decidere.

### Tech, atterraggio, DI e SDI

8. Il tech deve esistere anche in PvE e usare il tasto dello scudo?
   Perché conta: aggiunge una difesa a costo zero ma rende gli scontri con i nemici meno punitivi.
   Ipotesi nella specifica: sì, tasto scudo, solo dopo un lancio con tumble; finestra 20 frame, blocco anti-mash 40 frame.

9. Cosa succede quando si manca il tech: knockdown con rialzo automatico, rialzo con scelta (attacco, rotolata) o niente?
   Perché conta: definisce la punizione dopo un lancio e quanto tempo è in balia dei nemici.
   Ipotesi nella specifica: knockdown breve con un solo rialzo neutro; altri rialzi rimandati.

10. DI e SDI devono valere uguale per giocatori, nemici e boss, e in tutte le modalità?
   Perché conta: la possibilità di sfuggire ai combo è parte del feeling e della difficoltà.
   Ipotesi nella specifica: curve e scale per tag della vittima nell'asset regole di modalità; i boss con scala 0.

### Prese e lanci

11. Chi può essere afferrato e chi può afferrare: giocatori tra loro, nemici verso i giocatori, giocatori verso i nemici, boss?
   Perché conta: una presa subita da un boss è un'interazione completamente diversa da una tra alleati.
   Ipotesi nella specifica: i boss hanno `Combat.Immunity.Grab`; i nemici normali possono afferrare i giocatori con la stessa pipeline.

12. Un alleato può afferrare un compagno (fuoco amico) o salvarlo da una presa nemica colpendo il nemico?
   Perché conta: è la parte cooperativa più visibile del sistema.
   Ipotesi nella specifica: nessuna presa tra alleati; un colpo dell'alleato sul nemico non libera da solo (da decidere).

13. Come si esce da una presa: solo mash, e quanto dura al massimo? Dipende dalla percentuale di danno?
   Perché conta: una presa lunga in PvE co-op è frustrante se il giocatore non può reagire.
   Ipotesi nella specifica: mash con durata crescente col danno, minimo e massimo; l'uscita avviene tramite il server (circa 2 frame di ritardo).

14. Quanti lanci ha un afferratore e con quali input (avanti, indietro, su, giù)? Si può afferrare in aria?
   Perché conta: determina quanti slot e animazioni servono per ogni kit.
   Ipotesi nella specifica: 4 lanci e pummel, grab con Attacco+Scudo a terra, nessuna presa aerea.

15. Dopo una presa o un lancio c'è immunità alla presa? Per quanti frame e vale anche per il lanciatore?
   Perché conta: senza immunità le prese si ripetono all'infinito, con troppa non si può punire.
   Ipotesi nella specifica: immunità per frame configurabili dopo rilascio e dopo lancio, più breve ricarica per l'afferratore.

### Link, rete e sensazione di gioco

16. Come deve sentirsi una presa per chi la subisce sul proprio client (circa 1 RTT di ritardo prima dell'effetto) e per chi la fa?
   Perché conta: la specifica non predice il cambio di ruolo del giocatore che afferra, solo hitstop e feedback.
   Ipotesi nella specifica: nessuna previsione del ruolo per le prese (solo hitstop e presentazione); la telecamera di chi è afferrato segue la posa derivata.

17. Se lo spike del link parcheggiato non passa le soglie (errore di presentazione p95 10 cm / 25 cm), cosa preferisci: accettare, ritoccare le soglie o patchare il fork per la predizione in avanti?
   Perché conta: è la decisione che può far cambiare architettura al milestone.
   Ipotesi nella specifica: soglie come scritte; se falliscono il lavoro si ferma e decidi tu.

18. Quali eventi rompono un link oltre a mash e rilascio: colpo all'afferratore, morte, distanza, stato dell'afferratore?
   Perché conta: definisce quanto le prese sono "sicure" e quanto un compagno può interromperle.
   Ipotesi nella specifica: i colpi del partner non rompono; stordimento, tumble o morte dell'afferratore sì; distanza massima configurabile.

### Plugin, modularità e tag

19. Va bene dividere in due plugin opzionali (`GSTNDefence` e `GSTNLink`), con `GSTNLink` proprietario di tutto il sistema di link e del sottoalbero `Combat.Link.*`? (La superficie di estensione di `GSTNCombat` è la domanda generale 15.)
   Perché conta: permette di usare ogni plugin da solo e di far dipendere M7 (piggyback) e M10 (oggetti tenuti) solo da Link, senza campi di link in `GSTNCombat`.
   Ipotesi nella specifica: sì, con Link separato; Piggyback (M7) e Held (M10) come plugin ulteriori che dipendono da Link.

20. Va bene che M6 includa già i requisiti dei plugin successivi (§7.0: riferimento a entità `FGSTN_EntityRef` da 4 byte al posto dell'id NP, due canali di link corpo/mano, registro aperto dei tipi di link), anche se M6 usa solo il canale corpo e solo partner combattenti? *Proposal*
   Perché conta: M10 (oggetti e minion come partner, un rider che tiene un oggetto) non è realizzabile senza questi tre punti; aggiungerli dopo cambia il formato di rete dello stato di link.
   Ipotesi nella specifica: sì, costo circa 7 byte per entità in più nel caso peggiore e nessun costo da inattivo.

21. I tag dei tipi di link e dei ruoli (`Combat.Link.Type.*`, `Combat.Link.Role.*`) vanno dichiarati dal plugin che possiede il tipo (Grab in `GSTNLink`, Mount in `GSTNPiggyback`, Held in `GSTNHeld`) o tutti in `GSTNLink`? *Proposal*
   Perché conta: nel primo caso un dato che cita `Combat.Link.Role.Holder` non si carica senza `GSTNHeld` (lo segnala `IsDataValid`); nel secondo `GSTNLink` conosce i nomi dei plugin successivi.
   Ipotesi nella specifica: dichiarati dal plugin proprietario del tipo; `GSTNLink` dichiara solo quelli generici.

---

## M7 — Piggyback e mosse in coppia: domande al project owner

### Controlli e ruoli

1. Per montare/smontare/lasciare cadere va bene un solo tasto "Interact" con significato deciso dal ruolo (montare, smontare, drop)?
   Perché conta: decide i binding, le collisioni di input e quanti tasti servono sul pad.
   Ipotesi nella specifica: sì, un tasto; Drop = Interact + accovacciarsi; il cooldown di 30 frame evita il re-mount immediato.
2. Il rider può smontare anche saltando, oppure il salto è riservato alla mossa in coppia "boost jump"?
   Perché conta: il salto è il gesto più naturale per scendere, ma qui è anche il trigger del boost.
   Ipotesi nella specifica: Jump del rider = boost jump (mossa in coppia); lo smontaggio semplice usa Interact.
3. Il carrier deve poter rifiutare o scrollarsi di dosso il rider (drop) in qualsiasi momento, anche durante una mossa?
   Perché conta: è il rimedio al "rider fastidioso" in co-op; influisce su cancel window e priorità.
   Ipotesi nella specifica: Drop è sempre disponibile quando non in hitstun; nessun veto del rider.
4. Quali mosse del carrier sono vietate mentre porta qualcuno (salti aerei, dash, schivate, super)?
   Perché conta: definisce la sensazione del "peso" e il bilanciamento del co-op.
   Ipotesi nella specifica: lista per kit (`DisallowedCommands`), vuota nel kit di test; nessuna restrizione globale.
5. Il rider può cambiare arma/loadout o usare le sue supers mentre è in sella?
   Perché conta: i comandi del rider sono solo quelli con binding di ruolo; supers e cambio arma vanno decisi esplicitamente.
   Ipotesi nella specifica: cambio arma consentito, supers non vincolate a meno che il kit le leghi al ruolo.

### Sensazione di gioco
6. Il montaggio deve sembrare immediato per il rider anche con 60 ms di lag, accettando un piccolo "scatto" se il server rifiuta?
   Perché conta: è il compromesso tra reattività e correttezza (rollback visibile in caso di rifiuto).
   Ipotesi nella specifica: sì, predizione ottimistica a 2 frame; il rifiuto costa un rollback con blend visivo.
7. Quanto deve penalizzare il peso del rider il carrier (velocità, salto, salti aerei)?
   Perché conta: è il valore di bilanciamento principale di `GE_CarryLoad`.
   Ipotesi nella specifica: penalità lineare sul rapporto peso rider / `MaxCarryWeight`, valori per kit; pari peso = rider alla soglia massima.
8. Dopo uno smontaggio il rider deve poter essere rimontato subito oppure deve esserci un cooldown?
   Perché conta: evita il ping-pong accidentale con lo stesso tasto, ma può irritare in mosse acrobatiche.
   Ipotesi nella specifica: cooldown di 30 frame (0,5 s a 60 Hz), configurabile per kit.

### Regole di combattimento
9. Quando il carrier viene colpito, il rider cade sempre, mai, o solo oltre una soglia di knockback?
    Perché conta: determina quanto il duo è fragile e quanto conviene proteggersi a vicenda.
    Ipotesi nella specifica: soglia di knockback (carrier = soglia tumble, rider = doppia); sotto soglia il rider subisce danno e un flinch ma non è lanciato.
10. Il carrier deve "coprire" il rider da certi angoli o con lo scudo (M6), oppure ogni colpo ai rider va a segno normalmente?
    Perché conta: cambia molto il valore tattico di portare un compagno e il lavoro sugli hurtbox.
    Ipotesi nella specifica: nessuna copertura di default (arco 0, scudo non copre), ma opzioni pronte nelle regole.
11. Come si dividono danno, medidor EX/super e crediti per KO e assist nelle mosse in coppia (lancio del rider, boost jump)?
    Perché conta: influenza punteggi, progressione, missioni e la percezione di equità tra i due giocatori.
    Ipotesi nella specifica: danno al proprietario dell'hitbox; medidor diviso 50/50 nel lancio, solo al proprietario nel boost; assist registrato come "Partner" negli eventi.
12. Gli attacchi del rider o del carrier devono poter colpire il partner (friendly fire tra i due collegati), anche quando il fuoco amico è attivo?
    Perché conta: evita colpi accidentali costanti a distanza zero.
    Ipotesi nella specifica: mai, tranne hitbox esplicitamente marcati `bAffectsLinkPartner`.

### Mosse in coppia
13. Chi decide la direzione e la forza del lancio del rider (stick del carrier, stick del rider, o entrambi)?
    Perché conta: è il cuore del feeling della mossa e della collaborazione.
    Ipotesi nella specifica: direzione dallo stick del carrier al rilascio, angolo e velocità fissi per mossa, con eredità parziale della velocità del carrier.
14. Il partner può rifiutare o interrompere la propria parte di una mossa in coppia (es. il rider non vuole essere lanciato)?
    Perché conta: toglie o dà controllo al giocatore passivo; impatta input lock e grief tra giocatori.
    Ipotesi nella specifica: parte del partner cooperativa (non rifiutabile), con comandi bloccati durante la sua mossa.
15. Un ritardo di circa metà RTT più interpolazione prima che il partner veda partire la propria parte è accettabile, o va prevista la predizione in avanti del partner?
    Perché conta: la predizione in avanti richiede modifiche al fork e costo CPU; è la decisione più costosa del milestone.
    Ipotesi nella specifica: accettato (la parte del partner è un lancio/reazione); si decide dopo la misura dello spike 4 (soglia 150 ms).

### Regole di contorno
16. Può montare solo un alleato giocatore, o anche un PNG alleato o un nemico (domato) in futuro?
    Perché conta: i nemici sono minion senza modello NP, quindi un tipo di link diverso e molto lavoro.
    Ipotesi nella specifica: solo alleati giocatori con modello NP; rider singolo, niente catene.
17. Il carrier può usare mosse di movimento speciali (muro, scalata) con un rider in sella, o il link si rompe?
    Perché conta: i modi muro sono il cuore del progetto e il rider parcheggiato complica fisica e telecamera.
    Ipotesi nella specifica: consentito, nessun modo vietato di default (lista `ForbiddenModes` per kit).
18. Cosa succede al rider se il carrier muore, si teletrasporta o si disconnette?
    Perché conta: sono i casi limite che i giocatori incontrano subito in co-op PvE.
    Ipotesi nella specifica: il link si rompe sul server, il rider viene posizionato accanto al carrier (o all'ancora) e riprende il controllo.

### Modularità
19. Va bene che il piggyback sia un plugin separato e opzionale (`GSTNPiggyback`) che dipende da `GSTNLink` (M6) e usa solo i suoi punti di estensione (tipo di link, regola, azioni, §7.0 di M6), anche se questo ritarda un po' M6?
    Perché conta: costringe a rendere generico il sistema di link (kit, regole, snapshot, richieste) invece di cablare il piggyback (e `GSTNCombat` non contiene alcun campo di link).
    Ipotesi nella specifica: sì, né `GSTNCombat` né `GSTNLink` dipendono mai dal piggyback e funziona identico con il plugin disattivato.
---

## M8 Minion: domande al responsabile del progetto

Specifica: `docs/architecture/combat-m8-minions.md`. Domande sul comportamento desiderato; le ipotesi sono quelle usate nella specifica.

### Numeri e varieta

1. Quanti minion devono essere vivi e attivi contemporaneamente in una scena tipica, e quanti nel caso peggiore (ondata finale)?
   Perché conta: decide i budget (server, banda, client) e se basta l'obiettivo 40 o serve lo stretch 100.
   Ipotesi nella specifica: 40 richiesti, 60 tipici di progetto, 100 stretch.

2. Quanti tipi diversi di minion servono al lancio e quanto devono differire (solo statistiche, o mosse e comportamento diversi)?
   Perché conta: definisce quante `UGSTN_MinionDefinition` e quanti task di AI scrivere e testare.
   Ipotesi nella specifica: 2 tipi di test (grunt, heavy); tutto il resto e contenuto da designer.

3. Esistono minion volanti, a distanza (proiettili) o di grandi dimensioni?
   Perché conta: i volanti richiedono un altro backend di movimento, i proiettili un sistema di hitbox `Projectile` che M4 non ha ancora.
   Ipotesi nella specifica: solo minion a terra e corpo a corpo; volanti possibili in seguito col backend Mover per tipo.

### Combattimento e reazioni

4. I minion hanno punti vita (muoiono a 0) o la percentuale in stile platform fighter (volano sempre piu lontano)?
   Perché conta: cambia modello di knockback, interfaccia e come si "uccide" un nemico.
   Ipotesi nella specifica: punti vita, con knockback che cresce con la frazione di vita persa (`KnockbackModel_HealthFraction`).

5. Quanto devono volare via i minion colpiti: sempre lanciati, solo stordimento, o dipende dal colpo? Possono rimbalzare su muri e pavimento (tech, juggle)?
   Perché conta: lanci lunghi richiedono collisione e movimento piu robusti e piu tempo di simulazione per minion.
   Ipotesi nella specifica: lancio balistico con atterraggio e knockdown, nessun tech, nessun rimbalzo avanzato.

6. I minion possono avere armatura/poise (non interrompibili da colpi leggeri) o parare, schivare, usare scudo?
   Perché conta: la difesa dei minion e M6; armatura e invece gia gestita dalla pipeline.
   Ipotesi nella specifica: solo armatura (`PoiseHits`); niente scudo, parata o schivata nei minion.

7. Un singolo attacco del giocatore deve poter colpire molti minion insieme (spazzata, area)?
   Perché conta: richiede di alzare l'anello `HitTargets` e influisce sul costo delle query.
   Ipotesi nella specifica: si, anello portato a 16 voci.

8. Quanti minion possono attaccare lo stesso giocatore nello stesso momento?
   Perché conta: ogni colpo subito costringe il client del giocatore a un rollback; il limite e anche una scelta di difficolta e leggibilita.
   Ipotesi nella specifica: massimo 4 con sistema di token.

### Reazione e "feel"

9. Quanta latenza e accettabile tra il colpo del giocatore e la reazione visibile del minion (flash/hitstop subito, reazione vera dopo circa 150-250 ms)?
    Perché conta: i minion sono interpolati, quindi la reazione autoritativa arriva in ritardo; il feeling dipende dalla presentazione locale.
    Ipotesi nella specifica: flash e pausa immediati sul client dell'attaccante, reazione dopo periodo di replica piu buffer di interpolazione.

10. Il giocatore deve poter "sentire" un colpo mancato solo perche il minion si e mosso (lag compensation) o preferisci un colpo equo rispetto a cio che vede?
    Perché conta: la compensazione puo far prendere colpi "dopo essere scappati" ai bersagli ma e essenziale per il PvE reattivo.
    Ipotesi nella specifica: compensazione attiva sui minion con massimo 250 ms, come per i giocatori.

### Elite, boss e flusso di gioco

11. Quando e come un minion diventa elite/boss (script, vita bassa, evento)? Torna mai a minion?
    Perché conta: la promozione e un'entita di rete completa (costosa); il limite e le regole cambiano il budget.
    Ipotesi nella specifica: promozione solo da script/Blueprint, senza ritorno, massimo 4 combattenti promossi alla volta.

12. Come arrivano i minion: ondate a tempo, spawner fissi, spawn continuo? Quanti respawn per partita?
    Perché conta: determina limite di spawn per frame, pool e picchi di banda all'ingresso.
    Ipotesi nella specifica: ondate con spawner e data asset, 4 spawn per frame.

13. Cosa succede alla morte: cadavere, ragdoll, drop, punteggio? I cadaveri contano per il limite?
    Perché conta: i cadaveri occupano slot e banda; il ragdoll e solo estetico e non replicato.
    Ipotesi nella specifica: animazione di morte, cadavere breve (secondi), ragdoll solo cosmetico sul client.

### Giocatori, piattaforme, prestazioni

14. Quanti giocatori per partita co-op (2? 4? di piu) e il server dedicato su quale hardware (vCPU cloud, macchina propria)?
    Perché conta: il costo server cresce con i giocatori (query e rollback) e il budget in ms dipende dalla macchina.
    Ipotesi nella specifica: fino a 4 giocatori, server paragonabile al PC di sviluppo; da confermare con la misura.

15. Piattaforme client e requisiti minimi (PC solo? console? 60 fps?), e quanti minion visibili devono girare a 60 fps?
    Perché conta: il costo delle animazioni dei burattini puo superare quello del server e impone LOD e condivisione animazioni.
    Ipotesi nella specifica: PC, 60 fps, obiettivo 3 ms di game thread per 60 burattini.

16. Priorita tra quantita di nemici, precisione dei colpi (feel) e risparmio di banda se non si possono avere tutti e tre?
    Perché conta: guida le scelte dei compromessi (frequenza di replica, LOD, token) quando i budget sforano.
    Ipotesi nella specifica: prima precisione dell'hit con i nemici vicini, poi quantita, banda per ultima (cap 8 KB/s a 40 minion).

### IA, movimento, mondo

17. Quanto deve essere "intelligente" il comportamento (inseguire e attaccare, aggirare, formazioni, coordinamento, evitare burroni)? Chi la scrive: tu in StateTree?
    Perché conta: M8 offre solo un aggancio (intent, token, task base); un'IA ricca e lavoro a parte.
    Ipotesi nella specifica: StateTree scritto dal designer; percorsi con navmesh a bassa frequenza, nessun coordinamento oltre ai token.

18. I minion devono usare il movimento a muro del progetto (wall run, wall slide) o solo terreno, scale e pendii? I giocatori li spingono (collisione con la capsula del giocatore)?
    Perché conta: wall e spinta richiedono Mover (costoso) invece del movimento leggero.
    Ipotesi nella specifica: solo terreno e scale; separazione tra minion; nessuna spinta dei giocatori sui minion.

### Modularita e tag

19. Il plugin dei minion deve poter essere disattivato in un progetto che usa solo i combattenti, e deve essere riutilizzabile in altri progetti senza il progetto GASP?
    Perché conta: stabilisce quanto vincolare le dipendenze (nessuna verso il modulo di progetto) e dove stanno i contenuti di esempio.
    Ipotesi nella specifica: plugin `GSTNMinion` separato che dipende solo da `GSTNCombat`; il resto del progetto non lo conosce.

---

## Domande M9 — Adattatore GAS standard (opzionale)

Riferimento: `docs/architecture/combat-m9-stock-gas.md`. Servono soprattutto a decidere **se** costruirlo e **cosa può peggiorare**.

### Necessità e motivazione

1. Esiste (o è pianificato) un secondo progetto, una distribuzione (store, console, altro team) o un uso del plugin senza il fork GAS-NPP?
   Perché conta: è il primo criterio go/no-go; senza un consumatore concreto la raccomandazione è no-go.
   Ipotesi nella specifica: nessun consumatore oggi, quindi no-go, ma si fanno subito il controllo dei confini e l'audit S0.

2. Quanto ti pesa aggiornare il fork a una nuova versione di Unreal (giorni di lavoro stimati, patch di `PATCHES.md`)?
   Perché conta: se il costo di manutenzione del fork è alto, M9 diventa una via d'uscita invece di un lusso.
   Ipotesi nella specifica: soglia di circa 5 giorni di lavoro per ogni resincronizzazione.

3. Se il rollback in gioco ti sembrasse "troppo visibile" sulle botte subite, preferiresti correggere con il fork (previsione in avanti) o passare a GAS standard?
   Perché conta: il passaggio a GAS standard non migliora quel problema, lo peggiora.
   Ipotesi nella specifica: M9 non è una cura per il feel delle botte subite.

4. Va bene dividere i plugin (`GSTNCombatNP`, `GSTNInputNP` in plugin propri, e gli adattatori standard in `GSTNCombatStockGAS` e `GSTNInputStockGAS`) anche se M9 non verrà mai fatto? *Proposal* (modularity-and-tags.md §3.6 la recepisce)
   Perché conta: è un refactoring dei file `.uplugin` e dei redirect, ma rende ogni plugin utilizzabile da solo. Il confine "Core e GAS non dipendono da NP" è fatto rispettare in modo bloccante (domanda generale 6).
   Ipotesi nella specifica: sì, è il primo passo (M9.0) ed è utile in ogni caso.

### Funzioni che possono peggiorare

5. Parry e tech con finestre di pochi frame (sotto i 5) sono essenziali anche con latenze di 100 ms o più?
   Perché conta: senza rollback queste finestre sono giudicate all'arrivo del pacchetto e possono diventare ingiuste.
   Ipotesi nella specifica: accettabile solo in PvE co-op; finestre più larghe per l'adattatore standard.

6. Accetti che la reazione di chi subisce un colpo arrivi in ritardo di circa RTT/2 con uno scatto di posizione (nessuna previsione della vittima)?
   Perché conta: è la differenza più visibile tra i due adattatori.
   Ipotesi nella specifica: sì, mascherata da presentazione (lampo, camera shake) e da una breve fusione della mesh.

7. Con GAS standard i giocatori che si afferrano o si montano (M6/M7) perdono il controllo preciso della propria posizione: va bene?
   Perché conta: cambia il feel di presa e cavalcata rispetto al fork.
   Ipotesi nella specifica: sì, posizione guidata dal server, prevista solo la richiesta di montaggio tramite chiave di previsione.

8. È accettabile che un colpo previsto dall'attaccante venga annullato in rari casi (hitstop e cancel su colpo "disfatti" dopo la risposta del server)?
   Perché conta: sul fork l'attaccante è esatto; qui ogni mancata conferma si vede.
   Ipotesi nella specifica: sì, con identificativi di colpo e hitstop previsto breve.

9. Per i cancel (annullare una mossa in un'altra) il server deve fidarsi del frame dichiarato dal client entro ±2 frame, o essere più severo contro i cheat?
    Perché conta: fiducia contro robustezza ai jitter; il gioco è PvE co-op, ma potrebbe cambiare.
    Ipotesi nella specifica: fiducia con tolleranza ±2 e rivalidazione del pattern di input spenta in co-op.

### Contenuti e strumenti

10. Wall modes e salto di piattaforma restano solo NP: va bene che l'adattatore standard non li fornisca (movimento tramite CMC)?
    Perché conta: il tuo movimento caratteristico non esisterebbe nel progetto standard.
    Ipotesi nella specifica: sì, fuori dall'ambito; chi usa l'adattatore fornisce un ponte di movimento.

11. Serve poter passare gli stessi Blueprint di abilità da un adattatore all'altro (strumento di reparenting), o basta riscriverli?
    Perché conta: stesso nome di classe in due moduli rompe i Blueprint; i redirect e lo strumento costano tempo.
    Ipotesi nella specifica: sì, redirect più script Python; i data asset non servono modifiche.

12. Quanto sei disposto a mantenere un secondo progetto "canarino" (solo plugin neutri, senza fork) in CI?
    Perché conta: è l'unico test vero che non ci siano dipendenze nascoste; costa manutenzione.
    Ipotesi nella specifica: sì, build settimanale e prima di ogni release.

13. I plugin opzionali di funzionalità (`GSTNDefence`, `GSTNLink`, `GSTNHeld`) devono avere ciascuno un modulo per l'adattatore standard, oppure restano solo NP finché M9 non si fa? *Proposal*
    Perché conta: ogni plugin con un modulo `*NP` dipende dal plugin adattatore NP; senza moduli per l'adattatore standard, usare GAS standard significa rinunciare a difesa, link e oggetti tenuti.
    Ipotesi nella specifica: restano solo NP (`*NP` dentro il plugin, M6 §1b e M10 §4); gli adattatori standard dei plugin di funzionalità si decidono se M9 parte.

---

## Domande M10 — Oggetti tenuti e lanciati

Riferimento: `docs/architecture/combat-m10-held-objects.md`. Le domande riguardano il comportamento desiderato.

### Tipi di oggetto

1. Quali tipi di oggetto vuoi davvero in gioco: consumabili, armi raccolte, oggetti di scena lanciabili, nemici (minion) afferrati e lanciati?
   Perché conta: decide le fasi (M10a props, b armi, c presa al volo, d minion) e quanto lavoro serve.
   Ipotesi nella specifica: tutte e quattro, in quattro fasi; i minion per ultimi.

2. Quante classi di taglia servono e cosa cambia tra l'una e l'altra (velocità, salto, comandi permessi)?
   Perché conta: ogni taglia moltiplica i binding e il contenuto.
   Ipotesi nella specifica: Light, Medium, Heavy, Huge; Heavy permette solo camminare, posare, lanciare.

3. Le armi raccolte sostituiscono l'arma primaria mentre le tieni, hanno una durata (colpi/usi) e poi si rompono o diventano oggetti lanciabili?
   Perché conta: cambia come si progettano i binding e la durabilità.
   Ipotesi nella specifica: sostituiscono le mosse d'attacco, durabilità a cariche; a zero si rompono o diventano props Light.

### Raccogliere, posare, lanciare

4. Raccogliere è un comando dedicato (es. tasto Afferra) o contestuale all'attacco vicino a un oggetto?
   Perché conta: occupa o no uno slot e può confliggere con le mosse esistenti.
   Ipotesi nella specifica: comando dedicato `Command.Link.Pickup`, senza pattern di default, assegnato dal kit.

5. Se due giocatori raccolgono lo stesso oggetto nello stesso istante, chi vince?
   Perché conta: in co-op è un momento di attrito; la regola deve essere comprensibile.
   Ipotesi nella specifica: vince la richiesta più vecchia, poi l'id entità più basso; il perdente ha un "fumble".

6. Se il portatore subisce un colpo, l'oggetto cade sempre?
   Perché conta: definisce quanto è rischioso trasportare oggetti.
   Ipotesi nella specifica: qualsiasi hitstun fa cadere l'oggetto; le armi con armor frames lo tengono.

7. Il lancio può essere caricato (più forza tenendo premuto) e mirato con la camera, oppure sempre nella direzione dello stick?
   Perché conta: cambia input, curva di velocità e feel della precisione.
   Ipotesi nella specifica: direzione dello stick di default, mira camera opzionale per abilità, carica opzionale.

8. Gli oggetti lanciati fanno danno in base alla velocità d'impatto e alla taglia, o un valore fisso per tipo?
   Perché conta: decide se il danno è leggibile o fisico.
   Ipotesi nella specifica: curva sulla velocità × moltiplicatore di taglia.

9. Cosa succede all'impatto: rimbalza, si ferma e può essere raccolto di nuovo, si rompe, esplode? Dipende dall'oggetto?
    Perché conta: ogni comportamento complica fisica e rete; le geometrie complesse danno rimbalzi strani.
    Ipotesi nella specifica: comportamento per definizione (Break/Bounce/Stick/Explode), niente fisica Chaos.

10. Un proiettile può colpire più bersagli (perforare)? Un nemico colpito può essere ricolpito dallo stesso oggetto dopo un rimbalzo?
    Perché conta: gruppi di colpo e bilanciamento delle folle.
    Ipotesi nella specifica: un colpo per bersaglio per lancio; `MaxHits` per perforare.

11. Gli alleati possono essere colpiti da oggetti lanciati (fuoco amico)? Può dipendere dalla modalità?
    Perché conta: in co-op cambia molto la tensione e il design degli attacchi ad area.
    Ipotesi nella specifica: alleati ignorati di default, variante di fuoco amico per modalità.

12. A chi va il merito di un KO: a chi ha lanciato anche dopo la sua morte? Se un alleato prende l'oggetto al volo e lo rilancia?
    Perché conta: statistiche, punteggi e assist.
    Ipotesi nella specifica: merito al lanciatore (istantanea al lancio); la presa sposta il proprietario; il precedente è un assist.

13. Vuoi poter prendere al volo un oggetto lanciato dai nemici o solo dagli alleati? E deviare con un attacco?
    Perché conta: sono meccaniche separate (presa, deviazione) con regole e finestre diverse.
    Ipotesi nella specifica: alleati sempre, nemici secondo le regole; deviazione solo per oggetti marcati.

14. La finestra di presa è breve e a tempo (stile parry) o ampia?
    Perché conta: sul netcode è giudicata dal server con la posizione "vista" dal giocatore.
    Ipotesi nella specifica: finestra a frame definita dall'abilità, giudicata lato server con compensazione.

### Interazioni con prese e cavalcata

15. Chi cavalca (rider) può raccogliere e lanciare oggetti? E il carrier mentre porta qualcuno?
    Perché conta: richiede due canali di link (corpo e mano) invece di uno.
    Ipotesi nella specifica: il rider sì, il carrier solo oggetti Light se il kit lo permette.

16. Se un nemico afferra chi tiene un oggetto, l'oggetto cade? Si può afferrare tenendo un oggetto pesante?
    Perché conta: stabilisce le priorità tra grab e held.
    Ipotesi nella specifica: l'oggetto cade; con Medium o più non si può afferrare.

17. Afferrare un minion e lanciarlo deve usare le stesse regole degli oggetti (danno, rimbalzo, presa al volo)?
    Perché conta: decide se M10d è un caso particolare o una meccanica nuova.
    Ipotesi nella specifica: il minion lanciato è un proiettile che rientra nel suo componente con stordimento.

### Limiti e contenuti

18. Quanti oggetti attivi nello stesso momento (in volo o a terra) devono essere supportati, e gli oggetti a terra tornano dopo un po'?
    Perché conta: dimensiona pool, banda e budget del server.
    Ipotesi nella specifica: 32 proiettili attivi, oggetti a riposo dormienti; respawn per definizione.

19. Va bene che gli oggetti tenuti siano un plugin separato (`GSTNHeld`, che dipende da `GSTNCombat` e `GSTNLink`) che si può rimuovere senza toccare il resto?
    Perché conta: M10 usa il registro aperto dei tipi di link, `FGSTN_EntityRef` e il canale mano di M6 (§7.0), non hook propri in `GSTNCombat`; la modularità resta garantita.
    Ipotesi nella specifica: sì; i tag `Combat.Link.Type.Held` e dei ruoli Holder/HeldObject li dichiara `GSTNHeld` (domanda M6.21), così `GSTNCombat` e `GSTNLink` non lo conoscono.
