# Character creator modulare: piano (2026-11-27)

Cloud, Opus high, solo pianificazione: nessun codice. Feature nuova in fase greybox (regola 6), quindi le scelte di
architettura sono dell'owner (sezione 6). Legenda: **[letto]** = verificato nel repo, **[da verificare 5.9]** = da
provare sul fisso prima di contarci.

## 1. Cosa chiede l'owner
- Uno strumento per **costruire le facce dei personaggi base** (Overlord, BirdKnight, FlameFury, Voyager) e, più avanti,
  una schermata in cui **i giocatori cambiano solo la faccia del Voyager** (gli altri personaggi restano fissi).
- I personaggi sono molto stilizzati e diversi tra loro: una testa unica con blendshape non basta. Serve un sistema
  **a porzioni** (occhi, naso, bocca, orecchie, mento...) incollate tra loro e poi **rifinite con morph target**, come i
  personaggi di Zelda BOTW costruiti partendo da un sistema a parti tipo Mii.
- L'artstyle lo stanno definendo l'owner e Antigravity (documenti e artbook, proiezioni ortogonali di un personaggio di
  riferimento, non ancora finiti). Questo piano **non fa arte**: dice solo che cosa l'arte deve rispettare perché i pezzi
  si possano combinare (sezione 3, "contratto"), da riempire quando le loro specifiche sono pronte.

## 2. Che cosa c'è già e si riusa [letto]
| Pezzo | Dove | Uso nel creator |
|---|---|---|
| Skin = mesh + materiali + AnimClass, solo presentazione, riferimenti soft | `GSTNCombatCore/Kits/GSTN_SkinDefinition.h`, `GSTNCombatGAS/Cosmetics/GSTN_SkinComponent.h` | La faccia è un "pezzo" in più sopra la skin del Voyager; `SetForcedSkin` e la riga Skin del pannello GSTN Debug per confrontare |
| Loadout con blocco estensioni versionato (layout 1, max 1024 byte, entry da `FGSTN_LoadoutExtensionData`) | `GSTNCombatCore/Loadout/GSTN_LoadoutData.h` | Il descrittore della faccia viaggia lì: niente protocollo nuovo |
| Plugin GSTNCosmetics (Core): planning deterministico, chiave di cache con `AssemblerVersion`, tag `Cosmetic.*` | `Plugins/GSTNCosmetics/` | Stesso schema per la faccia: descrittore → piano → assemblatore → cache |
| Decisione 2A cosmetici (owner 11-23): assemblare all'armadietto (cache su disco) e al caricamento per gli altri, asincrono, con placeholder "amorfo", mai bloccante | `docs/notes/skin-cosmetici-modulari.md` §2b-2c | Identica per le facce: stesso assemblatore, stessi momenti |
| Scheletro di gioco unico invisibile (BirdKnight base), proporzioni nella mesh entro limiti, testa fino a +25% | `docs/notes/skin-proporzioni-analisi.md`, memoria skin-lyra-confronto (1A) | La testa del creator si attacca all'osso `head` di quello scheletro; non tocca hitbox né animazioni approvate |

La simulazione non legge mai una mesh (hitbox cotte, hurtbox del kit) [letto]: **la faccia non ha alcun effetto sul gioco**,
quindi non entra nello stato di rollback.

## 3. Come funziona (proposta)
### 3.1 Tre livelli, come un Mii
1. **Forma della testa**: una testa base per personaggio (o per famiglia di corpi) con **morph target** continui
   (larghezza cranio, mascella, guance, mento).
2. **Pezzi 3D intercambiabili** per ciò che sporge o cambia silhouette: naso, orecchie, sopracciglia in rilievo, mento,
   capelli/ciuffo, barba. Ogni pezzo ha i suoi morph (es. naso: lunghezza, punta, larghezza) e si posiziona con
   **parametri Mii**: tipo, posizione verticale, distanza, scala, rotazione, colore, tutti limitati.
