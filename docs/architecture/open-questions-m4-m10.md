# Domande aperte per le milestone M4–M10 e per la modularità

Domande che un ingegnere del software farebbe prima di implementare ogni milestone, sul **comportamento desiderato**
(feeling di gioco, regole, casi limite, priorità). Ogni domanda indica perché conta e, quando c'è, l'ipotesi che la
specifica ha già adottato: se l'ipotesi va bene, basta rispondere "ok".

Rispondi pure in ordine sparso, anche solo con il numero della sezione e della domanda (es. "M6.3: tasto dedicato").
Le risposte vanno poi riportate nelle specifiche e in combat.md.

Le indicazioni del 22/10 sono in [game-design-direction.md](game-design-direction.md); le domande a cui hanno già risposto sono marcate **Risposta** (quelle della mattina del 22/10, su stagger, hitstop e peso, **Risposta (2026-10-22, mattina)**).

## Da rispondere per prime (15)

Le quindici domande ancora aperte la cui risposta cambia di più l'architettura o le prossime milestone (prima M3/M4, poi modularità e tag, poi M4b e M6).

- [[M4.36](#m4--hits-domande-al-proprietario-del-progetto)] il danno rende il knockback più forte solo tramite il peso (modello standard), con la formula di Smash come opzione
- [[M4.32](#m4--hits-domande-al-proprietario-del-progetto)] se la gravità cambia durante un lancio, il lancio segue la nuova gravità
- [[Parti.2](#parti-distruttibili-m4b-domande-al-proprietario-del-progetto)] una parte rotta di un nemico può essere ripristinata
- [[M4.16](#m4--hits-domande-al-proprietario-del-progetto)] colpo subito come correzione di rollback o forward prediction per il client
- [[M4.20](#m4--hits-domande-al-proprietario-del-progetto)] chi decide splat, bounce o distruzione all'impatto (attacco, vittima, superficie)
- [[M4.24](#m4--hits-domande-al-proprietario-del-progetto)] vantaggio dell'host del server di ascolto: accettarlo o attenuarlo
- [[Mod.1](#domande-su-modularità-e-tag-per-il-proprietario)] Game Feature ed Experience subito o dopo M7
- [[Mod.11](#domande-su-modularità-e-tag-per-il-proprietario)] il modulo di progetto smette di dipendere pubblicamente da `GSTNCombatNP`/`GSTNCombatCore`
- [[Mod.17](#domande-su-modularità-e-tag-per-il-proprietario)] regola per le estensioni Blueprint dentro la simulazione (pure; quelle per frame in C++)
- [[Mod.15](#domande-su-modularità-e-tag-per-il-proprietario)] punto di estensione del combat costruito una volta sola prima di M6
- [[M4.31](#m4--hits-domande-al-proprietario-del-progetto)] trampolini: tenere premuto salto o il fast fall cambiano il rimbalzo
- [[Parti.12](#parti-distruttibili-m4b-domande-al-proprietario-del-progetto)] quando un nemico diventa afferrabile (numero e tipo di parti rotte)
- [[M8.1](#m8-minion-domande-al-responsabile-del-progetto)] quanti minion vivi e attivi in una scena tipica e nel caso peggiore
- [[M6.17](#m6--difesa-e-link-domande-al-proprietario-del-progetto)] cosa fare se lo spike del link parcheggiato non passa le soglie
- [[M6.22](#m6--difesa-e-link-domande-al-proprietario-del-progetto)] precedenza del tasto di interazione tra alleato, nemico e oggetto

| Sezione | Specifica |
|---|---|
| Modularità e tag | [modularity-and-tags.md](modularity-and-tags.md) |
| Indicazioni del proprietario (22/10) | [game-design-direction.md](game-design-direction.md) |
| M4 Colpi | [combat-m4-hits.md](combat-m4-hits.md) |
| M4b Parti distruttibili | [combat-parts.md](combat-parts.md) |
| M5 Editor | [combat-m5-editor.md](combat-m5-editor.md) |
| M6 Difesa e link | [combat-m6-defence-links.md](combat-m6-defence-links.md) |
| M7 Piggyback | [combat-m7-piggyback.md](combat-m7-piggyback.md) |
| M8 Minion | [combat-m8-minions.md](combat-m8-minions.md) |
| Molti nemici e nemici-piattaforma (ricerca del 22/10) | [many-enemies-replication.md](many-enemies-replication.md) |
| M9 Adattatore GAS standard | [combat-m9-stock-gas.md](combat-m9-stock-gas.md) |
| M10 Oggetti tenuti e lanciati | [combat-m10-held-objects.md](combat-m10-held-objects.md) |
| Superfici di attraversamento sulle parti dei nemici | [traversal-surfaces.md](traversal-surfaces.md) |
| M11 Kit in gioco, armi di supporto, mod, zaini | [combat-loadout-mods.md](combat-loadout-mods.md) |
| Modalità di gioco (Livelli, Spedizione, PvP) | [game-modes.md](game-modes.md) |
| Indicatore di profondità | [depth-marker.md](depth-marker.md) |
| Telecamera | [camera.md](camera.md) |
| Interazione (cosa si sta puntando) | [interaction.md](interaction.md) |
| Cristallo per giocatore, god rays, ombra sulla nebbia | [combat-parts.md](combat-parts.md) §6.0, [depth-marker.md](depth-marker.md) §4.1 |
| Input a carica (stile Guile) | [input.md](input.md) §10 |
| Zone di oggetti attivabili da un solo giocatore | [level-activation.md](level-activation.md) |
| Demolizione, hitbox variabili, spin da tastiera, collisioni dei colpi | [combat-m4-hits.md](combat-m4-hits.md) §3, §7c, §9c; [input.md](input.md) §9 |


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
   **Risposta (2026-10-22):** Sì: ogni plugin deve essere utilizzabile da solo in altri progetti indipendenti, con il minimo coupling tra plugin ([modularità §3.7](modularity-and-tags.md#37-reuse-in-independent-projects-owner-direction-2026-10-22)). Resta aperto se in futuro ci sarà anche una pubblicazione (Fab).

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
    **Risposta (2026-10-22, 10:34):** I designer devono poter creare tag appropriati per i nomi di armi, kit, speciali e simili ([direzione §2g](game-design-direction.md#2g-interact-button-dodge-windows-designer-tags-blueprint-replaceable-content-owner-input-2026-10-22-1034-and-1036)). Nella specifica: radici native, foglie create dall'editor nell'ini di chi possiede il contenuto, il codice non nomina mai una foglia ([modularità §4.2b](modularity-and-tags.md#42b-designer-identity-tags-owner-direction-2026-10-22)).

14. Vuoi una mappa di test minima e un test automatico per ogni plugin, e una build CI con ciascun plugin opzionale disattivato?
    Perché conta: è l'unico modo di dimostrare che "ogni plugin funziona da solo"; ha un costo di manutenzione.
    Ipotesi nella specifica: sì per i plugin opzionali e per i moduli NP; i moduli Core con soli test Automation.

15. Il punto di estensione del combat (frammenti di stato, hook di tick, registro dell'inbox) va costruito una volta sola prima di M6, anche se ritarda M3/M4?
    Perché conta: cambia il formato di rete dello stato; farlo dopo costringe a rifare serializzazione e test.
    Ipotesi nella specifica: sì, con test propri, e M3 prepara già i punti (sezione 8.5).

16. La documentazione per plugin (README) va tenuta in inglese come il resto, e con quale livello di dettaglio?
    Perché conta: ogni plugin deve avere docs leggibili da una persona; lo standard fissa il lavoro per ogni nuovo plugin.
    Ipotesi nella specifica: inglese, README breve + documento di architettura in docs/architecture/.

### Dopo le risposte delle 10:34 e 10:36 del 22/10 (Blueprint e contenuti sostituibili)

17. Le estensioni in Blueprint chiamate dentro la simulazione devono essere "pure" (leggono solo i dati che ricevono, niente accesso al mondo, niente `Delay`), e quelle chiamate ogni frame per ogni entità restano in C++ di default. Va bene?
    Perché conta: con il rollback la simulazione riesegue le stesse funzioni molte volte; un Blueprint che legge il mondo o tiene stato proprio rompe la predizione, e uno chiamato ogni frame su centinaia di nemici costa troppo.
    Ipotesi nella specifica: sì; per lo stato per entità c'è un frammento generico di contatori per i designer, e un controllo di determinismo in debug ([modularità §3.8](modularity-and-tags.md#38-designer-extension-in-blueprint-owner-direction-2026-10-22)).

18. Se host e client hanno contenuti di simulazione diversi (un kit o una mossa modificata, una mod), la connessione va rifiutata?
    Perché conta: con contenuti sostituibili, due macchine possono simulare regole diverse e desincronizzarsi in modo invisibile.
    Ipotesi nella specifica: sì, con una firma dei contenuti di simulazione controllata all'ingresso; i contenuti estetici (skin, suoni, UI) possono differire ([modularità §3.9](modularity-and-tags.md#39-replaceable-content-owner-direction-2026-10-22)).

19. I nomi delle speciali come tag: sotto la radice delle mosse già usata da M3 (`Ability.Combat.Special.<Nome>`) o sotto una radice propria (`Special.<Nome>`)?
    Perché conta: con due radici la stessa mossa avrebbe due identità da tenere allineate.
    Ipotesi nella specifica: `Ability.Combat.Special.<Nome>` ([modularità §4.2b](modularity-and-tags.md#42b-designer-identity-tags-owner-direction-2026-10-22)).

### Dopo i messaggi del 23/10 (kit, mod, zaini, modalità)

20. Spostiamo la "sonda di interazione" (quella che sceglie se il tasto interazione afferra, monta, raccoglie un oggetto o un kit, o attiva un terminale) dal plugin dei link a un piccolo plugin di base proprio, `GSTNInteract`?
    Perché conta: oggi vive in `GSTNLink`, ma solo la presa è un link; raccolta di kit e terminali delle missioni dovrebbero altrimenti dipendere dal plugin dei link.
    Ipotesi nella specifica: sì, `GSTNInteract` (Core e NP) proprietario della radice `Interact.*` ([modularità §3.6](modularity-and-tags.md#36-map-of-features-to-plugins), [M11 §12](combat-loadout-mods.md#12-world-pickups-and-drops-gstnpickups)).
    **Risposta (2026-10-23):** Sì, un plugin dedicato: il contratto resta semplice (un'interfaccia più i gameplay tag), ma il plugin serve a rendere facile interagire con **ciò che si sta puntando**, anche con moltissime armi per terra. Specifica: [interaction.md](interaction.md) (`GSTNInteract`). La radice dei tag diventa `Interact.*` (non più `Combat.Interact.*`), così il plugin funziona anche senza il combattimento.

---

## M4 — Hits: domande al proprietario del progetto

Riferimenti: `combat-m4-hits.md` (qui "M4 §n") e `combat.md` (qui "combat §n").

### A. Modello di danno e knockback

**1. Il gioco usa danno a percentuale (stile Smash), a HP, o entrambi (es. percentuale per i giocatori, HP per i nemici PvE)?**
Perché conta: decide `DamageMode`, il modello di knockback di default e la lettura di UI/bilanciamento; le due modalità danno un "feel" opposto.
Ipotesi nella specifica: `DamageMode` è un'opzione per regole di gioco (M4 §3 Combat rules); default `PercentScaling` Smash-like, `Fixed` per i giochi a HP (M4 §3 Knockback model; combat §1).
**Risposta (2026-10-22):** Entrambi, in un solo modello: danno a percentuale in stile platform fighter (il knockback cresce con la percentuale) ma con un tetto massimo; oltre il tetto la parte, o l'intera entità, viene distrutta ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)). Corrisponde a `PercentCapped` (M4 §3b). Resta aperto il valore del tetto per giocatori, nemici e singole parti.

**2. Nel PvE un nemico comune deve poter essere lanciato via come un giocatore, o solo subire stagger/hitstun senza launch?**
Perché conta: con 40+ nemici il launch pieno costa in rollback, in leggibilità visiva e in controllo della folla.
Ipotesi nella specifica: stessa pipeline per giocatori e IA (combat §1), nessuna distinzione per tipo di nemico; solo `KnockbackResistance`/`Poise` negli attributi (M4 §4).
**Risposta (2026-10-22):** Il nemico ha peso legato alla salute rimasta (più è danneggiato, più vola lontano) e fattori di stagger e juggle; hitstun e hitstop dipendono da cosa e dove viene colpito ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)). Resta aperto se i nemici comuni vengano sempre lanciati o possano solo subire stagger.
**Risposta (2026-10-22, mattina):** Il peso decide cosa succede quando un nemico entra in hitstun: quanto vola via, oppure se non vola affatto e viene solo staggerato sul posto, oppure se viene mandato all'indietro ([direzione §2b](game-design-direction.md#2b-hitstun-per-part-stagger-force-hitstop-weight-owner-input-2026-10-22-morning)). Quindi anche i nemici comuni usano tre livelli di reazione (lancio, spinta indietro, stagger sul posto), scelti confrontando il knockback con le soglie dei livelli (M4 §3). Restano aperti i valori delle soglie e in quale livello cadano di solito i vari tipi di nemico.

**3. Boss e nemici pesanti: armatura, super-armor o resistenza al knockback sono previste in M4, e con quale soglia?**
Perché conta: lo stato `Armor` esiste ma servono valori e regole (soglia in danno o in knockback?) per non rendere i boss inutilizzabili o banali.
Ipotesi nella specifica: `Armor` = niente knockback/hitstun sotto `ArmorThreshold`, il danno si applica (M4 §3 Hurtboxes); il rapporto tra `Poise` e `Armor` non è definito.
**Risposta (2026-10-22):** Per i nemici grandi vale il modello a parti: durezza della parte contro penetrazione dell'hitbox decide se la parte subisce danno ([direzione §1](game-design-direction.md#1-destructible-body-parts-helldivers-2-model)), più il fattore di stagger ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)). Restano aperti la soglia di armatura/super-armor dei boss e il rapporto tra `Poise` e `Armor`.
**Risposta (2026-10-22, mattina):** Se un nemico o un boss entra in hitstun dipende dalla parte colpita: ogni parte ha una condizione di hitstun, in genere una soglia che la forza di stagger dell'attacco deve raggiungere ([direzione §2b](game-design-direction.md#2b-hitstun-per-part-stagger-force-hitstop-weight-owner-input-2026-10-22-morning)). Per i boss le soglie alte stanno sulle parti corazzate (combat-parts §4.4); la specifica propone anche che `ArmorThreshold` sia misurata in forza di stagger (M4 §3). Restano aperti i valori delle soglie e il rapporto tra `Poise` e `Armor` (con la soglia per parte `Poise` potrebbe diventare ridondante).
**Risposta (2026-10-22, tarda mattina):** La super armor è il caso speciale di resistenza allo stagger **infinita**, per nemici e giocatori ([direzione §2e](game-design-direction.md#2e-super-armor-and-ricochet-owner-input-2026-10-22-late-morning)): nella specifica una soglia `Infinite` (M4 §3). Resta aperto se serva ancora un'armatura che si rompe dopo N colpi (`Poise`).
**Risposta (2026-10-22, prima di mezzogiorno):** Armor e poise sono casi speciali della resistenza allo stagger e del peso rimanente ([direzione §2f](game-design-direction.md#2f-clarifications-owner-input-2026-10-22-before-noon)): niente sistema `Poise` separato (tolto da M4 e M8).

**4. Esiste un limite superiore al knockback/launch (o un KO per percentuale/blast zone), e cosa succede quando un personaggio "muore" (KO)?**
Perché conta: M4 emette l'evento `KO` (revisione 2026-10-18) ma non dice quando scatta né cosa fa il gioco dopo (respawn, spettatore, game over).
Ipotesi nella specifica: nessuna; si cita solo l'evento `Combat.Event.KO`.
**Risposta (2026-10-22):** Quando la percentuale supera il tetto la parte o il nemico viene distrutto, e i morti vanno in pezzi come cristallo ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight), [direzione §5](game-design-direction.md#5-death-and-damage-presentation-crystal-people)). Restano aperti la condizione di KO per i giocatori (blast zone o tetto), e cosa fa il gioco dopo (respawn, spettatore, game over).
**Risposta (2026-10-22, prima di mezzogiorno):** Si muore per **vita a zero** o entrando in una **zona di KO** (es. burroni) ([direzione §2f](game-design-direction.md#2f-clarifications-owner-input-2026-10-22-before-noon)); nella specifica `AGSTN_KOZone` e `KOReason` (M4 §3). Resta aperto cosa succede dopo (respawn, spettatore, fine partita).

### B. Hitstop e sensazione dei colpi

**5. Quanto deve durare e "sentirsi" l'hitstop: ci sono valori target in frame per un jab, un colpo medio e uno forte? Deve essere uguale per attaccante e vittima?**
Perché conta: sono le costanti `HitstopPerDamage`, `HitstopBase`, `MaxHitstop` e i moltiplicatori; sono il cuore del game feel.
Ipotesi nella specifica: `floor(Damage × HitstopPerDamage + HitstopBase)` con tetto `MaxHitstop`, moltiplicatori separati per attaccante e vittima (M4 §3); nessun valore numerico.
**Risposta (2026-10-22):** L'hitstop dipende da cosa e dove viene colpito ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)), quindi anche dalla parte colpita. Restano aperti i valori in frame per jab, colpo medio e forte, e se attaccante e vittima li condividono.
**Risposta (2026-10-22, mattina):** L'hitstop può essere calcolato in automatico dal danno e dal knockback dell'attacco, oppure scelto a parte dal designer per ogni attacco ([direzione §2b](game-design-direction.md#2b-hitstun-per-part-stagger-force-hitstop-weight-owner-input-2026-10-22-morning)); in M4 §3 sono le modalità `Auto` e `Override`. Restano aperti i valori in frame (jab, colpo medio, forte), se attaccante e vittima li condividono e se l'automatico usi il knockback scritto nell'attacco o quello reale sul bersaglio (vedi M4.28).

**6. Durante l'hitstop la vittima può fare SDI: in M4 è davvero in scope, o si rimanda a M6 insieme a tech/shield?**
Perché conta: il testo si contraddice (SDI "oltre lo spostamento di base" è fuori scope in M4 §1, ma M4 §7 lo descrive con `MaxSDIPerHit`); cambia lavoro e test.
Ipotesi nella specifica: SDI base (spostamento sul piano, N volte per colpo) in M4 §7; tuning avanzato in M6 (M4 §1).
**Risposta (2026-10-22, tarda mattina):** L'SDI si fa **tenendo premuta** la direzione in cui si vuole driftare durante l'hitstop, niente mash ([direzione §2c](game-design-direction.md#2c-di-sdi-and-dodging-in-hitstun-owner-input-2026-10-22-late-morning)). Nella specifica resta in M6 §5.5 (deriva continua con un tetto di distanza per colpo); resta aperto solo se anticiparlo in M4.

**7. Durante il proprio hitstop, il giocatore deve poter bufferizzare input e quali (attacco, salto, dodge)? E la vittima in hitstun può fare qualcosa (es. DI, ma anche dodge/uscite)?**
Perché conta: definisce "reattività" e potenziale di combo infinite/escape; influisce su cosa registra il buffer.
Ipotesi nella specifica: i press durante l'hitstop sono registrati, l'età del buffer esclude i frame di hitstop (M4 §8); nessuna regola su azioni in hitstun oltre DI/SDI.
**Risposta (2026-10-22, tarda mattina):** In genere **non si può schivare durante hitstun o hitstop** ([direzione §2c](game-design-direction.md#2c-di-sdi-and-dodging-in-hitstun-owner-input-2026-10-22-late-morning)); la vittima ha DI e SDI. Nella specifica è un dato delle abilità (tag bloccanti), quindi un kit può avere un'uscita speciale. Resta aperto quali altri input bufferizzare durante il proprio hitstop.

**8. Il DI deve essere solo sul piano di movimento (come deciso), o anche in verticale/3D per colpi che lanciano in aria?**
Perché conta: in 3D il DI solo orizzontale può limitare molto la sopravvivenza in aria rispetto al 2D.
Ipotesi nella specifica: DI solo orizzontale, verticale invariata (M4 §3 DI; "as decided").
**Risposta (2026-10-22, tarda mattina):** No: il DI cambia anche l'elevazione del lancio ([direzione §2c](game-design-direction.md#2c-di-sdi-and-dodging-in-hitstun-owner-input-2026-10-22-late-morning)). DI in rende il lancio più ripido sul suo lato (su o giù), DI out lo avvicina all'orizzontale senza superarlo, lo stick laterale ruota attorno all'asse "su". Formula in M4 §3; vedi M4.29 e M4.30 per due punti da confermare.

**9. Angoli di lancio: per un attacco 3D la direzione "giusta" è la facciata dell'attaccante, il vettore hitbox→vittima o la velocità della hitbox, e può cambiare per colpo?**
Perché conta: determina la sensazione di "controllo" del lancio e la combinabilità dei colpi; `Sakurai3D` e `AutoLink` hanno comportamenti diversi.
Ipotesi nella specifica: `YawSource` per hitbox, `SpecialAngle` opzionale; non c'è un default consigliato per i colpi comuni (M4 §3 Hit data).

### C. Schivate (dodge)

**10. Regole delle schivate: durata dell'intangibilità, frame di vulnerabilità finale (recovery), costo (cooldown, stamina, meter) e se si può schivare durante hitstun?**
Perché conta: roll, spot dodge e air dodge sono in scope (M4 §1) ma la specifica definisce solo il meccanismo `HurtboxModifier`, non le regole.
Ipotesi nella specifica: nessuna sul costo/anti-spam; l'intangibilità viene dai tracciati `Intangible` della timeline.
**Risposta (2026-10-22):** Schivata base in PvE a terra: quasi un long jump di Super Mario o un tuffo di Helldivers, senza finire sdraiati all'atterraggio; solo i piedi sono invincibili. Nessuna implementazione standard: può dipendere dal kit ([direzione §6](game-design-direction.md#6-dodge-and-shield)). Restano aperti durata, recovery, costo (cooldown, stamina) e se si può schivare durante hitstun.
**Risposta (2026-10-22, tarda mattina):** In genere non si schiva durante hitstun o hitstop ([direzione §2c](game-design-direction.md#2c-di-sdi-and-dodging-in-hitstun-owner-input-2026-10-22-late-morning)).
**Risposta (2026-10-22, 10:34, corretta alle 11:41):** I piedi sono invincibili **nella parte iniziale** della schivata (alle 10:34 era scritto "finale" per errore); schivate e salti sono abilità pluggabili, quindi si cambia facilmente; in generale quali porzioni degli hurtbox sono invincibili, e quando, lo decide il designer ([direzione §2g](game-design-direction.md#2g-interact-button-dodge-windows-designer-tags-blueprint-replaceable-content-owner-input-2026-10-22-1034-and-1036), M4 §1b). Restano aperti durata, recovery e costo; vedi M4.33 e M4.34.

**11. L'air dodge può essere usata più volte in aria, e lascia il personaggio in stato "helpless"?**
Perché conta: incide molto sul recupero e sul gioco in verticale 3D, e va allineato ai salti aerei del M3.
Ipotesi nella specifica: nessuna.
**Risposta (2026-10-22):** La schivata in aria è un dive roll alla Super Mario 3D World; la modalità di movimento rolling è rimandata e non va implementata ora ([direzione §6](game-design-direction.md#6-dodge-and-shield)). Restano aperti il numero di schivate aeree consentite e lo stato helpless; vedi anche M4.22 e M4.23.

### D. PvE, squadre e fuoco amico

**12. Nel co-op PvE il fuoco amico è attivo di default? Se sì, quanto è "serio" (danno pieno) rispetto a una versione leggera (niente hitstun, danno ridotto)?**
Perché conta: scelta di design più visibile del progetto; determina i valori delle varianti per relazione `Ally`.
Ipotesi nella specifica: `bFriendlyFire` per modalità; esempio di variante alleati con forma dimezzata e senza hitstun (M4 §3, §10; combat §12.3).
**Risposta (2026-10-22):** La modalità di riferimento è il PvE (la schivata base è definita per il PvE) e i nemici lanciati colpiscono altri nemici ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)). Il fuoco amico tra giocatori in co-op non è stato deciso: resta aperto.

**13. Gli attacchi di supporto (cure, buff, ecc.) su alleati usano la stessa pipeline di hit (e quindi hitbox/hitstop), o sono esclusi dalla pipeline di danno?**
Perché conta: combat §1 cita "heals on allies"; M4 non dice se una cura è un `Hit` con hitstop o un effetto separato.
Ipotesi nella specifica: nessuna; la pipeline ha solo `TeamFilter` e varianti per relazione (M4 §3).

**14. I nemici si colpiscono tra loro (fuoco amico IA, area/boss che colpisce i minion)? E i colpi dei nemici che toccano oggetti/ambiente?**
Perché conta: con 40+ nemici un hit tra IA moltiplica costo e caos; serve per la tabella `HittableRelationships`.
Ipotesi nella specifica: il resolver restituisce Self/Ally/Enemy/Neutral; `NoTeam` = Neutral (M4 §6.3); la politica sui Neutral non è decisa.
**Risposta (2026-10-22):** Un nemico lanciato dal giocatore colpisce altri nemici ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)), quindi almeno quel caso di danno tra nemici è voluto (credito al lanciatore, M10 §6b). Restano aperti i colpi tra IA ordinari, le aree dei boss sui minion e la politica sui Neutral.

### E. Lag compensation ed equità

**15. In PvE co-op la lag compensation (`bUseLagCompensation`, `MaxRewindMs`) deve essere attiva di default? Quale massimo di rewind è accettabile per i colpi contro nemici IA?**
Perché conta: un rewind alto fa colpire "dietro l'angolo" per chi subisce, ma un rewind nullo fa mancare colpi che sul client sembravano ok.
Ipotesi nella specifica: opzione per modalità; le IA non rewindano mai, ma il bersaglio sì (M4 §9); nessun valore per `MaxRewindMs`.
**Risposta (2026-10-22):** Server di ascolto: un amico ospita, gli altri sono client ([direzione §6b](game-design-direction.md#6b-network-topology-listen-server-owner-input-2026-10-22)). L'host non ha latenza né rewind: la lag compensation vale solo per i colpi dei client. Resta aperto il massimo di rewind per i client e se attenuare il vantaggio dell'host (vedi M4.24).

**16. Quando un giocatore subisce un colpo, accettiamo che arrivi come correzione (rollback ~RTT/16 frame, "ti colpisce con ritardo"), o si vuole che M4 anticipi il colpo sul client locale (forward prediction)?**
Perché conta: è la decisione di feel più rischiosa per il giocatore colpito (movimento che scatta indietro); la specifica rimanda la scelta.
Ipotesi nella specifica: forward prediction spenta in M4, ogni hit sul giocatore locale è una correzione (M4 §13; combat §12.7).
**Nota (2026-10-22):** Con il server di ascolto la correzione ritardata riguarda solo i client; l'host non vede mai un rollback sui propri colpi subiti ([direzione §6b](game-design-direction.md#6b-network-topology-listen-server-owner-input-2026-10-22)). La scelta di feel per i client resta aperta.

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
**Risposta (2026-10-22):** Quando la salute scende devono saltare via pezzi: piccoli frammenti estetici o interi arti, e alla morte i corpi vanno in pezzi ([direzione §5](game-design-direction.md#5-death-and-damage-presentation-crystal-people)). Restano aperti flash, suoni, camera shake e numeri di danno obbligatori in M4.

### G. Dopo la direzione del 22/10: impatti, schivata e host

**20. Negli impatti con le superfici, chi deve decidere tra splat, bounce e distruzione: l'attacco, la vittima o la superficie?**
Perché conta: cambia dove il designer scrive i dati e chi può "forzare" un risultato (una mossa finale che sfonda sempre il muro, un nemico pesante che non si schiaccia mai).
Ipotesi nella specifica: tre livelli con precedenza: dati dell'attacco (forza o vieta), poi la vittima (cosa può subire), poi la superficie (materiale fisico mappato su `Surface.Type.*`).

**21. "Distruzione" all'impatto significa che si rompe la superficie (un muro di vetro) o che si frantuma la vittima?**
Perché conta: sono due meccaniche diverse (richiesta alla superficie contro danno che può raggiungere il tetto e causare KO).
Ipotesi nella specifica: si rompe la superficie e la vittima prosegue più lenta; la distruzione della vittima è solo un danno da impatto che può arrivare al tetto.

**22. Nella schivata in aria (dive roll) vale la stessa regola dei piedi soli invincibili, o il corpo intero è intangibile?**
Perché conta: decide quanto è sicura la schivata aerea e come si disegnano gli hurtbox.
Ipotesi nella specifica: stessa regola della schivata a terra (solo i piedi), finché non decidi altrimenti.

**23. Sui piedi la schivata è "intangibile" (il colpo non registra proprio) o "invincibile" (il colpo registra ma senza danno)?**
Perché conta: con l'invincibilità l'attaccante ha comunque hitstop e conferma del colpo, con l'intangibilità no.
Ipotesi nella specifica: intangibile.
**Risposta (2026-10-22, 10:34):** In generale lo decide il designer ([direzione §2g](game-design-direction.md#2g-interact-button-dodge-windows-designer-tags-blueprint-replaceable-content-owner-input-2026-10-22-1034-and-1036)): lo stato della finestra è un dato della mossa. Nella specifica il valore di partenza della schivata base resta intangibile (M4 §1b).

**24. Il vantaggio dell'host (zero latenza) va accettato così com'è o attenuato?**
Perché conta: in cooperativo è tollerabile, ma l'host rompe arti e colpisce in modo visibilmente più immediato.
Ipotesi nella specifica: nessuna mitigazione nella simulazione; la compensazione del lag resta la leva di equità; eventuale ritardo solo di presentazione se i playtest lo chiedono.


### H. Dopo la direzione della mattina del 22/10: stagger, hitstop e livelli di reazione

**25. I giocatori usano le stesse soglie di stagger e gli stessi livelli di reazione dei nemici (un giocatore colpito può restare fermo sul posto o essere solo spinto indietro), o vengono sempre lanciati?**
Perché conta: in un platform fighter ogni colpo lancia; se i giocatori usano i livelli, il peso del kit decide quanto è "pesante" un personaggio e una soglia alta lo rende un tank, ma cambia il feeling del PvP e del co-op.
Ipotesi nella specifica: stesse regole e stessa pipeline per tutti (combat §1); soglia dei giocatori bassa (ogni colpo manda in hitstun) e `MaxReaction = Launch`; i kit pesanti possono alzare la soglia e abbassare `MaxReaction` (M4 §3).

**26. Quanto deve scivolare una spinta indietro (`PushBack`), e si ferma contro muri e bordi?**
Perché conta: con 40+ nemici decide leggibilità e rischio di spinte nel vuoto o contro i muri; stabilisce se serve una regola di bordo e di collisione per quel movimento.
Ipotesi nella specifica: velocità = knockback × fattore con tetto e decadimento fisso per frame; resta a terra e si ferma contro muri e bordi (niente splat, rimbalzo o caduta); se la vittima è in aria o su un muro diventa un lancio (M4 §3, §7).

**27. Un nemico staggerato sul posto può essere "juggled" (lanciato da un secondo colpo)? Un colpo sotto soglia durante l'hitstun lo prolunga o non cambia nulla?**
Perché conta: decide le combo sui nemici e il rischio di stun-lock (tenere un nemico fermo per sempre) o, al contrario, di colpi che non si concatenano.
Ipotesi nella specifica: un secondo colpo sopra soglia ricalcola la reazione (può diventare un lancio se il knockback supera la soglia), uno sotto soglia non cambia lo stato; nessun decadimento dell'hitstun (resta la lacuna sull'hitstun decay).
**Risposta (2026-10-22, 11:18):** Sì, con la logica standard: il secondo colpo ricalcola la reazione con il peso rimasto dopo il danno del primo; se il knockback non basta a lanciarlo, il nemico viene solo staggerato di nuovo ([direzione §2h](game-design-direction.md#2h-hardness-stagger-resistance-weight-three-dials-owner-input-2026-10-22-11181121), M4 §3). Resta aperto se un colpo sotto soglia durante l'hitstun lo prolunghi.

**28. L'hitstop automatico usa danno e knockback scritti nell'attacco, o il knockback reale sul bersaglio (che dipende da percentuale e peso)?**
Perché conta: con il knockback reale l'attaccante non può prevedere il proprio hitstop in locale (la vittima è interpolata) e rischia un disallineamento visibile; con quello scritto, colpire un nemico quasi morto non congela di più.
Ipotesi nella specifica: danno e knockback scritti nell'attacco, senza moltiplicatori della vittima (solo dati statici); il designer può comunque fissare i frame a mano per attacco (`Override`) (M4 §3).
**Risposta (2026-10-22, tarda mattina):** Il client deve predire il comportamento corretto. Nella specifica l'hitstop automatico usa ora il knockback reale, calcolato dallo stato della vittima che l'attaccante vede (percentuale, peso, parti, finestre); l'host usa lo stesso stato riavvolto con la lag compensation, quindi la predizione coincide. Solo se lo stato della vittima cambia nella finestra di latenza (es. un altro giocatore la colpisce) l'hitstop di vittima e attaccante può differire di qualche frame, senza correzioni (M4 §3, §9). Anche la reazione della vittima viene mostrata subito sul client come presentazione.


### I. DI, gravità e trampolini (dopo la direzione della tarda mattina del 22/10)

**29. Nel tuo esempio, DI out su un lancio con dot 0.4 dà 0.4 (nessun cambiamento) o 0.2?**
Perché conta: con 0.4 il DI out non farebbe nulla sui lanci verso l'alto, mentre nel caso verso il basso lo hai descritto come "dot più basso"; la regola simmetrica dà 0.2.
Ipotesi nella specifica: 0.2 (DI out avvicina all'orizzontale di `MaxDIElevationDot` = 0.2, senza superarlo) (M4 §3 DI).
**Risposta (2026-10-22, prima di mezzogiorno):** I valori erano esempi: DI in rende il lancio più verticale, DI out più orizzontale; le quantità sono parametri ([direzione §2f](game-design-direction.md#2f-clarifications-owner-input-2026-10-22-before-noon)).

**30. Va bene il salto attorno alla soglia di -0.1? Con DI in, un lancio a -0.09 sale a +0.11, uno a -0.11 scende a -0.31.**
Perché conta: è una conseguenza diretta della soglia; si può ammorbidire con una zona di transizione, ma cambia la regola che hai dato.
Ipotesi nella specifica: soglia netta, regolabile (`UpwardThresholdDot`), più una curva opzionale che riduce l'effetto vicino alla verticale (M6 `ElevationDeltaByElevation`).

**31. Sui trampolini: tenere premuto salto fa rimbalzare più in alto (come il salto variabile), e il fast fall sul trampolino aumenta il rimbalzo?**
Perché conta: decide se il trampolino è un oggetto passivo o parte del sistema di movimento dei kit.
Ipotesi nella specifica: rimbalzo lungo la normale del trampolino, fisso o scalato dalla velocità d'arrivo; interazione con salto e fast fall da decidere (M4 §7b, trampolini).

**32. Quando la gravità cambia durante un lancio o un hitstun (es. si entra in una zona a gravità diversa), il lancio si piega con la nuova gravità o segue quella del momento del colpo?**
Perché conta: con gravità arbitraria il "su" del DI, la caduta e il tumble dipendono dalla gravità; serve una regola chiara anche per i rollback.
Ipotesi nella specifica: tutto usa la gravità del frame corrente della vittima (Mover), il DI usa quella del frame di fine hitstop (M4 §3).

### J. Schivata a terra (dopo le risposte delle 10:34 del 22/10)

**33. La schivata a terra si può annullare con un salto o un'altra mossa prima della coda cancellabile?**
Perché conta: decide quanto la schivata è un impegno e se serve una finestra di cancel apposita.
Ipotesi nella specifica: no, solo la coda cancellabile decisa dal designer, come le altre mosse ([direzione §6c](game-design-direction.md#6c-cancellable-animation-tails-owner-input-2026-10-22)).

**34. Fin dove arriva la "parte iniziale" in cui i piedi sono invincibili?**
Perché conta: serve un valore di partenza per il kit base, anche se poi lo regola il designer.
Ipotesi nella specifica: dal primo frame attivo (lo stacco) al 40% del volo (M4 §1b).

### K. Durezza, resistenza allo stagger e peso (dopo i messaggi delle 11:18–11:21 del 22/10)

**35. Il danno del colpo stesso alleggerisce già il peso che decide il suo knockback, o conta solo il danno subito prima?**
Perché conta: nel primo caso ogni colpo forte lancia un po' di più (come la percentuale in Smash, che include il danno del colpo); nel secondo il tuo esempio vale alla lettera e il risultato si legge dallo stato visibile prima del colpo.
Ipotesi nella specifica: conta solo il danno subito prima del colpo (M4 §3, punto 4).
**Risposta (2026-10-22, 11:32):** Confermata l'ipotesi: conta solo il danno subito prima del colpo.

**36. Il danno rende il knockback più forte solo tramite il peso (un'unica manopola, modello standard `WeightDriven`), mentre la formula di Smash con la percentuale come termine a sé resta un'opzione per il PvP?**
Perché conta: se peso e percentuale contano entrambi, lo stesso danno viene contato due volte e i nemici malconci volano troppo; con una sola manopola il designer regola "quanto si alleggerisce" con una curva.
Ipotesi nella specifica: sì; con la formula di Smash la curva del peso deve essere piatta e il validatore lo controlla (M4 §3, combat-parts §4.1, M8 §3).

### Lacune nella specifica

- Il testo non dice cosa succede a una hitbox contro più bersagli nello stesso frame oltre il "una volta per gruppo": manca un limite di bersagli per colpo (importante con 40+ nemici) e un ordine di risoluzione deterministico per l'attaccante.
- `HitTargets` ha 8 voci: non è definito il comportamento quando si superano (colpo multiplo su folle, sostituzione, overflow).
- Contraddizione sull'SDI: fuori scope in M4 §1 ma descritto in M4 §7.
- Nessun valore di partenza per le costanti del modello di knockback, hitstop, DI e SDI (peso, `HitstunFactor`, `TumbleThreshold`, `MaxHitstop`, soglie di stagger e dei livelli di reazione): l'implementatore deve inventarli.
- Il KO è un evento ma non c'è definizione della condizione (HP a 0? blast zone?), né di cosa fare dopo.
- Cure/buff agli alleati e `HitType` diversi da Normal (Grab/Projectile/Unblockable) sono nei dati ma non hanno comportamento in M4; non è chiaro se vadano ignorati o rifiutati dal validatore.
- Regole per colpire bersagli con stati speciali (già in hitstun/tumble, in hitstop, già a terra, sul muro, in dodge) e relativa gestione di combo/hitstun decay non sono descritte.
- Non è specificato cosa succede se l'attaccante muore, viene colpito o viene cancellato nello stesso frame in cui colpisce (hit già consegnato vs. annullato).

---

## Parti distruttibili (M4b): domande al proprietario del progetto

Riferimento: `combat-parts.md` (plugin `GSTNParts`) e [game-design-direction.md](game-design-direction.md) §1, §2, §5. Le ipotesi indicate sono quelle scritte nella specifica.

1. Le parti dei giocatori si rompono davvero (con effetti sul gameplay, es. braccio perso = niente pugni) o sono solo estetiche?
   Perché conta: decide se serve bloccare comandi/hitbox per i giocatori e quanto pesa sul bilanciamento PvP/co-op.
   Ipotesi nella specifica: sui giocatori le rotture sono solo estetiche di default; gli effetti di gameplay sono opzionali per kit.
   **Risposta (2026-10-22):** Le parti valgono anche per i giocatori: al calare della vita perdono pezzi, piccoli o interi arti ([direzione §1](game-design-direction.md#1-destructible-body-parts-helldivers-2-model), [direzione §5](game-design-direction.md#5-death-and-damage-presentation-crystal-people)). Resta aperto se la perdita abbia effetti di gameplay (per esempio un braccio perso) o sia solo estetica.
   **Risposta (2026-10-22):** I giocatori sono **una sola parte**: quando si rompe, muoiono ([direzione §5b](game-design-direction.md#5b-damage-inside-a-part-detachment-players-owner-input-2026-10-22)). Niente perdita di arti per i giocatori.

2. Una parte rotta può essere ripristinata (cura, respawn, checkpoint) o resta rotta fino al reset dell'entità?
   Perché conta: stato monotono = rollback e replica molto più semplici; il ripristino richiede un secondo meccanismo.
   Ipotesi nella specifica: la maschera delle parti rotte è monotona, si azzera solo al reset/respawn.

3. Quando un colpo supera il tetto di una parte, il danno in eccesso passa alla vita principale?
   Perché conta: cambia quanto conviene "finire" una parte e il numero di colpi per uccidere.
   Ipotesi nella specifica: nessun overflow; il bonus alla vita principale è un effetto esplicito per parte ("MainDamage").

4. Rompere una parte infligge un danno extra alla vita principale (es. testa) o può uccidere direttamente?
   Perché conta: definisce se esistono parti letali (decapitazione) e il ruolo dei punti deboli.
   Ipotesi nella specifica: configurabile per parte (percentuale extra o uccisione), nessun default letale.
   **Risposta (2026-10-22):** I punti deboli hanno bassa durezza e alto danno durabile, cioè trasferiscono molto alla vita principale ([direzione §1](game-design-direction.md#1-destructible-body-parts-helldivers-2-model)). Restano aperti i valori e se esistano parti letali (decapitazione).

5. La penetrazione è un valore intero a livelli (come i livelli di armatura di Helldivers) o un numero continuo?
   Perché conta: i livelli interi rendono la regola prevedibile per i designer e deterministica; il continuo dà più sfumature.
   Ipotesi nella specifica: livelli interi 0-15 per durezza e penetrazione, con tabella a fasce (sotto, pari, sopra, molto sotto).
   **Risposta (2026-10-22):** Il riferimento è Helldivers 2, dove durezza e penetrazione sono livelli di armatura ([direzione §1](game-design-direction.md#1-destructible-body-parts-helldivers-2-model)); la lettura a livelli interi resta da confermare.
   **Risposta (2026-10-22, 11:21):** La durezza è in pratica il valore di armatura di Helldivers e decide solo se il danno si applica ([direzione §2h](game-design-direction.md#2h-hardness-stagger-resistance-weight-three-dials-owner-input-2026-10-22-11181121)). Nella specifica le fasce standard sono ora tre: sopra = danno pieno, pari = circa 65 %, sotto = ricochet; le fasce non toccano più stagger e hitstop (combat-parts §3.4).

6. Un colpo molto sotto la durezza viene deviato del tutto (nessun danno alla parte, nessun hitstun) ma registra comunque?
   Perché conta: definisce il feedback dei colpi inutili (scintille, hitstop breve) e quanto si può "spammare" contro armature.
   Ipotesi nella specifica: sì, deviato = hit registrato, hitstop ridotto, niente stagger, danno principale quasi nullo.
   **Risposta (2026-10-22):** La penetrazione dell'hitbox contro la durezza della parte decide se la parte subisce danno e può essere distrutta ([direzione §1](game-design-direction.md#1-destructible-body-parts-helldivers-2-model)). Resta aperto il comportamento dei colpi molto sotto la durezza (deviazione, hitstop, hitstun).
   **Risposta (2026-10-22, tarda mattina):** Contro una parte troppo dura non c'è hitstop ma un **ricochet**: l'attaccante ritira l'arma con un montage che dipende dall'attacco ([direzione §2e](game-design-direction.md#2e-super-armor-and-ricochet-owner-input-2026-10-22-late-morning)). Nella specifica: nessun danno, nessuno stagger, nessun hitstop da entrambe le parti, montage di rinculo per attacco con ripiego su arma e kit (combat-parts.md §3.4, M4 §3b). Resta aperto se il ricochet lasci l'attaccante punibile (frame di recupero) e se il rinculo possa essere annullato in un'altra mossa.
   **Risposta (2026-10-22):** Il rinculo segue lo schema generale: una parte bloccata, poi una coda cancellabile da qualsiasi cosa, decisa dal designer ([direzione §6c](game-design-direction.md#6c-cancellable-animation-tails-owner-input-2026-10-22)); l'attaccante è punibile solo nella parte bloccata (`RecoilFrames`, M4 §7).

7. Se un colpo tocca più parti insieme, quale conta?
   Perché conta: i punti deboli devono essere colpibili in modo affidabile, ma non deve essere sempre il punto debole a vincere.
   Ipotesi nella specifica: vince la hurtbox con priorità più alta (i punti deboli ne hanno di più), a pari priorità l'indice più basso.

8. Il peso effettivo scende solo con il danno percentuale, o anche con la perdita di arti (un nemico senza gambe vola diverso)?
   Perché conta: decide se ogni parte ha una "massa persa" e quanto un nemico mutilato diventa leggero.
   Ipotesi nella specifica: curva di peso sulla percentuale (da 1.0 a circa 0.6 al tetto) più una frazione di massa persa per parte rotta, con un minimo.
   **Risposta (2026-10-22):** Il nemico ha un peso legato alla salute rimasta, che entra nel knockback ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)). Resta aperto se anche la perdita di arti alleggerisca (lettura da confermare).

9. Cosa significa esattamente "juggle": un fattore che aumenta il peso (più pesante = più difficile tenere in aria) o un decadimento che cresce con i colpi in aria?
   Perché conta: cambia la sensazione delle combo aeree e il rischio di combo infinite sui nemici.
   Ipotesi nella specifica: fattore moltiplicativo sul peso per parte e per entità, più un parametro globale "AirborneJuggleScale" (default 1).
   **Risposta (2026-10-22):** Il "juggle" è il peso per il knockback, insieme al fattore di stagger ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)), quindi un fattore sul peso e non un decadimento. Restano aperti i valori e il parametro globale per i colpi in aria.

10. Lo stagger funziona a soglia (sotto una certa forza nessuna reazione, come l'armor) o sempre proporzionale?
    Perché conta: i boss "super armor" richiedono la soglia; i nemici piccoli vanno meglio con la proporzionalità.
    Ipotesi nella specifica: soglia globale nelle regole; sotto la soglia danno e hitstop ma niente hitstun né lancio.
   **Risposta (2026-10-22, mattina):** Lo stagger funziona a soglia, e la soglia è per parte: la forza di stagger dell'attacco, che in genere scala con la potenza ma può essere impostata a parte, deve raggiungere la soglia della parte colpita ([direzione §2b](game-design-direction.md#2b-hitstun-per-part-stagger-force-hitstop-weight-owner-input-2026-10-22-morning)). Restano aperti i valori delle soglie e se, sopra la soglia, la durata dell'hitstun debba anche crescere con la forza (la specifica la fa dipendere solo dal knockback).

11. L'hitstop dell'attaccante deve poter dipendere dalla parte colpita (più lungo sul punto debole)?
    Perché conta: l'attaccante lo predice in locale su dati vecchi; usare solo dati statici evita disallineamenti.
    Ipotesi nella specifica: sì, ma solo da penetrazione e durezza/reazione statica della parte, mai da stato dinamico della vittima.
    **Risposta (2026-10-22):** Hitstun e hitstop dipendono da cosa e dove viene colpito ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)), quindi dalla parte. Resta aperto se l'hitstop dell'attaccante debba usare solo dati statici della parte (ipotesi della specifica).
   **Risposta (2026-10-22, mattina):** L'hitstop si calcola dal danno e dal knockback dell'attacco, oppure lo sceglie il designer ([direzione §2b](game-design-direction.md#2b-hitstun-per-part-stagger-force-hitstop-weight-owner-input-2026-10-22-morning)); la parte colpita lo modifica con la sua scala solo nella modalità automatica. Resta aperto se l'hitstop dell'attaccante debba usare solo dati statici (ipotesi della specifica, valida in entrambe le modalità; vedi M4.28).

12. Quando i nemici diventano afferrabili: serve un numero minimo di parti rotte, parti specifiche, o una combinazione?
    Perché conta: è il cuore dell'esempio dell'owner (nemico grosso inafferrabile finché non si rompe).
    Ipotesi nella specifica: un "gate" per set di parti (numero minimo e/o parti richieste) che proietta il tag Combat.Part.GrabLocked, usato dalle regole di grab.
    **Risposta (2026-10-22):** Un nemico grande diventa afferrabile quando gli si rompono delle parti ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)). Restano aperti il numero minimo e quali parti, o la combinazione.

13. I pezzi staccati (arti) devono poter essere raccolti, lanciati o fare danno, oppure sono solo decorazione?
    Perché conta: se sono interattivi diventano oggetti di gioco (M10) con replica e simulazione, non più estetica.
    Ipotesi nella specifica: solo estetici, mai nella simulazione; qualsiasi oggetto lanciabile è un sistema separato (M10).
    **Risposta (2026-10-22):** Dalle parti saltano via piccoli pezzi estetici o interi arti ([direzione §5](game-design-direction.md#5-death-and-damage-presentation-crystal-people)); non è detto se gli arti staccati siano interattivi. Resta aperto: la specifica li tratta come puramente estetici.

14. Vuoi che il guscio di cristallo e la pelle abbiano regole di gioco (es. cristallo esposto = più vulnerabile)?
    Perché conta: sarebbe un secondo livello di durezza per area e cambierebbe dati e bilanciamento.
    Ipotesi nella specifica: no, è solo presentazione; la vulnerabilità si ottiene con durezza e trasferimento delle parti.
    **Risposta (2026-10-22):** Cristallo all'interno e pelle umana all'esterno è l'idea di presentazione ([direzione §5](game-design-direction.md#5-death-and-damage-presentation-crystal-people)). Resta aperto se abbia regole di gameplay (cristallo esposto più vulnerabile).
    **Risposta (2026-10-22, parziale):** Il danno dentro una parte è solo un materiale, un modo diegetico di mostrarne la vita ([direzione §5b](game-design-direction.md#5b-damage-inside-a-part-detachment-players-owner-input-2026-10-22)); il distacco di una parte è una separazione vera. Resta aperto se il cristallo esposto abbia effetti di gioco.
    **Risposta (2026-10-22, prima di mezzogiorno):** I pezzi che si staccano dal materiale sono solo estetici; per i giocatori, che sono una sola parte, è tutto estetico ([direzione §2f](game-design-direction.md#2f-clarifications-owner-input-2026-10-22-before-noon)).

15. Con il listen server, accetti che l'host rompa le parti senza latenza (vantaggio dell'host) o vuoi mitigarlo (es. ritardare solo la presentazione)?
    Perché conta: la rottura di parti è molto visibile e rende evidente il vantaggio di chi ospita.
    Ipotesi nella specifica: nessuna mitigazione di default; opzione per ritardare solo la presentazione degli effetti irreversibili.

16. Quanto deve reggere il PC dell'host in scena (numero di nemici, detriti e frammenti contemporanei)?
    Perché conta: simulazione, rendering e upload condividono la macchina dell'host; i budget di detriti dipendono dalla macchina minima.
    Ipotesi nella specifica: 48 frammenti fisici, 600 particelle di cocci, 2 Geometry Collection, con riduzione automatica se il frame time sale.

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
    **Risposta (2026-10-22):** Le zone del corpo non sono solo tag di hurtbox: le parti distruttibili hanno durezza, vita propria e trasferimento alla vita principale ([direzione §1](game-design-direction.md#1-destructible-body-parts-helldivers-2-model)); la tassonomia definitiva nasce da combat-parts.md. Resta aperto quali tag di zona servano oltre al set a sei tag e come si mappano sulle parti.
19. I tag per gli eventi e i test dell'editor vanno nei plugin o restano solo nel contenuto di test del progetto?
    Perché conta: tag nei plugin vincolano chi li riusa; nell'ini del progetto restano facili da cambiare.
    Ipotesi nella specifica: nel contenuto di test del progetto (ini), non nei plugin.

### Parti distruttibili (dopo la direzione del 22/10)

20. Gli strumenti per i set di parti (durezza, penetrazione, anteprima) possono vivere nel modulo editor del plugin GSTNParts invece che in M5?
   Perché conta: mantiene il combattimento utilizzabile senza parti, ma sposta il lavoro di editor in un secondo plugin.
   Ipotesi nella specifica: sì, `GSTNPartsEditor` riusa anteprima, generatore da Physics Asset e validatori di M5 tramite hook pubblici (M5 §12b).

---

## M6 — Difesa e link: domande al proprietario del progetto

Specifica di riferimento: docs/architecture/combat-m6-defence-links.md (le ipotesi indicate sono quelle scritte lì).

### Scudo e parry

1. Lo scudo deve essere limitato (barra che si consuma e si rompe) o sempre disponibile in PvE co-op?
   Perché conta: cambia il rischio nei combattimenti contro boss e il bilanciamento di tutte le mosse nemiche.
   Ipotesi nella specifica: limitato, con barra `ShieldHealth`, rigenerazione ritardata e rottura con stordimento lungo.
   **Risposta (2026-10-22):** Nessuna implementazione standard di scudo e schivata: dipende dal kit ([direzione §6](game-design-direction.md#6-dodge-and-shield)). Resta aperto il comportamento dello scudo del kit di riferimento (barra o sempre disponibile).
   **Risposta (2026-10-22, prima di mezzogiorno):** Lo scudo esiste solo dove previsto, cioè in **PvP e non in PvE**, e funziona **come in Smash**; alcune **mosse speciali** possono fare parry ([direzione §2f](game-design-direction.md#2f-clarifications-owner-input-2026-10-22-before-noon)). Specifica M6 aggiornata di conseguenza.

2. Lo scudo è direzionale (arco davanti al personaggio) o a bolla a 360°?
   Perché conta: l'arco rende rilevante il posizionamento, la bolla è più semplice da leggere per il giocatore.
   Ipotesi nella specifica: arco di 140° sul piano di movimento, direzione fissata alla pressione (nessuna rotazione).
   **Risposta (2026-10-22):** Lo scudo dipende dal kit ([direzione §6](game-design-direction.md#6-dodge-and-shield)): direzionale o a bolla è un dato del kit, non una regola globale. Resta aperta la scelta per il kit di riferimento.

3. Il parry è una finestra nei primi frame della pressione dello scudo o ha un tasto dedicato?
   Perché conta: decide quanto è facile fare parry per sbaglio e quanto pesa la ricompensa.
   Ipotesi nella specifica: 5 frame iniziali dello scudo, con blocco se si ripreme subito dopo aver rilasciato.
   **Risposta (2026-10-22):** Scudo e schivata dipendono dal kit ([direzione §6](game-design-direction.md#6-dodge-and-shield)). Resta aperto se il parry del kit di riferimento sia una finestra iniziale o un tasto dedicato.

4. Cosa guadagna chi fa parry e cosa perde l'attaccante (stordimento, vantaggio, barra EX, riflessione proiettili)?
   Perché conta: stabilisce se il parry è una scelta rischiosa o la risposta standard.
   Ipotesi nella specifica: difensore libero prima, attaccante stordito per frame configurabili per classe di colpo, guadagno di barra; nessuna riflessione.
   **Risposta (2026-10-22):** Il premio del parry è un dato del kit ([direzione §6](game-design-direction.md#6-dodge-and-shield)). Restano aperti stordimento, vantaggio, barra EX e riflessione per il kit di riferimento.

5. Boss e attacchi speciali possono essere non parabili o non bloccabili? Le super sono bloccabili?
   Perché conta: evita che un'abilità cardine del boss sia annullata da un parry o da uno scudo.
   Ipotesi nella specifica: flag per singolo colpo; i finalizzatori delle super sono non bloccabili, gli altri colpi sì con grande danno allo scudo.

6. Lo scudo è disponibile in aria e su parete?
   Perché conta: in un platform fighter 3D la difesa aerea cambia molto il gioco verticale.
   Ipotesi nella specifica: solo a terra; in aria e su parete si usa lo schivata.
   **Risposta (2026-10-22):** In aria la difesa base è la schivata: un dive roll alla Super Mario 3D World ([direzione §6](game-design-direction.md#6-dodge-and-shield)); lo scudo, anche in aria, dipende dal kit. Resta aperto se esista uno scudo aereo o a parete.

7. In PvE i colpi dei nemici comuni consumano lo scudo come quelli dei giocatori?
   Perché conta: i minion in massa potrebbero rompere lo scudo in un attimo.
   Ipotesi nella specifica: moltiplicatore per relazione/tag configurabile nelle regole, valori da decidere.

### Tech, atterraggio, DI e SDI

8. Il tech deve esistere anche in PvE e usare il tasto dello scudo?
   Perché conta: aggiunge una difesa a costo zero ma rende gli scontri con i nemici meno punitivi.
   Ipotesi nella specifica: sì, tasto scudo, solo dopo un lancio con tumble; finestra 20 frame, blocco anti-mash 40 frame.
   **Risposta (2026-10-23):** Di base niente tech: il tech è un'abilità pluggabile che un kit o una modalità può aggiungere, ma non c'è nel set standard ([M6 §6](combat-m6-defence-links.md#6-tech-and-knockdown)).

9. Cosa succede quando si manca il tech: knockdown con rialzo automatico, rialzo con scelta (attacco, rotolata) o niente?
   Perché conta: definisce la punizione dopo un lancio e quanto tempo è in balia dei nemici.
   Ipotesi nella specifica: knockdown breve con un solo rialzo neutro; altri rialzi rimandati.
   **Risposta (2026-10-23):** Se non c'è wall splat o rimbalzo eccessivo, il personaggio fa un mini rimbalzo a terra (direzione controllabile con lo stick) e finisce a terra (downed, come in Smash). Senza input resta a terra; tenendo lo stick in una direzione rotola da quella parte; attaccando fa un getup attack (da 0 a quanti ne vuole il designer per moveset). Ci si rialza premendo salto (risposta del 23/10, 16:23); un rialzo automatico dopo un certo tempo resta un dato opzionale, spento di default.

10. DI e SDI devono valere uguale per giocatori, nemici e boss, e in tutte le modalità?
   Perché conta: la possibilità di sfuggire ai combo è parte del feeling e della difficoltà.
   Ipotesi nella specifica: curve e scale per tag della vittima nell'asset regole di modalità; i boss con scala 0.

### Prese e lanci

11. Chi può essere afferrato e chi può afferrare: giocatori tra loro, nemici verso i giocatori, giocatori verso i nemici, boss?
   Perché conta: una presa subita da un boss è un'interazione completamente diversa da una tra alleati.
   Ipotesi nella specifica: i boss hanno `Combat.Immunity.Grab`; i nemici normali possono afferrare i giocatori con la stessa pipeline.
   **Risposta (2026-10-22):** Un nemico grande non si può afferrare finché non si rompono delle parti; poi diventa afferrabile e lanciabile ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)), quindi per i nemici grandi il gate a parti e peso può sostituire l'immunità assoluta alla presa. Restano aperti nemici che afferrano i giocatori e se esistano nemici davvero inafferrabili.

12. Un alleato può afferrare un compagno (fuoco amico) o salvarlo da una presa nemica colpendo il nemico?
   Perché conta: è la parte cooperativa più visibile del sistema.
   Ipotesi nella specifica: nessuna presa tra alleati; un colpo dell'alleato sul nemico non libera da solo (da decidere).

13. Come si esce da una presa: solo mash, e quanto dura al massimo? Dipende dalla percentuale di danno?
   Perché conta: una presa lunga in PvE co-op è frustrante se il giocatore non può reagire.
   Ipotesi nella specifica: mash con durata crescente col danno, minimo e massimo; l'uscita avviene tramite il server (circa 2 frame di ritardo).

14. Quanti lanci ha un afferratore e con quali input (avanti, indietro, su, giù)? Si può afferrare in aria?
   Perché conta: determina quanti slot e animazioni servono per ogni kit.
   Ipotesi nella specifica: 4 lanci e pummel, grab con Attacco+Scudo a terra, nessuna presa aerea.
   **Risposta (2026-10-22):** Il tasto della presa è anche il tasto di interazione generale, con varianti di input (per esempio tenere premuto) ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)); M6 §8 lo recepisce: tocco = presa, pressione lunga = presa pesante, tocco da afferratore = rilascio. L'ipotesi "Attacco+Scudo" qui sopra è superata. Restano aperti il numero di lanci, gli input direzionali e la presa aerea.

15. Dopo una presa o un lancio c'è immunità alla presa? Per quanti frame e vale anche per il lanciatore?
   Perché conta: senza immunità le prese si ripetono all'infinito, con troppa non si può punire.
   Ipotesi nella specifica: immunità per frame configurabili dopo rilascio e dopo lancio, più breve ricarica per l'afferratore.

### Link, rete e sensazione di gioco

16. Come deve sentirsi una presa per chi la subisce sul proprio client (circa 1 RTT di ritardo prima dell'effetto) e per chi la fa?
   Perché conta: la specifica non predice il cambio di ruolo del giocatore che afferra, solo hitstop e feedback.
   Ipotesi nella specifica: nessuna previsione del ruolo per le prese (solo hitstop e presentazione); la telecamera di chi è afferrato segue la posa derivata.
   **Nota (2026-10-22):** Con il server di ascolto l'host che afferra non ha ritardo; i client mantengono circa 1 RTT ([direzione §6b](game-design-direction.md#6b-network-topology-listen-server-owner-input-2026-10-22)). La scelta di feel per i client resta aperta.

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

### Interazione e presa (dopo la direzione del 22/10)

22. Se un nemico e un alleato (o un oggetto) sono entrambi a portata del tasto interazione, quale ha la precedenza?
   Perché conta: lo stesso tasto fa afferrare, montare o raccogliere; un errore di contesto è frustrante.
   Ipotesi nella specifica: vince il più vicino alla direzione di sguardo; a parità, ordine dati: afferra, raccogli, monta.
   *23/10: ora vale un solo bersaglio, quello puntato ([interaction.md](interaction.md)); la precedenza tra significati resta la domanda INT.5.*
   Nota (2026-10-22, dopo la risposta a M6.23): ora tocco e pressione lunga hanno significati diversi. Con un solo bersaglio alla volta la presa parte subito alla pressione; un'alternativa è proiettare insieme presa e raccolta e far partire la presa al rilascio del tocco, così nemico e oggetto non competono più, al costo di qualche frame di ritardo sulla presa.
   
23. Tenere premuto il tasto di interazione deve dare la presa pesante (più lenta, per bersagli più pesanti)?
   Perché conta: definisce il significato della pressione lunga e quanto si può afferrare.
   Ipotesi nella specifica: tocco = presa normale, pressione lunga = presa pesante con soglia di peso maggiore; rilascio = tocco da afferratore.
   **Risposta (2026-10-22, 10:34):** Per ora **tocco = presa, pressione lunga = raccogliere oggetti** ([direzione §2g](game-design-direction.md#2g-interact-button-dodge-windows-designer-tags-blueprint-replaceable-content-owner-input-2026-10-22-1034-and-1036)). Nella specifica la presa pesante è tolta dal set standard (M6 §8, M10 §8); un kit può aggiungerne una su un altro tasto.
   
24. Un nemico può essere afferrabile per peso (si alleggerisce con la salute) oltre che per parti rotte, oppure basta il meccanismo delle parti?
   Perché conta: senza parti (o con nemici semplici) serve comunque una regola di afferrabilità.
   Ipotesi nella specifica: entrambe le regole disponibili e combinabili (devono passare tutte quelle configurate).


### Scudo come in Smash e mosse parry (dopo la direzione del 22/10)

25. In Smash Ultimate il parry si fa **rilasciando** lo scudo pochi frame prima del colpo. Va bene così, o preferisci il powershield a pressione dei capitoli precedenti?
   Perché conta: cambia il gesto del giocatore e quanto è facile fare parry per sbaglio.
   Ipotesi nella specifica: rilascio, finestra di 5 frame (`Anchor = Release`); la pressione resta un'opzione per kit o modalità (M6 §5.4).

26. Dopo un parry (dello scudo o di una mossa) l'attaccante resta davvero stordito, o il parry dà solo un vantaggio di frame al difensore come in Ultimate? Il difensore con una mossa parry prosegue nel contrattacco?
   Perché conta: decide se il parry è una punizione forte o solo un vantaggio, e quanto pesa una mossa contrattacco rispetto a uno scudo.
   Ipotesi nella specifica: stordimento dell'attaccante configurabile per classe di colpo (0 = solo vantaggio, come Ultimate); con lo scudo il difensore torna libero prima, con una mossa continua la sua animazione e può annullarla in un contrattacco tramite la condizione `OnParry`.

27. In PvE le mosse con finestra di parry possono parare gli attacchi dei boss e dei nemici? Quali colpi restano non parabili?
   Perché conta: un contrattacco troppo generoso può banalizzare uno scontro, mentre uno troppo limitato rende inutili le mosse parry in PvE.
   Ipotesi nella specifica: sì, vale in ogni modalità; i colpi simbolo dei boss sono non parabili tramite il flag `bParryable = false` o il tag `Combat.Immunity.Parry`, deciso per singolo colpo nei contenuti.

28. Lo scudo è attivato per modalità di gioco (PvP sì, PvE no) o dipende anche da chi colpisce? Nelle modalità PvP con minion o pericoli ambientali, questi colpi consumano lo scudo come quelli dei giocatori?
   Perché conta: stabilisce se serve un solo interruttore per modalità o anche regole per relazione, e se i minion in massa possono rompere lo scudo in un attimo.
   Ipotesi nella specifica: un solo interruttore per modalità (`UGSTN_DefenceRules::bShieldEnabled`, più lo scudo effettivamente presente nel kit); nelle modalità PvP con minion un moltiplicatore per relazione nelle regole (valori da decidere).

29. Va bene che la bolla dello scudo sia una hurtbox aggiuntiva, così blocca anche i colpi che toccano solo la bolla e non il corpo (fedele a Smash), anche se richiede un piccolo intervento sul rilevamento colpi di M4?
   Perché conta: l'alternativa più semplice (testare solo il punto di contatto sul corpo) non permette di bloccare un colpo che sfiora soltanto la bolla, quindi lo scudo risulterebbe più piccolo di quello che si vede.
   Ipotesi nella specifica: hurtbox aggiuntiva con priorità sopra il corpo (la presa la ignora e colpisce il corpo, gli shield poke colpiscono le parti esposte), verificata dallo spike 2.5; se lo spike non passa si torna al test sul punto di contatto e ti avviso.

---

## M7 — Piggyback e mosse in coppia: domande al project owner

### Controlli e ruoli

1. Per montare/smontare/lasciare cadere va bene un solo tasto "Interact" con significato deciso dal ruolo (montare, smontare, drop)?
   Perché conta: decide i binding, le collisioni di input e quanti tasti servono sul pad.
   Ipotesi nella specifica: sì, un tasto; Drop = Interact + accovacciarsi; il cooldown di 30 frame evita il re-mount immediato.
   **Risposta (2026-10-22):** Sì: un solo tasto, quello della presa, è anche il tasto di interazione generale ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)). Il montaggio è un significato contestuale scelto dal contesto (M6 §8, tag `Interact.Target.Mount`). Resta aperta la precedenza tra alleato e nemico a portata (vedi M6.22).
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
   **Nota (2026-10-22):** Con il server di ascolto, se l'host è il rider il montaggio è istantaneo e senza rifiuto percepibile; per i client resta valido il compromesso descritto ([direzione §6b](game-design-direction.md#6b-network-topology-listen-server-owner-input-2026-10-22)).
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
    **Nota (2026-10-22):** Con il server di ascolto, se il partner è l'host non c'è ritardo di rete sulla sua parte della mossa; per un partner client resta valida la domanda ([direzione §6b](game-design-direction.md#6b-network-topology-listen-server-owner-input-2026-10-22)).

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
   Ipotesi nella specifica: 40 richiesti e tipici, 60 di progetto, 80 come picco sull'host, 100 stretch solo con server dedicato; **150 solo come popolazione**, con al massimo circa 60 nemici svegli insieme (gli altri dormienti o lontani, a costo quasi nullo). A 150 tutti svegli l'host spenderebbe circa 6,4 ms di frame solo per i nemici e circa 61 KB/s di upload con 3 client (143 KB/s con 7): irrealistico su un server di ascolto (M8 §4d.1, §8.3, §11; aggiornata dopo la ricerca del 22/10, [many-enemies-replication.md](many-enemies-replication.md) §3.3).

2. Quanti tipi diversi di minion servono al lancio e quanto devono differire (solo statistiche, o mosse e comportamento diversi)?
   Perché conta: definisce quante `UGSTN_MinionDefinition` e quanti task di AI scrivere e testare.
   Ipotesi nella specifica: 2 tipi di test (grunt, heavy); tutto il resto e contenuto da designer.

3. Esistono minion volanti, a distanza (proiettili) o di grandi dimensioni?
   Perché conta: i volanti richiedono un altro backend di movimento, i proiettili un sistema di hitbox `Projectile` che M4 non ha ancora.
   Ipotesi nella specifica: solo minion a terra e corpo a corpo; volanti possibili in seguito col backend Mover per tipo.
   **Risposta (2026-10-22):** I nemici di grandi dimensioni esistono (non afferrabili finché non perdono parti) ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)). Restano aperti volanti e a distanza.

### Combattimento e reazioni

4. I minion hanno punti vita (muoiono a 0) o la percentuale in stile platform fighter (volano sempre piu lontano)?
   Perché conta: cambia modello di knockback, interfaccia e come si "uccide" un nemico.
   Ipotesi nella specifica: punti vita, con knockback che cresce con la frazione di vita persa (`KnockbackModel_HealthFraction`).
   **Risposta (2026-10-22):** Percentuale e tetto insieme: la salute è una percentuale che causa la distruzione della parte o del nemico oltre un massimo, con peso legato alla salute rimasta ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)); i nemici hanno anche parti con vita propria ([direzione §1](game-design-direction.md#1-destructible-body-parts-helldivers-2-model)). La specifica a punti vita con frazione persa va allineata. Resta aperto il valore del tetto per tipo di minion.

5. Quanto devono volare via i minion colpiti: sempre lanciati, solo stordimento, o dipende dal colpo? Possono rimbalzare su muri e pavimento (tech, juggle)?
   Perché conta: lanci lunghi richiedono collisione e movimento piu robusti e piu tempo di simulazione per minion.
   Ipotesi nella specifica: lancio balistico con atterraggio e knockdown, nessun tech, nessun rimbalzo avanzato.
   **Risposta (2026-10-22):** Il peso scende con la salute (più danno, più volo), stagger e juggle dipendono da cosa e dove si colpisce, e l'impatto con superfici dà splat, rimbalzo o distruzione secondo il designer ([direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight), [direzione §4](game-design-direction.md#4-impacts-with-surfaces)). Restano aperti se ogni colpo lanci sempre il minion e se esistano tech o juggle avanzati.
   **Risposta (2026-10-22, mattina):** Non sempre: il peso decide se il minion vola via, viene solo staggerato sul posto o viene spinto indietro, e se entra in hitstun dipende dalla parte colpita ([direzione §2b](game-design-direction.md#2b-hitstun-per-part-stagger-force-hitstop-weight-owner-input-2026-10-22-morning)). Valgono le stesse regole dei combattenti (M8 §4d.2) e il livello è codificato in `ActionState`. Restano aperti i valori delle soglie e se esistano tech o juggle avanzati.

6. I minion possono avere armatura/poise (non interrompibili da colpi leggeri) o parare, schivare, usare scudo?
   Perché conta: la difesa dei minion e M6; armatura e invece gia gestita dalla pipeline.
   Ipotesi nella specifica: solo armatura (`PoiseHits`); niente scudo, parata o schivata nei minion.
   **Risposta (2026-10-22):** I nemici hanno parti con durezza e fattore di stagger ([direzione §1](game-design-direction.md#1-destructible-body-parts-helldivers-2-model), [direzione §2](game-design-direction.md#2-health-as-a-capped-percentage-and-weight)), che fa da armatura. Restano aperti scudo, parata e schivata dei minion (schivata e scudo dipendono dal kit, [direzione §6](game-design-direction.md#6-dodge-and-shield)).
   **Risposta (2026-10-22, mattina):** L'armatura di un minion è la soglia di stagger della parte colpita o del tipo ([direzione §2b](game-design-direction.md#2b-hitstun-per-part-stagger-force-hitstop-weight-owner-input-2026-10-22-morning)); `PoiseHits` resta come armatura a conteggio. Restano aperti scudo, parata e schivata.
   **Risposta (2026-10-22, prima di mezzogiorno):** Armatura e poise sono soglia di stagger e peso rimanente, quindi nessun contatore separato ([direzione §2f](game-design-direction.md#2f-clarifications-owner-input-2026-10-22-before-noon)).

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
    **Risposta (2026-10-22):** I nemici morti vanno in pezzi: cristallo all'interno, rivestito di pelle, con pezzi che saltano via al calare della vita ([direzione §5](game-design-direction.md#5-death-and-damage-presentation-crystal-people)). Restano aperti cadavere, drop, punteggio e se i frammenti contino per il limite di entità.

### Giocatori, piattaforme, prestazioni

14. Quanti giocatori per partita co-op (2? 4? di piu) e su quale macchina gira l'host del server di ascolto (il PC di casa di un amico)?
    Perché conta: il costo server cresce con i giocatori (query e rollback) e il budget in ms dipende dalla macchina.
    Ipotesi nella specifica: fino a 4 giocatori, host paragonabile al PC di sviluppo, che gioca e fa da server insieme; da confermare con la misura.
    **Risposta (2026-10-22):** Il server di ascolto è deciso: un amico ospita e gli altri sono client ([direzione §6b](game-design-direction.md#6b-network-topology-listen-server-owner-input-2026-10-22)). Restano aperti il numero di client e l'hardware tipico dell'host (vedi M8.20).

15. Piattaforme client e requisiti minimi (PC solo? console? 60 fps?), e quanti minion visibili devono girare a 60 fps?
    Perché conta: il costo delle animazioni dei burattini puo superare quello del server e impone LOD e condivisione animazioni.
    Ipotesi nella specifica: PC, 60 fps, obiettivo 3 ms di game thread per 60 burattini; tetto `MaxVisiblePuppets` = 60 burattini con scheletro visibili (gli altri istanziati o nascosti), budget di animazione 1 ms sui client e 1,5-2 ms sull'host (M8 §8.4, spike 7); aggiornata dopo la ricerca del 22/10.

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
    **Risposta (2026-10-22):** Sì, vale per tutti i plugin: riusabili da soli in progetti indipendenti ([modularità §3.7](modularity-and-tags.md#37-reuse-in-independent-projects-owner-direction-2026-10-22)).

### Server di ascolto (dopo la direzione del 22/10)

20. Con il server di ascolto, quanti client si connettono in genere all'host (2, 3, 7)? E che upload ha l'host di casa (circa)?
   Perché conta: la banda dell'host si moltiplica per i client; decide il tetto per client dei minion (circa 6 KB/s a 40 nemici) e se serve il tetto totale dell'host.
   Ipotesi nella specifica: fino a 3 client, host con almeno 5 Mbit/s di upload, tetto totale soft di 40 KB/s (M8 §4d.1). Con 40 nemici svegli e 20 B per aggiornamento sono 5,8 KB/s per client, cioè circa 17 KB/s con 3 client e 40 KB/s con 7 (sul limite del tetto); un upload di 1 Mbit/s (125 KB/s) non regge 150 svegli con 3 client (circa 61 KB/s) insieme al traffico dei giocatori (M8 §8.3; aggiornata dopo la ricerca del 22/10).
   
21. Con 40 minion, la CPU dell'host deve restare libera per il suo gioco: va bene ridurre i minion attivi sull'host (per esempio 32 attivi in combattimento invece di 48) se il frame non basta?
   Perché conta: decide se il numero richiesto (40) è un obiettivo fisso o un tetto che scende in base alla macchina dell'host.
   Ipotesi nella specifica: sì, si riduce prima `MaxActiveT0` (da 48 a 32), poi la frequenza dei T0 a 10 Hz, poi il tetto dei puppet visibili (`MaxVisiblePuppets`) e il loro LOD; il massimo N misurato sull'host viene registrato onestamente (M8 §4d.1; ordine aggiornato dopo la ricerca del 22/10).

### Replicazione di molti nemici e nemici-piattaforma (dopo la ricerca del 22/10)

Specifica di riferimento: `docs/architecture/many-enemies-replication.md` (ricerca e proposta; i numeri citati sono quelli del suo §3.3 e §4) e `docs/architecture/combat-m8-minions.md` (§4d.1, §4d.5, §8, §11). La ricerca non è una risposta del proprietario: le ipotesi sono quelle ora usate nella specifica e non c'è nessuna **Risposta** da riportare. Le domande della ricerca su quanti nemici devono essere svegli sull'host (popolazione contro nemici attivi insieme) e sull'upload minimo dell'host sono già la 1 e la 20 di questa sezione: non sono ripetute, ma le loro ipotesi sono state aggiornate qui sopra.

22. Quali nemici possono fare da piattaforma: solo i fighter (boss ed elite) o anche alcuni minion grandi? Quanti contemporaneamente?
   Perché conta: i minion dello sciame non sono attori sui client e non possono essere una base su cui il giocatore cammina; per un minion-piattaforma serve un piccolo attore di appoggio replicato, che costa banda e CPU in più (circa 20-25 B per aggiornamento a 30 Hz per ciascuno).
   Ipotesi nella specifica: sono piattaforme i fighter e al massimo 8 minion-piattaforma vivi insieme (`gstn.Platform.MaxActors`), di tipi contrassegnati da un flag nei dati; i minion dello sciame mai. La piattaforma è un collider rigido separato dalla mesh animata (mai un osso o la capsula) (M8 §4d.5).

23. A che velocità si muove un nemico-piattaforma mentre un giocatore ci sale o ci cammina sopra? Si può saltare su un nemico che sta correndo, oppure deve rallentare o fare un'animazione di "appoggio" quando un giocatore si avvicina?
   Perché conta: l'errore di atterraggio dei client cresce con velocità × (ping + circa 100 ms di interpolazione): a 300 cm/s sono 48-66 cm. Fino a circa 150-300 cm/s lo schema semplice (il giocatore si muove nello spazio locale della base) basta; sopra serve la compensazione del ritardo della piattaforma, che richiede lavoro in più.
   Ipotesi nella specifica: velocità fino a 300 cm/s, con rallentamento automatico della piattaforma (`MaxSpeedWhileOccupied`) e animazione di appoggio quando un giocatore è vicino o sopra; la compensazione del ritardo si aggiunge solo se le misure (spike P1/P2) la richiedono.

24. Cosa succede ai giocatori sopra una piattaforma quando viene colpita forte, lanciata, uccisa, afferrata, o quando si rompe la parte su cui stanno?
   Perché conta: il server decide la perdita della base e il client lo scopre con ritardo (una correzione). Con un breve avviso visibile prima (circa 300 ms) la correzione è quasi invisibile; senza, il giocatore può cadere o scattare.
   Ipotesi nella specifica: lo stagger e il knockback leggero non la disattivano; lancio, morte, rottura o presa la disattivano dopo un avviso di circa 20 frame (`DropGraceFrames`); il giocatore cade conservando la velocità della base e non viene mai rimosso in silenzio.

25. I nemici che non sono piattaforme devono bloccare fisicamente i giocatori, oppure i giocatori li attraversano e il contatto conta solo per i colpi? (Estende la 18, che riguarda solo i minion.)
   Perché conta: una collisione giocatore-nemico predetta sul client e simulata sul server è una fonte continua di correzioni (il nemico sul client è circa 160-220 ms indietro). Attraversabile significa zero correzioni, ma meno sensazione di massa.
   Ipotesi nella specifica: solo le piattaforme sono solide per i giocatori; gli altri nemici non li bloccano (si separano solo tra loro) e i colpi usano gli hitbox, non la fisica.

26. Quanto ritardo visivo sui nemici lontani e quanta perdita di dettaglio sono accettabili? In particolare: nemici lontani o numerosi con animazioni ridotte o condivise, istanze senza scheletro oltre una certa distanza, e posizioni dei minion in secondo piano meno precise (anche 40 cm di errore). (Approfondisce la 15 e la 16.)
   Perché conta: è la leva più efficace per tenere frame e banda dell'host sotto controllo (limite di puppet visibili, animazioni condivise, aggiornamenti più rari ai lontani), ma cambia l'aspetto di una folla.
   Ipotesi nella specifica: massimo circa 60 puppet con scheletro visibili; gli altri istanziati o nascosti; i nemici in combattimento (T0) aggiornati a 15 Hz con lo stesso ritardo di interpolazione dei giocatori (100 ms), gli altri tra 0,5 e 5 Hz (T1 5 Hz o 2-3 Hz, T2 1 Hz o meno) con posizione stimata sul client (M8 §7, §8.3, §8.4).

27. Va bene restare per ora sulla replicazione classica di Unreal (pacchetti di nemici con FastArray, push model e dormancy) e rimandare Iris finché non risulta verificato con Network Prediction e Mover del nostro fork in 5.8?
   Perché conta: Iris è dichiarato pronto per la produzione in 5.8 ma la compatibilità con NPP e Mover è documentata solo per la versione di sviluppo, e attivarlo è un interruttore per tutto il progetto, compresi i giocatori. Per il nostro numero di client il guadagno atteso è piccolo.
   Ipotesi nella specifica: replicazione classica per M8; Iris solo come esperimento isolato (spike R3), da adottare se non rompe nessuno scenario di NPP/Mover e riduce di almeno il 25% CPU o banda di replicazione dell'host.

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
   **Risposta (2026-10-22):** Nemici afferrati e lanciati addosso ad altri nemici sono voluti, anche come proiettili con hitbox ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)). Gli altri tipi (consumabili, armi raccolte, oggetti di scena) non sono stati commentati: restano aperti.

2. Quante classi di taglia servono e cosa cambia tra l'una e l'altra (velocità, salto, comandi permessi)?
   Perché conta: ogni taglia moltiplica i binding e il contenuto.
   Ipotesi nella specifica: Light, Medium, Heavy, Huge; Heavy permette solo camminare, posare, lanciare.
   **Risposta (2026-10-22):** La taglia conta per la presa: un nemico troppo grande non si afferra finché non perde parti ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)). Restano aperti il numero di classi di taglia e cosa cambia nei comandi permessi.

3. Le armi raccolte sostituiscono l'arma primaria mentre le tieni, hanno una durata (colpi/usi) e poi si rompono o diventano oggetti lanciabili?
   Perché conta: cambia come si progettano i binding e la durabilità.
   Ipotesi nella specifica: sostituiscono le mosse d'attacco, durabilità a cariche; a zero si rompono o diventano props Light.

### Raccogliere, posare, lanciare

4. Raccogliere è un comando dedicato (es. tasto Afferra) o contestuale all'attacco vicino a un oggetto?
   Perché conta: occupa o no uno slot e può confliggere con le mosse esistenti.
   Ipotesi nella specifica: comando dedicato `Command.Link.Pickup`, senza pattern di default, assegnato dal kit.
   **Risposta (2026-10-22):** Contestuale: il tasto della presa è anche il tasto di interazione, con tocco e pressione lunga come varianti ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)); M6 §8 e M10 §8 lo recepiscono con il tag `Interact.Target.Pickup`. Resta aperta la precedenza tra oggetto, nemico e alleato (vedi M6.22).

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
    **Risposta (2026-10-22):** Un lancio contro una superficie può causare splat, rimbalzo o distruzione a seconda del designer ([direzione §4](game-design-direction.md#4-impacts-with-surfaces)); M4 §7b ne fissa il meccanismo. Resta aperto chi decide (attacco, vittima o superficie, vedi M4.20).

10. Un proiettile può colpire più bersagli (perforare)? Un nemico colpito può essere ricolpito dallo stesso oggetto dopo un rimbalzo?
    Perché conta: gruppi di colpo e bilanciamento delle folle.
    Ipotesi nella specifica: un colpo per bersaglio per lancio; `MaxHits` per perforare.

11. Gli alleati possono essere colpiti da oggetti lanciati (fuoco amico)? Può dipendere dalla modalità?
    Perché conta: in co-op cambia molto la tensione e il design degli attacchi ad area.
    Ipotesi nella specifica: alleati ignorati di default, variante di fuoco amico per modalità.
    **Risposta (2026-10-22):** I nemici lanciati colpiscono altri nemici ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)). Resta aperto se alleati giocatori e oggetti di scena subiscano fuoco amico.

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
    **Risposta (2026-10-22):** Sì: afferrare e lanciare nemici è una meccanica voluta, con il nemico lanciato come proiettile con hitbox ([direzione §3](game-design-direction.md#3-grabs-interaction-and-throwing-enemies)); M10 §6b la tratta come caso dello stesso sistema. Restano aperti i dettagli di presa al volo e rimbalzi (vedi M10.20-22).

### Limiti e contenuti

18. Quanti oggetti attivi nello stesso momento (in volo o a terra) devono essere supportati, e gli oggetti a terra tornano dopo un po'?
    Perché conta: dimensiona pool, banda e budget del server.
    Ipotesi nella specifica: 32 proiettili attivi, oggetti a riposo dormienti; respawn per definizione.

19. Va bene che gli oggetti tenuti siano un plugin separato (`GSTNHeld`, che dipende da `GSTNCombat` e `GSTNLink`) che si può rimuovere senza toccare il resto?
    Perché conta: M10 usa il registro aperto dei tipi di link, `FGSTN_EntityRef` e il canale mano di M6 (§7.0), non hook propri in `GSTNCombat`; la modularità resta garantita.
    Ipotesi nella specifica: sì; i tag `Combat.Link.Type.Held` e dei ruoli Holder/HeldObject li dichiara `GSTNHeld` (domanda M6.21), così `GSTNCombat` e `GSTNLink` non lo conoscono.

### Nemici lanciati (dopo la direzione del 22/10)

20. Un nemico grande lanciato è un normale combattente (entità NP) con un hitbox generato dal suo stesso tick; va bene o i nemici afferrabili devono essere solo minion?
   Perché conta: i combattenti NP lanciati richiedono un seam "lancio con credito" in M4/M6; solo minion è molto più semplice.
   Ipotesi nella specifica: entrambi, ma M10d consegna prima i minion e poi i combattenti se il seam è pronto (M10 §6b).
   
21. Chi decide cosa succede quando un nemico lanciato sbatte contro una superficie (splat, rimbalzo, distruzione): il colpo, l'entità o la superficie?
   Perché conta: M10 rimanda alle regole di M4 e non le ridefinisce; senza risposta le due specifiche potrebbero divergere.
   Ipotesi nella specifica: stesse regole di un nemico lanciato da un colpo (M4); il lancio di oggetti-prop mantiene `ImpactBehaviour` dei dati dell'oggetto.
   
22. Il nemico lanciato può essere danneggiato mentre è in mano o in volo, e può colpire i suoi stessi alleati (altri nemici)?
   Perché conta: decide `bHittableWhileHeld` e le regole di fuoco amico durante `ThrownBody`.
   Ipotesi nella specifica: non colpibile mentre è in mano o in volo; colpisce qualsiasi nemico del lanciatore ignorando la squadra, credito al lanciatore (M10 §6b).
   

---

## Superfici di attraversamento sulle parti dei nemici (22/10 sera)

Richiesta del proprietario (22/10): "alcuni nemici potrebbero avere porzioni di corpo su cui è possibile fare wall slide, oppure parti come trampolini etc...".
Specifica di riferimento: `docs/architecture/traversal-surfaces.md`, piano `docs/architecture/plans/traversal-surfaces-plan.md` (T0..T9). Tutto ciò che non è elencato qui è deciso nella specifica come *Proposal* (regola di auto-approvazione del 22/10). Impostazione di fondo: una parte su cui si fa wall slide o che fa da trampolino è una **forma-piattaforma** del nemico (lo stesso collider rigido e separato dalla mesh di M8 §4d.5), con un "comportamento di superficie" come dato statico; la rottura della parte spegne la forma. Niente stato di simulazione nuovo.

TS.1. Va bene un piccolo plugin a sé, `GSTNSurface` (solo `GSTNSurfaceCore`, senza GAS/NP/Mover), proprietario della radice di tag `Surface.*` e del contratto dati (comportamenti, parametri di lancio, interfaccia `IGSTN_SurfaceProvider`), da cui dipendono `GSTNCombatCore` (H11/H11b), `GSTNPlatform` e il codice di wall movement? Comporta spostare in quel plugin `Surface.Type.*`, `IGSTN_ImpactSurface`/`IGSTN_LaunchSurface` e `FGSTN_SurfaceLaunchParams` previsti in H11/H11b (non ancora iniziati).
   Perché conta: tre consumatori indipendenti (impatti, wall movement, piattaforme) devono leggere gli stessi dati e un solo plugin deve possedere i tag; metterli in `GSTNLevelObjects` o in `GSTNPlatform` costringerebbe il wall movement a dipendere da combat o da Mover-piattaforme.
   Risposta consigliata: sì, `GSTNSurface` separato; costo piccolo se deciso prima che H11 parta.
   **Risposta (22/10, 22:15):** va bene. → `GSTNSurface` approvato; T1 prima di H11.

TS.2. Autorizzi una modifica al core del wall movement (come `IsMovementLocked`): un campo `FGSTN_WallSurfaceFilter` (query di tag, default vuoto = comportamento attuale) in `FGSTN_WallCheckSettings`, valutato in `IsHitOnValidWall` e nello sweep della transizione di ingresso, per dire "questo muro non si può scalare/scivolare" o "solo arrampicata"?
   Perché conta: senza, le parti dei nemici funzionano lo stesso (solo risposte di collisione, T2), ma un muro **statico** che blocca il giocatore senza permettere il wall slide, o una superficie solo-arrampicata, non sono esprimibili.
   Risposta consigliata: sì (T5), è un solo punto di estensione con default identico a oggi.
   **Risposta (22/10, 22:14):** sì: alcune superfici sono wall-slidabili e altre no, va inserito. → T5 approvato.

TS.3. Quali nemici possono avere parti-superficie: solo quelli che possono già fare da piattaforma (fighter: boss ed elite, più al massimo 8 minion-piattaforma vivi con attore di appoggio), oppure vuoi che anche minion dello sciame abbiano un fianco scivolabile?
   Perché conta: i minion dello sciame non hanno un attore sui client e nessun riferimento stabile come base del Mover; aprirli a questa meccanica significa più attori di appoggio e banda.
   Risposta consigliata: stesso insieme delle piattaforme, stesso tetto di 8; i minion dello sciame mai.

TS.4. Mentre un giocatore scivola o si arrampica su un nemico, il nemico deve rallentare / fare l'animazione di "appoggio" come per le piattaforme (`MaxSpeedWhileOccupied`, brace), oppure resta libero di muoversi a piena velocità?
   Perché conta: l'errore di aggancio sui client cresce con velocità × (ping + ~100 ms); rallentare è la leva più economica ed è anche una scelta di design (un boss che "sente" il giocatore addosso).
   Risposta consigliata: sì, il giocatore in wall slide conta come occupante: stesso limitatore e stessa animazione delle piattaforme.
   **Risposta (22/10, 21:58):** dipende dalla parte e dal nemico. → Dato per parte (`FGSTN_OccupantResponse` su ogni forma), con un default per nemico e un evento Blueprint; senza risposta il nemico si muove a piena velocità.

TS.5. Quando la parte su cui il giocatore sta scivolando si rompe (o il nemico viene lanciato, muore, viene afferrato): avviso visibile di ~20 frame e poi il giocatore cade conservando la velocità del nemico, oppure distacco immediato? E il giocatore può "cavalcare" il pezzo staccato?
   Perché conta: la perdita della base arriva ai client con un RTT di ritardo (una correzione); l'avviso la rende quasi invisibile. Il pezzo staccato è solo presentazione (parti §6.2) e farne una superficie lo renderebbe parte della simulazione.
   Risposta consigliata: avviso di `DropGraceFrames` poi caduta con la velocità della base; i pezzi staccati non sono mai superfici.

TS.6. Parti-trampolino: la direzione di lancio segue la posa della parte in quel frame (nemico inclinato → lancio inclinato) e si somma la velocità del nemico? Una vittima in tumble scagliata contro la parte rimbalza (riga `Surface.Type.Trampoline` di H11)? Colpire la parte con un attacco **non** lancia mai l'attaccante?
   Perché conta: fissa cosa è "data della superficie" e cosa è contatto; evita casi speciali nel codice degli impatti.
   Risposta consigliata: sì, sì, sì (flag `bAddSurfaceVelocity` nei parametri di lancio, default attivo).

TS.7. Chi usa queste superfici: solo giocatori e fighter (entità NP con Mover), oppure anche i minion devono poter essere lanciati da una parte-trampolino di un altro nemico o scivolare su di essa?
   Perché conta: i minion non hanno modalità di muro e il loro movimento leggero (M8) ha un lancio proprio; per i trampolini **statici** il percorso esiste già (H11), per quelli in movimento no.
   Risposta consigliata: giocatori e fighter; i minion solo sui trampolini statici, per ora.

TS.8. Quali comportamenti vuoi nella prima consegna: `WallSlide`, `WallClimb`, `Launch` (trampolino), `Stand` (piattaforma) sono quelli pianificati; `Slippery` (scivolo più veloce, niente presa) e `OneWay` restano tag riservati senza lettore. Ne mancano altri importanti (es. "appiccicoso", "danno al contatto")?
   Perché conta: ogni comportamento con logica propria è un lettore in più (sottoclasse di modalità o di passo di pipeline); i tag riservati costano zero.
   Risposta consigliata: i quattro pianificati; gli altri come tag riservati finché un livello non li chiede.

---

## M11 — Kit in gioco, armi di supporto, mod, zaini (23/10)

Specifica: [combat-loadout-mods.md](combat-loadout-mods.md). Già deciso da te il 23/10: kit = statistiche + moveset; il kit si cambia a metà partita raccogliendone uno; l'arma di supporto eredita le statistiche del kit; per ora niente cambio dell'arma primaria separato dal kit; mod estetiche che possono toccare speciali e moveset; speciali intercambiabili; impianto sulla schiena; zaini modulari attaccati sopra l'impianto; quasi tutto è un'abilità pluggabile.

1. Dove si trovano i kit da raccogliere: sparsi nei livelli, lanciati come rifornimento, lasciati dai nemici, o tutti questi a seconda della modalità?
   Perché conta: decide chi crea gli oggetti raccoglibili (la modalità o il livello) e quanti ne servono.
   Ipotesi nella specifica: li crea la modalità (livelli: piazzati dal designer; spedizione: rifornimenti), con lo stesso oggetto raccoglibile ([M11 §12](combat-loadout-mods.md#12-world-pickups-and-drops-gstnpickups)).

2. Cambiando kit, il danno già subito resta? Se il nuovo kit ha un tetto diverso, si conserva la proporzione (es. 50 % del tetto)?
   Perché conta: senza una regola, cambiare kit potrebbe "curare" o uccidere di colpo.
   Ipotesi nella specifica: si conserva la proporzione del tetto; le barre (meter) restano, limitate ai nuovi massimi ([M11 §4](combat-loadout-mods.md#4-changing-kit-mid-match-pickup)).

3. Il vecchio kit cade a terra e può raccoglierlo un compagno?
   Perché conta: è comodo in co-op (scambio di kit) ma aumenta gli oggetti nel mondo.
   Ipotesi nella specifica: sì, cade dove si trovava il giocatore, con le sue mod.

4. Il kit raccolto porta con sé le sue mod e speciali, o usa la configurazione che il giocatore ha salvato per quel kit?
   Perché conta: nel primo caso le mod si trovano nel mondo; nel secondo ogni giocatore ritrova il "suo" kit.
   Ipotesi nella specifica: la configurazione salvata del giocatore, se esiste; altrimenti quella dell'oggetto raccolto.

5. Cambiando kit, arma di supporto e zaino restano?
   Perché conta: lo zaino è indipendente dal kit (si attacca all'impianto con un connettore standard), l'arma di supporto potrebbe non essere compatibile.
   Ipotesi nella specifica: lo zaino resta sempre; l'arma di supporto resta se il nuovo kit la accetta, altrimenti cade a terra.

6. Un'arma di supporto può comunque modificare le statistiche (es. molto pesante = più lento), o le statistiche del kit restano sempre identiche?
   Perché conta: hai detto che le statistiche si ereditano dal kit; resta da capire se è una regola assoluta.
   Ipotesi nella specifica: di base nessuna modifica; il meccanismo resta disponibile al designer con un avviso del validatore ([M11 §6](combat-loadout-mods.md#6-support-weapons)).

7. Alla morte arma di supporto e zaino cadono a terra e si possono recuperare, come in Helldivers 2?
   Perché conta: cambia il flusso di rinforzo e il numero di oggetti nel mondo.
   Ipotesi nella specifica: sì, cadono dove si muore.

8. Le armi di supporto hanno munizioni, ricariche o tempi di ricarica? Uno zaino può fornire munizioni a un'arma di supporto (ricarica di squadra)?
   Perché conta: senza munizioni le armi di supporto sono solo un secondo moveset; con munizioni servono attributi e UI.
   Ipotesi nella specifica: se servono, sono costi e cooldown delle abilità (GAS), nessun sistema nuovo; lo zaino può essere compatibile con certe armi di supporto.

9. Le speciali si cambiano solo prima della partita, o anche durante (checkpoint, punti di rifornimento)?
   Perché conta: cambiarle durante la partita passa dallo stesso percorso del cambio di kit (ricalcolo delle abilità).
   Ipotesi nella specifica: prima della partita e tra un livello e l'altro; durante solo dove la modalità lo consente.

10. Le mod valgono anche per armi di supporto e zaini, o solo per il kit (arma primaria compresa)?
    Perché conta: la definizione delle mod lo permette già come dato; decide quanti contenuti servono.
    Ipotesi nella specifica: solo kit e arma primaria per ora.

11. Lo zaino si usa con un tasto dedicato? E sono ammessi zaini che proiettano una cupola energetica in PvE (lo scudo come azione difensiva resta solo PvP)?
    Perché conta: un tasto in più sul controller; la cupola è un oggetto, non lo scudo di M6, ma potrebbe sembrare in contrasto con "niente scudo in PvE".
    Ipotesi nella specifica: comando `Command.Backpack.Use` su un tasto da decidere; la cupola è ammessa come abilità dello zaino, a tua scelta.

12. Come si ottengono kit, mod e zaini (sblocchi, progressione, valuta)?
    Perché conta: incide su salvataggi e interfaccia, non sulla simulazione.
    Ipotesi nella specifica: fuori da M11; tutto disponibile in sviluppo.

13. Quanti slot mod ha un kit, e quali tipi (lama, nucleo, impugnatura...)? Una sola mod per slot?
    Perché conta: definisce i limiti che impediscono combinazioni che rompono le mosse.
    Ipotesi nella specifica: slot dichiarati dal kit con tipi creati dal designer (`Mod.Slot.*`), una mod per slot, limiti di velocità e portata per kit ([M11 §8](combat-loadout-mods.md#8-mods-gstnkitmods)).

14. Gli impianti valgono anche per le armi di supporto (es. un impianto sul braccio che compare con l'arma) e per i nemici?
    Perché conta: decide se il sistema visivo degli impianti è solo dei giocatori.
    Ipotesi nella specifica: il componente visivo è generico (kit, arma, mod, zaino); sui nemici solo se il designer lo usa.

## Indicatore di profondità (23/10)

Specifica: [depth-marker.md](depth-marker.md). Già deciso da te il 23/10: un decal sotto il personaggio e una linea tratteggiata come nella mod 3D di Smash Melee, anche per alcuni nemici; l'aspetto finale lo sceglie il designer. Il punto aperto principale è su quale superficie puntare quando si è in aria (DM.1). Se l'ipotesi va bene basta rispondere "ok".

DM.1. A mezz'aria il marcatore deve puntare **dritto lungo la gravità locale** (superficie sotto i piedi) oppure al **punto di atterraggio previsto**? Va bene che la previsione dell'atterraggio sia un secondo marcatore opzionale, spento di default?
   Perché conta: la superficie sotto i piedi è stabile e leggibile ma non dice dove si atterra; la previsione dipende dagli input (controllo in aria, doppio salto, schivata, DI) e cambia a ogni frame, e per una vittima lanciata non è affidabile.
   Ipotesi nella specifica: dritto lungo la gravità con tolleranza ai bordi e isteresi ([depth-marker.md](depth-marker.md) §2.2); la previsione è un secondo selettore, spento di default e mai mostrato a chi è in hitstun.
   **Risposta (2026-10-23):** Dritto lungo la direzione della gravità. Se a mezz'aria si scende sotto la superficie dell'ultima piattaforma, l'indicatore punta anche verso l'alto, contro la gravità, al piano di quella piattaforma (come nella seconda foto); se si scende da una piattaforma alta verso un'altra più in basso, si vedono entrambi, sopra e sotto. Niente previsione dell'atterraggio per ora ([depth-marker.md](depth-marker.md) §2.7).

DM.2. Quando sotto non c'è terreno (burrone, fuori dal palco), l'anello vuoto va disegnato su quale piano di riferimento: **l'ultima altezza su cui si è stati a terra**, un **volume di riferimento posto dal designer** nel livello, o il piano della zona KO?
   Perché conta: decide se serve lavoro di authoring nei livelli e cosa legge il giocatore che recupera da sotto (dove sta il bordo, non dove muore).
   Ipotesi nella specifica: volume del designer se presente, altrimenti ultima altezza a terra su superficie statica, altrimenti nessun anello; il piano KO non è usato per l'anello ([depth-marker.md](depth-marker.md) §2.7).
   **Risposta parziale (2026-10-23):** Il piano è la superficie dell'ultima piattaforma. Resta un'ipotesi della specifica che il designer possa sostituirlo con un volume nei burroni dove il bordo da raggiungere non è l'ultima piattaforma.

DM.3. Chi vede il marcatore: sempre il giocatore locale, e gli **alleati**? Ci sono differenze tra PvE e PvP (in PvP anche gli avversari)? Il giocatore può spegnerlo o cambiarne l'intensità nelle opzioni?
   Perché conta: in un livello affollato i marcatori degli altri possono confondere, e l'opzione di accessibilità cambia le impostazioni utente da prevedere.
   Ipotesi nella specifica: giocatore locale sempre, alleati visibili (cooperativa), opzione utente per spegnere o attenuare; la regola per modalità è un dato dell'esperienza ([depth-marker.md](depth-marker.md) §3).

DM.4. Quali nemici hanno il marcatore: solo volanti, boss e nemici lanciati, o qualunque tipo scelto dal designer? Qual è il numero massimo di marcatori contemporanei (giocatori inclusi)?
   Perché conta: ogni marcatore costa tracce e disegno sull'host, che disegna anche il proprio gioco (listen server); il tetto fissa il budget.
   Ipotesi nella specifica: lo decide il dato del tipo di nemico (componente o estensione della definizione), tetto globale di 12 marcatori e 8 tracce per frame, ordinati per rilevanza ([depth-marker.md](depth-marker.md) §3).

DM.5. Il marcatore compare solo quando si è in aria oltre una certa altezza (per esempio 60 cm), oppure anche appena sopra il suolo? Compare anche durante hitstun, tumble e schivata in aria?
   Perché conta: soglia e stati cambiano quanto "rumore" visivo c'è nei combattimenti e quanto aiuta chi viene lanciato.
   Ipotesi nella specifica: in aria oltre 60 cm (nascosto sotto 40 cm), visibile anche da lanciato e in tumble, senza previsione dell'atterraggio ([depth-marker.md](depth-marker.md) §3, §2.6).

DM.6. Come aspetto di partenza va bene riprodurre quello della mod (disco pieno sul terreno, anello vuoto senza terreno, linea tratteggiata), tenendo cono d'ombra e versione diegetica come stili alternativi da scegliere alla fine? Un colore per giocatore va bene, con un simbolo o un motivo in più per i daltonici?
   Perché conta: lo stile è un asset sostituibile, quindi si può costruire prima e scegliere dopo; il colore da solo non basta a distinguere i giocatori.
   Ipotesi nella specifica: stile di default come nella mod, altri stili come asset separati, tinta per giocatore o team più glifo opzionale ([depth-marker.md](depth-marker.md) §4.1, §4.4).

DM.7. Durante il **wall slide** il marcatore resta visibile? Per chi è **afferrato, trasportato o in groppa** (piggyback) il marcatore si nasconde lasciando solo quello di chi porta?
   Perché conta: sono i casi in cui la posizione del personaggio non è la sua da solo, e due marcatori nello stesso punto confondono.
   Ipotesi nella specifica: wall slide sì (l'altezza dal suolo è utile), parcheggiato e in groppa nascosto, regola modificabile nel dato dello stile ([depth-marker.md](depth-marker.md) §2.6).

DM.8. Su quali superfici può puntare: **trampolini** e **piattaforme-nemico** sì? Le superfici dove si può solo scivolare sul muro (non camminare) no? Una piattaforma sottile a senso unico sì?
   Perché conta: decide il filtro sui comportamenti `Surface.Behaviour.*` e se i nemici-piattaforma devono rispondere al canale del marcatore.
   Ipotesi nella specifica: sì a pavimenti, piattaforme sottili, nemici-piattaforma e trampolini (`Stand` o `Launch`), no a personaggi, proiettili, trigger e superfici solo-parete ([depth-marker.md](depth-marker.md) §2.8).

DM.9. Nelle zone KO l'anello vuoto deve anche **avvertire del pericolo** (colore diverso, pulsazione), oppure resta lo stesso anello?
   Perché conta: è un segnale di gameplay oltre che di profondità, e richiede che il designer segni le zone con un volume di riferimento.
   Ipotesi nella specifica: sì, tramite un'opzione `bHazard` sul volume di riferimento posto nel burrone; senza volume l'anello resta neutro ([depth-marker.md](depth-marker.md) §2.7).

## Modalità di gioco (23/10)

Specifica: [game-modes.md](game-modes.md). Già deciso da te il 23/10: una modalità a livelli 3D fatti dal designer (stile Super Mario 3D World) e una modalità alla Helldivers con un grande livello generato a caso e missioni da completare; niente scudo in PvE; il kit si cambia raccogliendone uno; zaini come in Helldivers 2; le azioni delle modalità (terminali, chiamate di rinforzi e rifornimenti, estrazione) sono abilità pluggabili. Tutto il resto è un'ipotesi della specifica: se va bene basta rispondere "ok".

GM.1. Realizziamo prima la modalità Livelli (con il framework di missioni, respawn e verbi) e solo dopo la Spedizione alla Helldivers?
   Perché conta: la Spedizione richiede generazione procedurale, direttore dei nemici e lancio dei rifornimenti, molto più lavoro; farla per seconda dimostra prima l'infrastruttura comune.
   Ipotesi nella specifica: sì, M12 = framework + Livelli, M13 = Spedizione ([game-modes.md](game-modes.md) §9.3).
   **Risposta (2026-10-23):** Sì, prima i Livelli. La Spedizione resta da fare ma per ora non ce ne preoccupiamo molto.

GM.2. Quanti giocatori per le modalità PvE (1–4?), e deve funzionare anche da solo (l'host senza altri client)?
   Perché conta: dimensiona i budget dell'host ([game-modes.md](game-modes.md) §7.3), il numero di rinforzi, la difficoltà e i moduli caricati contemporaneamente.
   Ipotesi nella specifica: da 1 a 4 giocatori, giocabile anche da soli, difficoltà scalata sul numero di giocatori.

GM.3. Come si scelgono e si sbloccano i livelli (mappa del mondo, elenco, ordine fisso), e c'è salvataggio o progressione tra le partite?
   Perché conta: decide se serve una `UGSTN_LevelSetDefinition` a grafo e un sistema di salvataggio, oggi fuori ambito.
   Ipotesi nella specifica: elenco ordinato di mappe, nessun salvataggio nella prima versione.

GM.4. Nei Livelli, come funziona il respawn: vite infinite o limitate, checkpoint condivisi dalla squadra, cosa succede se muoiono tutti e quanto dura l'attesa?
   Perché conta: è la regola che più cambia il senso di una caduta in un burrone (KO zone) e va nei dati di `UGSTN_RespawnRules`.
   Ipotesi nella specifica: checkpoint condivisi, vite infinite con contatore delle morti per il punteggio, ritorno al checkpoint se muoiono tutti.
   **Risposta parziale (2026-10-23, da GM.9):** Vite condivise dalla squadra, niente rianimazione; nei Livelli chi cade riparte dal checkpoint condiviso consumando una vita del gruppo (ipotesi: in automatico, senza chiamata dei compagni). Resta aperto cosa succede quando le vite finiscono.
   **Risposta (2026-10-23, 12:44):** Quando finiscono le vite si ricomincia dal checkpoint o dall'inizio del livello, a seconda di quante vite vuoi dare: per esempio 20 vite senza checkpoint, oppure 4 vite che si ricaricano velocemente, così si perde solo quando quattro giocatori muoiono quasi insieme (in quel caso i checkpoint hanno senso). Nella specifica: dimensione del pool, ricarica, checkpoint sì/no e punto di ripartenza sono dati del livello ([game-modes.md](game-modes.md) §5).

GM.5. Qual è il traguardo di un livello (basta uno che arrivi o devono arrivare tutti i vivi), ci sono collezionabili (stelle, francobolli), contano per giocatore o per squadra, e c'è un punteggio?
   Perché conta: il traguardo è un obiettivo `Reach` con parametro `Require`; collezionabili e punteggio aggiungono stato replicato e una schermata dei risultati.
   Ipotesi nella specifica: arriva uno qualsiasi dei giocatori vivi, collezionabili contati per livello e per squadra, punteggio semplice a dati (`UGSTN_ScoreRules`).

GM.6. Che telecamera hanno i livelli: fissa o su binario per sezione come Mario 3D World, libera, o mista; una per giocatore o condivisa che inquadra tutti?
   Perché conta: la telecamera fissa peggiora la percezione della profondità (collegata a `depth-marker.md`), e l'orientamento della telecamera entra come input nella simulazione (serve il "latch" dello stick ai tagli).
   Ipotesi nella specifica: sezioni di telecamera scelte dal designer, una telecamera per giocatore, marcatore di profondità acceso nelle sezioni fisse.
   **Risposta (2026-10-23):** In generale abbastanza lontana dal giocatore, stile Mario 3D World; può passare sulle spalle per mirare con alcune armi e abilità; si può muovere a mano, ma nei livelli il designer può indicare l'angolo desiderato e se si può spostare o no. Specifica: [camera.md](camera.md).

GM.7. Quanto grande deve essere la mappa della Spedizione (superficie, durata media, numero di livelli di difficoltà), deve essere assemblata da moduli fatti a mano o anche generata "libera" (PCG), e vuoi poter ripetere o condividere una spedizione con un seed?
   Perché conta: dimensione e tipo di moduli decidono il tempo di generazione, la banda e la memoria dell'host, e se la posizione esatta delle collisioni è garantita; il seed condiviso obbliga a conservare le versioni del generatore.
   Ipotesi nella specifica: circa 800 m di lato, 150–250 moduli fatti a mano su griglia con rotazioni a 90°, PCG solo per decorazioni senza collisione; il seed è registrato e riproducibile con la stessa `GenerationVersion`, senza funzione di condivisione (seed del giorno) nella prima versione.
   *Spedizione a bassa priorità (23/10): si può rispondere più avanti.*

GM.8. Quali tipi di missione vuoi (distruggere, difendere, recuperare e portare un oggetto, attivare un terminale, eliminare, scortare, altro), quante per spedizione, obbligatorie o facoltative, e l'estrazione deve sempre esserci (basta un giocatore vivo o tutti)?
   Perché conta: ogni tipo è una regola in più da costruire e da testare; "distruggere un bersaglio" richiede un'entità distruttibile che oggi non esiste come oggetto di livello; la scorta richiede un'entità con attore.
   Ipotesi nella specifica: tutti i tipi di §4.1, bersagli come minion stazionari, 1–3 missioni primarie e alcune facoltative, estrazione sempre presente con almeno i giocatori vivi richiesti dentro la zona.
   *Spedizione a bassa priorità (23/10): si può rispondere più avanti.*

GM.9. Come si torna in gioco da morti: solo rinforzo con un pool condiviso alla Helldivers (quanti, si ricaricano?), o anche rianimazione sul posto (che richiederebbe uno stato "a terra" che oggi non esiste)? Con quale kit si rientra, e un giocatore morto guarda i compagni?
   Perché conta: lo stato "a terra" è una modifica al combat; il kit al rientro dipende dalla specifica dei loadout.
   Ipotesi nella specifica: solo rinforzo, pool condiviso con dimensione a dati, rientro con il kit scelto nella lobby, spettatore finché non viene richiamato.
   **Risposta (2026-10-23):** Niente rianimazione. Sistema a rinforzi (vite) condivisi. Letta come regola per entrambe le modalità PvE; resta aperto con quale kit si rientra.

GM.10. Come si chiamano rinforzi e rifornimenti: codice di direzioni come gli stratagemmi di Helldivers, menu, o un tasto? Cooldown condivisi o per giocatore? Si lancia un faro (oggetto tenuto, M10) o si sceglie un punto?
   Perché conta: cambia l'input (pattern in `GSTNInput`), l'interfaccia e se serve M10; l'abilità resta la stessa.
   Ipotesi nella specifica: stessa abilità con binding a dati, punto di mira nella prima versione, il faro lanciato come opzione successiva.
   *Spedizione a bassa priorità (23/10): si può rispondere più avanti.*

GM.11. Quanto deve essere "intelligente" il direttore dei nemici: pattuglie, irruzioni (breach) e ondate decise da una pressione che dipende da come stanno i giocatori, oppure schemi fissi e casuali? I nemici possono comparire alle spalle?
   Perché conta: un direttore adattivo è più complesso da bilanciare e da testare, e cambia la percezione di equità; i limiti di budget restano comunque quelli di M8.
   Ipotesi nella specifica: pressione adattiva con pattuglie dormienti, irruzioni telegrafate fuori dal cono visivo e a distanza minima, ondate legate agli obiettivi.
   *Spedizione a bassa priorità (23/10): si può rispondere più avanti.*

GM.12. L'ingresso a partita in corso è ammesso (Livelli, Spedizione)? Come entra il nuovo giocatore (vicino ai compagni, come rinforzo a pagamento del pool) e durante un conto alla rovescia di estrazione?
   Perché conta: obbliga a replicare lo stato completo e a costruire la mappa durante il gioco, e interagisce con il pool di rinforzi.
   Ipotesi nella specifica: ammesso in Livelli e Spedizione tranne durante l'estrazione, ingresso vicino ai compagni senza costo; mai in Versus.

GM.13. In ogni modalità: fuoco amico (già aperto come M4.12), cambio kit a metà partita raccogliendo un kit (permesso ovunque? kit trovati o solo dai rifornimenti?), zaini e armi di supporto solo nella Spedizione?
   Perché conta: sono i valori degli interruttori di §2.3; quelli che toccano la simulazione si fissano al caricamento dell'Experience, mai a partita in corso.
   Ipotesi nella specifica: cambio kit consentito in Livelli e Spedizione (kit piazzati dal designer nei primi, rifornimenti e kit trovati nei secondi), zaini e armi di supporto solo nella Spedizione, fuoco amico ancora da decidere.

GM.14. In Spedizione i giocatori possono allontanarsi quanto vogliono, o c'è un guinzaglio / un teletrasporto verso il gruppo?
   Perché conta: ogni giocatore lontano obbliga l'host a tenere caricati i moduli (collisione e navmesh) e a simulare nemici in più zone.
   Ipotesi nella specifica: nessun guinzaglio rigido; tetto morbido di moduli caricati sull'host (120) e, se superato, regola di ricongiungimento a dati.
   *Spedizione a bassa priorità (23/10): si può rispondere più avanti.*

GM.15. Il PvP (Versus) userà la stessa infrastruttura (arene d'autore, vite/stock, respawn a tempo, scudo, niente missioni né minion) e va rimandato dopo la Spedizione? Quali valori di fuoco amico e squadre?
   Perché conta: decide se `GSTNRespawn` deve già supportare gli "stock" e se M14 viene pianificato ora.
   Ipotesi nella specifica: sì, stessa infrastruttura, progettazione rimandata a M14; scudo acceso solo lì (indicazione già data).

## Telecamera (23/10)

Specifica: [camera.md](camera.md). Già deciso da te il 23/10 (GM.6): telecamera in generale lontana stile Super Mario 3D World, sulle spalle per mirare con alcune armi e abilità, movimento manuale possibile, ma nei livelli il designer sceglie l'angolo e se si può spostare. Se l'ipotesi va bene basta rispondere "ok".

CAM.1. Dopo che il giocatore ha spostato la telecamera a mano, deve **tornare da sola** all'angolo del designer? Dopo quanto tempo, e anche fuori dalle sezioni (telecamera libera)?
   Perché conta: un ritorno automatico aiuta a non perdere la lettura del livello ma toglie il controllo; il valore sta nei dati di ogni sezione.
   Ipotesi nella specifica: nessun ritorno automatico di default; per sezione un ritardo opzionale (0 = mai) e una velocità; entrando in una nuova sezione si va all'angolo del designer e lo spostamento manuale si perde ([camera.md](camera.md) §4).

CAM.2. Sopra **quale spalla** passa la telecamera quando si mira (destra o sinistra), e il giocatore può scambiarla con un tasto? Cambia per arma?
   Perché conta: la spalla decide cosa si vede e quanto il personaggio copre il mirino; lo scambio richiede un'azione in più (una richiesta `Camera.Request.*` data da un'abilità).
   Ipotesi nella specifica: spalla destra di default, scambio con un tasto dato da un'abilità, scelta ricordata per giocatore ([camera.md](camera.md) §6).

CAM.3. Mirare si fa **tenendo premuto** o con un **tasto che attiva e disattiva**? Quando smetti di mirare la telecamera torna all'angolo di prima o resta dove l'hai portata?
   Perché conta: la modalità di mira è dell'abilità (non della telecamera) e il ritorno cambia quanto ci si sente "spostati" dopo uno sparo.
   Ipotesi nella specifica: tenuto premuto (l'abilità termina al rilascio); all'uscita si mantiene la direzione di vista attuale come orbita libera, senza scatto indietro ([camera.md](camera.md) §3.3, §6).

CAM.4. Se la telecamera cambia (taglio di sezione, cambio di mirino) mentre tieni lo stick, il personaggio deve **continuare nella stessa direzione finché non rilasci lo stick**, oppure finché non giri lo stick di molto (per esempio oltre 60 gradi)?
   Perché conta: senza questa regola un taglio capovolge la direzione di corsa; il rilascio troppo lento sembra un controllo "incollato", troppo rapido non risolve il problema.
   Ipotesi nella specifica: direzione mantenuta finché lo stick torna al neutro; il rilascio per angolo è un'opzione spenta, per sezione si sceglie se tenere il blocco solo ai tagli o anche alle transizioni morbide ([camera.md](camera.md) §5).

CAM.5. Online e in locale: **una telecamera per giocatore** (come ipotizzato) o una condivisa che inquadra tutti? In locale ci sarà **split-screen** (co-op sullo stesso schermo) o solo un giocatore per macchina?
   Perché conta: una telecamera condivisa è un altro modo (distanza e limiti legati alla posizione di tutti, rischio di sezioni in conflitto) e lo split-screen raddoppia costi e interfaccia.
   Ipotesi nella specifica: una telecamera per giocatore, ognuna locale alla propria macchina (host incluso); split-screen non previsto nella prima versione ma l'architettura lo ammette; la telecamera di gruppo è un modo aggiuntivo rimandato ([camera.md](camera.md) §7.3).
   **Risposta parziale (2026-10-23, 12:45):** In PvP la telecamera è alla Tekken, centrata tra i due personaggi e con entrambi paralleli allo schermo ([camera.md](camera.md) §2.5). Restano aperti lo split-screen e la telecamera condivisa in co-op.

CAM.6. Quando un muro o un soffitto si mette tra la telecamera lontana e il personaggio, preferisci che l'ostacolo **sfumi** (la telecamera resta ferma) o che la telecamera **si avvicini** al personaggio?
   Perché conta: avvicinare la telecamera in un platform 3D distrugge la lettura delle distanze, ma sfumare richiede materiali compatibili e che il designer marchi i muri.
   Ipotesi nella specifica: sfumare per Follow e sezioni lontane, avvicinarsi solo per ostacoli non sfumabili vicini e in modalità spalla; una sezione può scegliere il contrario ([camera.md](camera.md) §7.1).

## Interazione: cosa si sta puntando (23/10)

Specifica: [interaction.md](interaction.md). Già deciso da te il 23/10 (Mod.20): un plugin dedicato, `GSTNInteract`, perché con moltissime armi per terra sia sempre chiaro su quale agisce il tasto; il contratto resta un'interfaccia più i gameplay tag. INT.5 riprende M6.22. Se l'ipotesi va bene basta rispondere "ok".

INT.1. Con la telecamera lontana alla Super Mario 3D World, il giocatore "punta" con il **personaggio** (direzione dello stick, poi quella in cui guarda) oppure con il
**centro dello schermo**? Con la mira da spalla di certe armi o abilità si usa la direzione di mira?
   Perché conta: il centro dello schermo richiederebbe la posizione della telecamera nella simulazione, che oggi non c'è; lo stick è già nell'input.
   Ipotesi nella specifica: personaggio (stick, poi direzione in cui guarda); direzione di mira solo con la mira da telecamera attiva ([interaction.md](interaction.md) §2.1).
   **Risposta (2026-10-23):** Con il personaggio (direzione dello stick, poi quella in cui guarda).

INT.2. Serve un **tasto per scorrere il bersaglio** quando due oggetti sono quasi alla stessa distanza e direzione, o basta spostarsi un poco?
   Perché conta: è un comando in più da legare a un tasto e un piccolo stato nel frammento; senza di esso la scelta dipende solo da dove ci si mette.
   Ipotesi nella specifica: nessun tasto nella prima versione, disponibile come opzione a dati ([interaction.md](interaction.md) §2.5).

INT.3. Gli altri oggetti vicini (non quello selezionato) devono avere un **segnalino tenue**, oppure si evidenzia solo il bersaglio?
   Perché conta: con tante armi per terra i segnalini aiutano a capire cosa c'è, ma possono aumentare la confusione che vuoi evitare, e costano disegno sull'host.
   Ipotesi nella specifica: spenti di default, evidenziazione forte solo sul bersaglio; i segnalini (massimo 8) sono un'opzione dello stile ([interaction.md](interaction.md) §5).

INT.4. Gli **alleati** (e in PvP gli avversari) vedono cosa stai puntando, per esempio con un contorno di colore diverso sull'oggetto che un compagno sta per raccogliere?
   Perché conta: evita di contendersi lo stesso oggetto in cooperativa, ma aggiunge rumore visivo e, in PvP, regala informazione.
   Ipotesi nella specifica: no, si disegna solo il bersaglio del giocatore locale; il dato è già replicato, quindi l'opzione costa solo presentazione ([interaction.md](interaction.md) §5).

INT.5. Quando un **nemico afferrabile** e un **oggetto** a terra sono entrambi a portata, vince sempre il nemico (così il tocco afferra nel mezzo di uno scontro) oppure conta
soprattutto ciò che si sta puntando? Qual è l'ordine di priorità tra afferrare, terminale, raccogliere oggetto, kit e montare?
   Perché conta: è la domanda M6.22; con un nemico davanti, un tocco che non afferra sarebbe grave in combattimento, ma una priorità rigida impedisce di scegliere l'oggetto.
   Ipotesi nella specifica: la priorità è un contributo al punteggio (non un'esclusione), ordine afferrare, terminale, raccogliere oggetto, kit, montare, tutto a dati ([interaction.md](interaction.md) §2.3).

INT.6. Un oggetto che **non si può usare ora** (si porta già qualcosa, kit non compatibile, terminale non ancora attivo) viene evidenziato con un messaggio grigio che spiega il
motivo, oppure viene ignorato come se non ci fosse?
   Perché conta: mostrarlo spiega perché il tasto non fa nulla, ma un oggetto inutilizzabile può rubare il bersaglio a uno utilizzabile lì vicino.
   Ipotesi nella specifica: ignorato, tranne dove il progettista lo segna (terminali bloccati), che mostrano il motivo senza tag attivo ([interaction.md](interaction.md) §2.2).

CAM.7. Nel PvP con più di due giocatori (3 o 4), la telecamera alla Tekken segue te e l'avversario più vicino, oppure passa a un'inquadratura larga alla Smash che li tiene tutti?
   Perché conta: la regola "centro tra i due personaggi" vale per un uno contro uno; con più giocatori serve una scelta.
   Ipotesi nella specifica: con due giocatori la telecamera alla Tekken; con più giocatori inquadratura larga che li tiene tutti, con la direzione dell'ultimo asse tra te e l'avversario più vicino ([camera.md](camera.md) §2.5).

CAM.8. Online, ognuno vede se stesso sempre a sinistra dello schermo (come Tekken 7 aveva provato con entrambi i giocatori sul lato sinistro dei rispettivi cabinati), oppure i lati sono fissi per giocatore 1 e 2?
   Perché conta: cambia da che lato parte la telecamera su ogni macchina; in locale sullo stesso schermo i lati sono per forza condivisi.
   Ipotesi nella specifica: online ognuno parte a sinistra sul proprio schermo; sullo stesso schermo il giocatore 1 parte a sinistra.

CAM.9. Della telecamera di Astro Bot fuori dai livelli guidati, ti va bene questa lettura: non gira da sola dietro al personaggio, durante i salti resta all'altezza dell'ultima piattaforma e scende subito se cadi più in basso, guarda un po' avanti nella direzione di corsa, e un tasto la riporta dietro al personaggio? C'è altro che ti piace di quella telecamera?
   Perché conta: ho potuto verificare solo il controllo con lo stick destro e l'assistenza che la ricentra; il resto è la mia lettura di una telecamera da platform 3D calma.
   Ipotesi nella specifica: come descritto ([camera.md](camera.md) §2.4), tutti i valori modificabili nei dati.

CAM.10. La finestra di zoom del mirino va sempre in un punto fisso (in alto di lato, come nel disegno), oppure segue il punto mirato? Quanto grande (circa 40% dell'altezza dello schermo)? E la linea che la collega al mirino dell'arma la vuoi sempre?
   Perché conta: decide se la finestra copre mai il personaggio o il bersaglio e quanto si vede del resto della scena.
   Ipotesi nella specifica: in alto e di lato rispetto al punto mirato, circa 40% dell'altezza, sempre dentro lo schermo e mai sopra il personaggio, con la linea di collegamento; tutto modificabile per arma ([camera.md](camera.md) §6.1).
   **Risposta (2026-10-23):** Segue la telecamera: posizione fissa nella vista della telecamera (di default in alto di lato come nel disegno), non insegue il punto mirato.

CAM.11. Il mirino ha più livelli di zoom da cambiare con un tasto? Mentre si mira con il mirino ci si può muovere liberamente (magari più lentamente), saltare o scivolare sui muri?
   Perché conta: la flessibilità della terza persona dipende da cosa resta permesso mentre si mira; i livelli di zoom richiedono un comando in più.
   Ipotesi nella specifica: zoom a uno o più livelli per arma (dati), ci si muove liberamente con velocità ridotta decisa dall'abilità di mira dell'arma; salti e muri a scelta del designer della singola arma.
   **Risposta (2026-10-23):** Dipende dall'abilità e dall'arma che usa il mirino: sono dati loro, non della telecamera.

## Cristallo per giocatore, god rays, ombra sulla nebbia (24/10)

Specifiche: [combat-parts.md](combat-parts.md) §6.0 e [depth-marker.md](depth-marker.md) §2.7, §4.1, §4.4. Già deciso da te il 24/10: dentro i
personaggi sono di cristallo e il colore del cristallo cambia a seconda del giocatore; god rays dal personaggio e ombra sulla nebbia sono idee
da tenere come opzioni accanto alla solita ombra alla Mario.

CR.1. Il colore del cristallo è **legato al posto del giocatore** (giocatore 1 sempre blu, giocatore 2 sempre rosso...) oppure **lo sceglie
il giocatore**? Se lo sceglie, cosa succede quando due giocatori vogliono lo stesso colore?
   Perché conta: un colore per posto è automatico e sempre distinguibile; la scelta libera richiede un menu e una regola per i conflitti.
   Ipotesi nella specifica: colore per posto da una tavolozza (posto assegnato all'ingresso, tenuto per tutta la partita); la scelta libera si
   aggiunge dopo, e in caso di conflitto chi entra dopo prende il primo colore libero.
   **Risposta (2026-10-24):** Dipende dal numero del giocatore: 1 rosso, 2 blu, 3 giallo, 4 verde. L'ordine segue l'ordine alfabetico dei
   nomi, così quattro amici che giocano sempre insieme hanno sempre lo stesso colore. Nella specifica: chi entra a partita iniziata prende il
   primo numero libero e nessuno cambia colore durante la partita ([combat-parts.md](combat-parts.md) §6.0).

CR.2. I **god rays** che partono dal personaggio sono soprattutto un **indicatore di profondità** (un fascio di luce che scende fino a dove
atterri, al posto della linea tratteggiata), oppure **luce che esce dalle crepe** del corpo (più crepe, più raggi), oppure entrambe le cose?
   Perché conta: il primo è uno stile dell'indicatore di profondità; il secondo fa parte dell'aspetto dei danni del personaggio.
   Ipotesi nella specifica: entrambe come opzioni separate: uno stile `Style_CrystalRays` dell'indicatore e, a parte, un bagliore del cristallo
   che cresce con i danni.

CR.3. Il cristallo deve **brillare di più quando il personaggio è danneggiato** (luce dalle crepe che aumenta), oppure il colore resta fisso e
si vedono solo le crepe?
   Perché conta: un bagliore crescente aiuta a leggere la salute da lontano, ma con quattro giocatori molto danneggiati può diventare rumore.
   Ipotesi nella specifica: sì, emissivo che cresce con il danno, con intensità massima regolabile dal designer.

CR.4. La **nebbia su cui cade l'ombra** è qualcosa che il designer mette in certi livelli (un mare di nuvole sotto le piattaforme), oppure deve
esserci sempre sotto ogni livello?
   Perché conta: se il designer la piazza, i livelli senza nebbia usano l'anello nel vuoto; se c'è sempre, l'anello nel vuoto serve solo quando
   si è sotto la piattaforma.
   Ipotesi nella specifica: la piazza il designer (un attore "strato di nebbia"); dove c'è, l'ombra cade sulla nebbia al posto dell'anello nel
   vuoto.

CR.5. I **nemici** hanno un colore di cristallo per tipo di nemico (scelto dal designer), oppure tutti uguali, oppure il colore indica qualcosa
(per esempio il punto debole)?
   Perché conta: il colore per giocatore serve a riconoscersi; se anche i nemici usano colori vivi bisogna evitare che somiglino a quelli dei
   giocatori.
   Ipotesi nella specifica: colore per tipo di nemico, scelto dal designer, da una tavolozza separata da quella dei giocatori.

## Demolizione, hitbox variabili, spin da tastiera, collisioni dei colpi (25/10)

Specifiche: [combat-m4-hits.md](combat-m4-hits.md) §3, §7c, §9c, [combat-parts.md](combat-parts.md) §3.4, [input.md](input.md) §9. Le parti
del trapano sono in [spin-drill.md](spin-drill.md) §13b (PR #15).

DEM.1. La forza di demolizione è un **numero a passi di 10** come in Helldivers 2 (10 un pugno, 20 un colpo pesante, 30 il trapano, 40 un
esplosivo), oppure **pochi livelli con un nome** (leggera, media, pesante, esplosiva)?
   Perché conta: i numeri lasciano spazio fine al bilanciamento; i livelli con nome sono più facili da leggere per un designer.
   Ipotesi nella specifica: numeri interi a passi di 10; l'editor può mostrare un nome per ogni fascia.

DEM.2. Sotto la soglia l'oggetto **non subisce nulla** e si sente un colpo sordo, oppure l'attacco **rimbalza** come con la durezza delle parti?
   Perché conta: un rimbalzo punisce chi colpisce la cosa sbagliata, ma interrompe la combo.
   Ipotesi nella specifica: nessun danno e un suono sordo, nessun rimbalzo; solo il trapano rimbalza, perché non riesce ad agganciarsi.

DEM.3. Un **nemico lanciato** contro una cassa la rompe? Con quale forza?
   Perché conta: decide se i lanci servono anche a distruggere cose.
   Ipotesi nella specifica: la forza la decide l'attacco che lo ha lanciato (un campo dell'impatto), di base uguale alla forza di
   demolizione di quel colpo.

DEM.4. Un oggetto distruttibile si rompe **al primo colpo** sopra la soglia, oppure ha **più punti vita**?
   Perché conta: casse da un colpo rendono il trapano e i colpi pesanti immediati; muri da più colpi fanno durare l'aggancio del trapano.
   Ipotesi nella specifica: punti vita per oggetto, 1 di base.

HV.1. Lo stick tenuto durante lo swing si legge **rispetto al personaggio** (verso la sua destra o sinistra) o **rispetto allo schermo**?
   Perché conta: con la telecamera lontana e girata, "destra" sullo schermo e destra del personaggio possono essere diverse.
   Ipotesi nella specifica: rispetto a dove guarda il personaggio, come gli altri comandi di combattimento.

HV.2. Se lasci lo stick, la spada **torna verticale** o **resta inclinata** dove era arrivata? E da un certo momento l'angolo si **blocca**?
   Perché conta: decide quanto si può correggere il colpo all'ultimo momento.
   Ipotesi nella specifica: resta dove era, e si blocca al frame scelto dal designer (di solito il primo frame attivo).

KB.1. A-S-D vale come spin in un senso e D-S-A nell'altro? Vanno premuti nello stesso tempo concesso allo spin con lo stick?
   Perché conta: decide se la scorciatoia funziona in entrambi i sensi e quanto deve essere veloce.
   Ipotesi nella specifica: sì a entrambe; A-S-D è antiorario, D-S-A orario.

KB.2. La scorciatoia vale anche per il **d-pad** del controller, e la stessa idea va usata per altri gesti difficili da tastiera?
   Perché conta: il d-pad ha gli stessi limiti della tastiera.
   Ipotesi nella specifica: vale per tutti gli ingressi digitali (tastiera e d-pad), gesto per gesto.

HV.3. Quali attacchi **ruotano il personaggio** mentre tieni lo stick (per esempio alcuni attacchi in aria)? La rotazione segue lo
stick finché lo tieni, oppure si sceglie solo una volta (all'inizio o in un momento preciso, come girarsi all'indietro)?
   Perché conta: una rotazione continua cambia dove colpisce l'attacco mentre è attivo; una scelta singola è più leggibile.
   Ipotesi nella specifica: entrambe possibili per singola mossa, decise dal designer con la stessa traccia che inclina lo swing.
   **Risposta (2026-10-25 19:56):** Dipende dalla mossa. In alcune durante lo startup ci si gira di pochi gradi, in altre si cammina. Esempio:
   swing verticale in aria, la direzione la decide l'input iniziale, nella prima parte dello startup si corregge un po' l'angolo, durante lo
   swing lo si porta in diagonale. In aria di solito ci si muove ma non ci si gira, salvo queste piccole correzioni
   ([combat-m4-hits.md](combat-m4-hits.md) §3, traccia `Control`).

INTP.1. L'interpolazione delle hitbox tra un frame e l'altro (come nei platform fighter) va **accesa di base** su tutti i colpi, oppure solo
dove il designer la attiva?
   Perché conta: accesa ovunque evita i colpi "bucati" dagli attacchi veloci, ma costa qualche controllo in più per colpo.
   Ipotesi nella specifica: accesa in tutte le mosse del set standard, e l'editor avvisa quando una hitbox veloce è senza.

COL.1. I colpi corpo a corpo devono **fermarsi contro i muri** di base, oppure solo dove il designer lo attiva per una mossa?
   Perché conta: oggi un colpo attraversa un muro sottile e colpisce chi c'è dietro; bloccarlo costa un controllo in più per colpo.
   Ipotesi nella specifica: spento nei dati di M4 per non cambiare nulla, acceso nelle mosse del set standard dopo H16.

COL.2. Per colpire la cavità dentro la gamba corazzata basta **toccare l'apertura** con un'arma abbastanza sottile, oppure l'arma deve
**entrare** nel buco?
   Perché conta: "toccare" è più semplice e generoso, "entrare" è più preciso ma più difficile da ottenere con uno swing.
   Ipotesi nella specifica: basta toccare l'apertura con un'arma che ci passa.

## Input a carica, stile Guile (26/10)

Specifica: [input.md](input.md) §10. Il plugin di input ha già la condizione `Charge` (insieme di direzioni, durata minima, tolleranza dopo
il rilascio); qui ci sono solo le scelte di gioco.

CH.1. In 3D, fuori dal duello, il personaggio si gira verso lo stick, quindi "tenere indietro" non si può fare. Le cariche **indietro**
valgono solo dove la direzione è bloccata (telecamera da duello, aggancio al bersaglio), e altrove si usano le cariche **in basso**
(tenendo premuto abbassarsi)? Oppure "indietro" va inteso rispetto alla telecamera?
   Perché conta: cambia quali mosse a carica si possono dare ai kit usati nei livelli.
   Ipotesi nella specifica: cariche indietro solo con la direzione bloccata, cariche in basso ovunque.
   **Risposta (2026-10-26 09:53):** La carica indietro funziona in 3D: basta mantenerla finché lo stick resta entro un certo limite di gradi.
   Nella specifica: la carica è relativa alla direzione in cui è iniziata, e "avanti" vuol dire l'opposto di quella direzione
   ([input.md](input.md) §10). Poi (09:54, 09:55): una mossa può avere le hitbox all'indietro, oppure girare subito il personaggio e
   colpire in avanti; lo decide l'abilità.

CH.2. Quanto deve durare la carica? In Street Fighter è circa tre quarti di secondo o poco più.
   Perché conta: è il ritmo di tutte le mosse a carica.
   Ipotesi nella specifica: valore per singola mossa, di base 40 frame (circa 0,65 s), con 8 frame di tolleranza dopo il rilascio.
   **Risposta (2026-10-26 09:56):** Va bene. Nella specifica: 40 frame di base, 8 di tolleranza, regolabili mossa per mossa.

CH.3. Nel duello, se l'avversario ti salta dall'altra parte, la carica indietro si perde (come in Street Fighter) o viene conservata?
   Perché conta: conservarla rende più facile difendersi dai salti incrociati con una mossa a carica.
   Ipotesi nella specifica: si perde, come in Street Fighter.
   **Risposta (2026-10-26 09:56, ricavata dalle tue risposte a CH.1):** La carica è relativa alla direzione in cui è iniziata, non a
   dove guarda il personaggio, quindi un salto incrociato **non la fa perdere**. La mossa parte comunque nel verso opposto alla carica, e
   se si gira verso l'avversario lo decide l'abilità. Una mossa che vuole la regola di Street Fighter può ancora usare la carica relativa
   allo sguardo ([input.md](input.md) §10).

CH.4. Di quanti gradi può spostarsi lo stick dalla direzione della carica senza perderla, e quanto velocemente quella direzione può
seguire lo stick (tu hai detto: anche un giro intero, ma muovendo lo stick molto lentamente)?
   Perché conta: un angolo stretto fa perdere la carica a chi muove un po' lo stick; una direzione che segue troppo in fretta rende la carica
   valida in qualunque direzione.
   Ipotesi nella specifica: 50 gradi da ogni lato, e la direzione segue lo stick a 90 gradi al secondo al massimo (un giro intero in 4
   secondi); tutto regolabile mossa per mossa.

## Zone di oggetti attivabili da un solo giocatore (26/10)

Specifica: [level-activation.md](level-activation.md). Come hai chiarito (09:58, 10:00), è la **base generica**: una zona di oggetti che un
solo giocatore attiva con un'azione e che prende il suo colore; le piattaforme rosse e blu e la piattaforma "solo Mario" sono esempi che
costruirai tu sopra. Nella base: il proprietario può essere il primo che sale, il primo che entra, chi ci sta sopra in quel momento, oppure
un giocatore **assegnato** in partenza (come la piattaforma di Mario). Il ritardo prima del cambio aiuta anche online, perché dà il tempo
a tutte le macchine di sapere quando avverrà.

FLP.1. Quando una zona **torna libera** per un altro giocatore? Quando il proprietario non ci sta sopra da un po', quando muore, oppure mai
finché resta nel livello? (Non vale per le zone con proprietario assegnato.)
   Perché conta: se non si libera mai, un giocatore lontano blocca la zona per tutti.
   Ipotesi nella specifica: si libera dopo circa 2 secondi che il proprietario non ci sta sopra, o quando va KO.
   **Risposta (2026-10-26 10:13):** Lo decide il designer. Nella specifica: per ogni zona si sceglie se si libera dopo un tempo senza il
   proprietario, quando il proprietario va KO, entrambe le cose (valore di base) oppure mai.

FLP.2. Il **ritardo** prima che l'oggetto reagisca va bene come valore per zona, con 20 frame di base (circa 0,3 s) per gli oggetti che
cambiano il terreno sotto gli altri giocatori e 0 per quelli che portano solo il proprietario? Vale anche giocando da soli?
   Perché conta: con 0,3 secondi online tutti vedono il cambio nello stesso momento; da soli può sembrare lento.
   Ipotesi nella specifica: sì, deciso dal designer zona per zona, con gli oggetti che lampeggiano nel colore del proprietario durante
   l'attesa; da soli lo stesso valore, salvo che tu non preferisca 0.
   **Risposta (2026-10-26 10:13):** L'ipotesi va bene.

FLP.3. L'azione del proprietario conta **ovunque si trovi** (come il salto in Mario 3D Land) oppure solo se la fa **dentro la zona** o
stando sopra?
   Perché conta: "ovunque" permette di attivare da lontano per aiutare gli altri, "dentro" lega la zona a chi la sta usando.
   Ipotesi nella specifica: opzione del designer per zona, di base "dentro la zona".
   **Risposta (2026-10-26 10:13):** L'ipotesi va bene.

FLP.4. Gli **altri giocatori** possono fare qualcosa su una zona occupata (per esempio salirci senza farla reagire), oppure la zona li
ignora del tutto?
   Ipotesi nella specifica: ci possono stare sopra e camminare, le loro azioni non contano finché la zona è occupata.
   **Risposta (2026-10-26 10:13):** L'ipotesi va bene.

FLP.5. Quali **altre azioni** vuoi che attivino gli oggetti? Schianto a terra, spin, trapano, colpo, interazione, entrare in una zona?
   Ipotesi nella specifica: tutte, scelte dal designer oggetto per oggetto; salto, atterraggio, entrata e colpo funzionano subito, le altre
   dopo una piccola aggiunta al sistema delle abilità.
   **Risposta (2026-10-26 10:13):** L'ipotesi va bene.

FLP.6. I **nemici** possono attivare questi oggetti (per esempio un nemico che salta sulle piattaforme)?
   Ipotesi nella specifica: no di base, solo i giocatori; il designer può aprirlo per un oggetto.
   **Risposta (2026-10-26 10:13):** L'ipotesi va bene.