3. **Tratti disegnati** (occhi, pupille, bocca, sopracciglia piatte, segni, lentiggini): texture su strati del materiale
   della testa, con offset/scala/rotazione da parametri. È lo stile giocattolo di Astro Bot ed è il modo più economico per
   le **espressioni** (cambio di texture o di UV, come i Mii e molti personaggi stilizzati).

### 3.2 "Incollare" i pezzi senza cuciture
Due modi, scelti per pezzo:
- **Pezzo appoggiato** (consigliato per naso, orecchie, sopracciglia, capelli): il pezzo si sovrappone alla pelle e la
  normale al bordo si sfuma verso quella della testa. Nessuna saldatura, nessun problema di vertici; nello stile
  toy-like la piccola compenetrazione non si vede.
- **Pezzo con anello di giunzione** (solo dove serve davvero, es. mento/mascella): il bordo del pezzo ha un anello di
  vertici standard (stesse posizioni, normali, UV) uguale su tutte le varianti, e i morph della testa muovono l'anello
  allo stesso modo. È il punto più costoso per l'arte: limitarlo a 1-2 zone.

### 3.3 Rig e animazione della faccia
La testa è un **componente skeletal mesh separato con un piccolo scheletro facciale** (mascella, palpebre, sopracciglia,
bocca, 10-20 ossa) attaccato all'osso `head` dello scheletro di gioco. Tutti i pezzi sono skinnati a quelle ossa, quindi
ogni combinazione si anima con gli stessi asset di espressione. Lo scheletro del corpo, le clip approvate e i bake non
cambiano (regola base dell'owner).

### 3.4 Dati
- `UGSTN_FacePartDefinition` (primary asset, soft ref): slot (`Cosmetic.Face.Nose`...), mesh o texture, morph esposti
  con range, limiti di posizione/scala, personaggi compatibili (tag).
- `FGSTN_FaceDescriptor`: per slot un ID di pezzo + parametri quantizzati a 1 byte + colori in palette. Stima
  **40-80 byte**: entra nel blocco estensioni del loadout (max 1024).
- **Preset** per personaggio base (asset editabile dall'owner) = descrittore salvato. I personaggi base possono essere
  **cotti** in una skeletal mesh normale: a runtime costano come una skin qualsiasi.
- Validazione: range e compatibilità controllati all'ingresso in partita (l'host rifiuta valori fuori limite, come per le
  proporzioni); versione del descrittore per leggere i salvataggi vecchi.

### 3.5 Rete (1v1 P2P e co-op, rollback)
Solo il descrittore viaggia, una volta, nell'handshake del loadout all'inizio della partita (revisione del loadout, mai
bit per frame), coerente con "tutto il gameplay in rollback" perché la faccia non è gameplay. Ogni macchina assembla in
locale con un assemblatore deterministico (stesso input, stessa testa). Entrata a partita iniziata: placeholder e
assemblaggio asincrono (decisione 2A dei cosmetici). Nulla nello stato NP.

## 4. Tecnologie UE 5.8/5.9
| Opzione | Pro | Contro | Verdetto |
|---|---|---|---|
| **Mutable (Customizable Object)** | Fatto per questo: scambio di parti, morph, strati di texture e proiezioni, unione in una mesh con LOD, generazione a runtime asincrona; usato da Fortnite | Plugin grande, curva di apprendimento, tempi di compilazione del Customizable Object, un secondo sistema accanto a GSTNCosmetics; maturità in 5.9 **[da verificare 5.9]** | Candidato forte, va provato |
| **Assemblatore nostro** (estende GSTNCosmetics: componenti con leader pose o merge, morph, material layer) | Coerente con il plugin esistente, stesso planning/cache, controllo completo | `USkeletalMergingLibrary` storicamente perde i morph target nel merge **[da verificare 5.9]**; senza merge, un componente per pezzo (costo draw call, ok per 2-4 giocatori) | Candidato forte, va provato |
| Solo morph target su una testa | Semplice | L'owner l'ha già escluso: troppo diversi tra loro | No |
| Decal per i tratti | Facili da posizionare | Su una testa che si anima scivolano; meglio strati del materiale in UV | Solo per segni statici |
| **MetaHuman** | Rig facciale eccellente | Realistico, pesante, topologia fissa, non modulare a pezzi | Inadatto allo stile |

Consiglio: **una prova breve dei due candidati sulla stessa testa greybox** prima di scegliere (F1).

## 5. Fasi, costi, rischi
| Fase | Cosa | Dove / modello | Stima |
|---|---|---|---|
| **F0** | Contratto per l'arte, da scrivere insieme alle specifiche di Antigravity quando sono pronte: elenco slot, quali pezzi sono appoggiati e quali con anello, budget poligoni e texture, nomi dei morph, ossa facciali, atlante dei tratti, cosa si vede dalla camera di gioco | cloud, Opus | 2-4 $ |
| **F1** | Prova (spike) su una testa greybox: 3 nasi, 2 orecchie, 3 occhi disegnati, 3 morph; Mutable contro assemblatore nostro. Misure: morph conservati, cuciture, tempo di assemblaggio, memoria, tempo di compilazione | fisso RC, Opus + Sonnet | 6-10 $ |
| **F2** | Dati (pezzi, descrittore, preset) e strumento editor per i personaggi base (pannello con slider, casuale, salva preset, cuoci in mesh) | cloud + build fisso | 10-16 $ |
| **F3** | Runtime e rete: descrittore nel loadout, validazione dell'host, assemblaggio asincrono con placeholder e cache, test GSTN | cloud + build + harness | 6-10 $ |
| **F4** | Schermata giocatore per la faccia del Voyager (camera ravvicinata, slot, slider, colori, casuale, annulla) | cloud + fisso | 8-14 $ |
| **F5** | Espressioni facciali (layer d'animazione della testa, texture degli occhi/bocca), solo nella fase estetica | fisso | 5-8 $ |

Totale **~37-62 $** senza il costo dell'arte. F0 si fa appena l'artstyle è pronto; F1-F4 aspettano il via dell'owner;
F5 resta ferma fino alla fase estetica (regola 6).

**Rischi**
1. **Quanto si vede**: con la camera lontana stile Mario 3D World la faccia in gioco occupa pochi pixel. Il creator conta
   in menu, primi piani, vittoria e camera di spalla: il contratto F0 deve dire a che distanza va letta la faccia.
2. **Costo dell'arte**: ogni pezzo moltiplica morph, LOD e espressioni. Partire con pochi pezzi per slot (3-5) e
   preferire i tratti disegnati, che costano una texture.
3. **Cuciture e luce** sui pezzi con anello: limitarli a 1-2 zone, provarli in F1.
4. **Morph dopo il merge** e tempi di Mutable: il motivo dello spike F1.
5. **Capelli/cappelli e skin**: compenetrazioni tra pezzi della faccia e cosmetici della testa; serve una regola di
   priorità per slot (un cappello nasconde il ciuffo).
6. **Combinazioni brutte o offensive** nella schermata giocatore: limiti stretti e palette chiuse.
7. **Salvataggi**: il descrittore va versionato da subito, come il loadout.

## 6. Decisioni per l'owner (consigliate 1A 2A 3A)
1. **Come si costruisce la faccia**: A tre livelli (testa con morph + pezzi 3D appoggiati + tratti disegnati); B tutto in
   pezzi 3D saldati con anelli + morph.
2. **Tecnologia**: A spike F1 di Mutable contro assemblatore nostro, poi si sceglie (+6-10 $ subito, evita di rifare
   F2-F4); B Mutable subito; C assemblatore nostro in GSTNCosmetics subito.
3. **Rig della faccia**: A testa separata con il suo scheletro facciale, attaccata all'osso `head` (corpo e clip
   approvate intatti); B ossa facciali aggiunte allo scheletro di gioco condiviso.
