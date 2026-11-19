# Peer Review: Design Telecamera da Platform per GSTN (2026-11-19)

Valutazione critica del documento `design.md` a cura di Antigravity / agy, a valle della ricerca indipendente e dell'analisi del plugin `GSTNCamera`.

---

### 1. Errori di fatto su Astro Bot, DK Bananza, Mario Odyssey/Galaxy, Nesky (§1)
- **Riga 29 (Odyssey - Occlusione)**: *Errore di fatto*. "Gli oggetti fra camera e Mario non spostano la camera: diventano trasparenti a retino". FALSO. In Super Mario Odyssey il dithering stipple/Bayer si applica solo a prop minori (alberi, cespugli, cartelli, steccati). Le geometrie solide del mondo (edifici di New Donk City, pareti di roccia, colline) attivano la normale collisione (sphere-sweep) e spingono la telecamera in avanti (pull-in).
  - *FONTE*: Gameplay e analisi fotogramma per fotogramma di Super Mario Odyssey; schede tecniche rendering Nintendo EPD.
  - *OPINIONE*: Dichiarare l'occlusione a solo retino senza pull-in come regola generale di Odyssey è fuorviante.
- **Riga 31 (Mario 3D World)**: *Imprecisione/Errore*. "Camera fissa senza zoom". FALSO. 3D World non è a camera completamente fissa: offre 3 scatti angolari discreti tramite D-pad (sinistra, centro, destra) e micro-inclinazione/zoom fine controllabile tramite il giroscopio del controller.
  - *FONTE*: Manuale e mappatura comandi di Super Mario 3D World (Wii U / Switch).
  - *OPINIONE*: Definirla "senza zoom" ne travisa la natura di camera guidata con micro-regolazioni dell'utente.
- **Righe 33-38 (Nesky vs Haigh-Hutchinson)**: *Errore di attribuzione storica*. L'uso dei "baffi" (raycast a ventaglio / whiskers) per anticipare gli ostacoli e la distinzione tra pareti e colline tramite normale di contatto non provengono da John Nesky (GDC 2014, focalizzato su 50 errori matematici/comportamentali), bensì dal testo fondamentale di Mark Haigh-Hutchinson (*Real-Time Cameras*, 2009, cap. 6-7). Inoltre *The Last Guardian* (citato a riga 39) è uscito a fine 2016, quindi posteriore al talk di Nesky del 2014.
  - *FONTE*: J. Nesky, GDC 2014; M. Haigh-Hutchinson, *Real-Time Cameras* (Morgan Kaufmann, 2009).
  - *OPINIONE*: Distinguere chiaramente le fonti teoriche per non attribuire paternità errate.
- **Riga 24 (DK Bananza - Opzioni)**: *Dato incompleto/superato*. "Le opzioni sono solo sensibilità orizzontale/verticale, giroscopio, scuotimento". A seguito delle forti critiche per motion sickness durante lo scavo nei cunicoli, la patch v3.0.0 di Nintendo ha introdotto il toggle per disattivare l'auto-follow e il ricentraggio su stick (Z-targeting).
  - *FONTE*: Patch notes v3.0.0 DK Bananza; ricerca agy §2.
  - *OPINIONE*: Omettere la patch fa perdere la soluzione progettuale esatta adottata da Nintendo contro la nausea.
- **Riga 17 (Astro Bot - Tasto O)**: *Imprecisione*. "Il tasto O ricentra, tenendolo premuto lo stick sinistro muove la camera". In Astro Bot il tasto O standard è lo Spin Attack (pugno rotante). Il ricentraggio o la guida a una sola mano citata in F3 è un preset specifico di Accessibilità per mobilità ridotta, non lo schema comandi standard.
  - *FONTE*: Schema comandi DualSense Astro Bot PS5; recensione Access-Ability.
  - *OPINIONE*: Specificare che si tratta di una funzione di accessibilità, non del mapping predefinito.

---

### 2. Cose che la ricerca ha trovato e il documento no (Top 8 per importanza)
1. **Disaccoppiamento da Z-up per gravità arbitraria**: Gameplay Cameras di UE 5.9 assume un mondo Z-up; gestire gravità su pareti e superfici a 360° (core di Giustino) richiede frame locale su Up con quaternioni (`FQuat::Slerp`) e parallel transport.
   - *FONTE*: Ricerca agy §7; architettura Mario Galaxy / `FGSTN_CameraMath`. *OPINIONE*: Elemento cardine per non rompere la camera sul wall-run.
2. **Input Latching dopo tagli o cambi di inquadratura**: Quando l'inquadratura stacca o cambia bruscamente (aree Fixed/Rail), la direzione vettoriale dell'input dello stick deve rimanere memorizzata (latched) fino al rilascio per evitare inversioni e cadute accidentali.
   - *FONTE*: Nesky (2014); Haigh-Hutchinson (2009). *OPINIONE*: Requisito fondamentale citato solo nominalmente (StickHold) ma non dettagliato.
3. **Ombra di contatto netta (Drop/Blob Shadow)**: La leggibilità del salto in Astro Bot è garantita in modo inscindibile dalla proiezione verticale di un'ombra di contatto netta a terra per giudicare profondità e atterraggio.
   - *FONTE*: N. Doucet, GDC 2019; Ricerca agy §8.2. *OPINIONE*: Senza ombra dedicata a terra, nessuna taratura di camera basterà.
4. **Isteresi verticale e Ground Anchoring avanzato**: In Astro e Odyssey la quota verticale è ancorata al suolo di stacco con isteresi temporale e filtro passa-basso all'atterraggio per non sobbalzare su gradini o asperità del terreno.
   - *FONTE*: K. Hayashida, GDC 2012; Ricerca agy §3. *OPINIONE*: Una deadzone fissa a 50 cm senza filtro temporale genera instabilità.
5. **Clamp angolare massimo rigido sugli Hint/Focus Points (max 25°-35°)**: I punti di focus non devono mai deviare la vista oltre 25°-30° rispetto alla direzione di marcia e devono inibirsi se il punto è alle spalle del personaggio.
   - *FONTE*: Cinemachine Target Group; Ricerca agy §5. *OPINIONE*: Superare tale soglia senza togliere i comandi induce forte cinetosi.
6. **Inibizione degli automatismi in aria/salto**: Sospensione automatica di correzioni angolari o bias di focus durante salti acrobatici di precisione per non falsare la prospettiva del giocatore.
   - *FONTE*: Nesky (2014); Ricerca agy §5. *OPINIONE*: Un focus point che sterza durante un salto causa cadute frustranti.
7. **Preset discreti storici vs slider continuo**: L'uso di preset a scatti discreti (es. Vicina 0.8x / Media 1.0x / Lontana 1.25x stile Mario 64 o Ratchet & Clank) è superiore allo slider continuo perché preserva inquadrature note al level designer.
   - *FONTE*: Menu Mario 64, Rift Apart, Psychonauts 2. *OPINIONE*: Lo slider continuo in gioco crea combinazioni visive incontrollate.
8. **Asimmetria dello smorzamento orizzontale**: Utilizzo di smorzamento asimmetrico nell'inseguimento orizzontale (più cedevole in accelerazione, più reattivo in frenata) per comunicare il peso e l'inerzia del personaggio.
   - *FONTE*: Ricerca agy §8.3. *OPINIONE*: Una molla simmetrica impoverisce il feeling di peso di un personaggio robusto.

---

### 3. Architettura (§3.1): Plugin proprio vs Gameplay Cameras UE 5.9 e Rischi
- **Verdetto**: Ha PIENAMENTE SENSO usare il plugin proprio `GSTNCamera` con l'Anchor come soggetto (`IGSTN_CameraSubject`).
- **Motivazione**: Gameplay Cameras in UE 5.9 è ancora soggetto a breaking changes tra versioni, assume Z-up nei nodi stock (inadatto alla gravità a 360° di Giustino), e imporrebbe di riscrivere a nodi ciò che `GSTNCameraCore` ha già implementato e validato con 44 test headless. L'Anchor isola perfettamente la pulizia del pivot dalla logica d'inquadratura.
- **Rischi non visti o sottovalutati**:
  1. *Ordine dei tick e Jitter con Network Prediction (C1)*: Se l'Anchor aggiorna il pivot in `TG_PostUpdateWork` (per seguire l'interpolazione della simulazione o le animazioni) e `UGSTN_CameraComponent` o il `PlayerCameraManager` vengono valutati in un gruppo precedente, la camera userà il pivot del frame precedente. A 60/120 Hz questo genera micro-scatti visibili. L'aggiornamento deve essere esplicitamente vincolato in `APlayerCameraManager::UpdateViewTarget` leggendo il pivot consolidato.
  2. *Control Rotation e Abilità GAS (C4)*: `GSTNCamera` gestisce un'orbita autonoma (`FGSTN_CameraOrbitState`). Se non c'è una politica chiara di sincronizzazione con `AController::ControlRotation`, tutte le abilità GAS, proiettili e logiche di locomozione orientate al controller (`bUseControllerRotationYaw` / `GetControlRotation()`) leggeranno una rotazione disallineata rispetto alla visuale effettiva.
  3. *Listen Server / Host locale*: Sull'host di un listen server, il PlayerController 0 condivide il processo del server; il codice camera non deve toccare attori di gameplay senza controlli di net-role. Inoltre, una camera puramente locale senza interfaccia standard complica future feature di Replay o Spettatore.
  - *FONTE*: Architettura Unreal Engine (`APlayerCameraManager`, `TG_PostUpdateWork`); `README.md` di `GSTNCamera`.
  - *OPINIONE*: La risoluzione dello spike C4 (Control Rotation) è la priorità architetturale assoluta per non bloccare il combat.

---

### 4. Distanza regolabile (§3.3): Modello e Valori
- **Verdetto sul modello**: BUONO il principio moltiplicativo (`Distanza = Base * Scala`) con clamp d'area per evitare compenetrazioni negli spazi angusti.
- **Critica ai valori e all'interfaccia**:
  - *Conflitto Preset vs Slider*: Avere 3 preset a D-pad E uno slider continuo 0.6-1.6 nelle opzioni crea un conflitto di stato e ridondanza inutile. Raccomandazione: lo slider delle opzioni deve semplicemente configurare o rispecchiare i 3 preset a scatti discreti.
  - *Valori eccessivi (0.6x - 1.6x)*: Con distanza base a 650 cm, 1.6x porta l'arm a 1040 cm (oltre 10 metri: personaggio minuscolo e continue collisioni col soffitto); 0.6x su 400 cm porta a 240 cm (troppo ravvicinata per il platforming). Gamma sensata e collaudata: **0.8x - 1.25x** (Vicina 0.8x, Media 1.0x, Lontana 1.25x).
  - *Tempo di transizione 0.3s*: Troppo secco se attivato accidentalmente durante un salto; meglio **0.4 - 0.5s** con curva EaseInOut.
  - *Mappatura D-Pad*: La croce direzionale è preziosa per emote, abilità o consumabili; ottima la scelta di tenerla su Input Action disaccoppiata.
  - *FONTE*: Standard ergonomici platform 3D (Rift Apart, Psychonauts 2).
  - *OPINIONE*: Eliminare lo slider continuo in game: 3 preset a scatto sono la soluzione più pulita e a prova di bug.

---

### 5. Punti di focus (§3.4): Tassonomia, Mancanze e Rischio Nausea
- **Verdetto**: La divisione `Frame` / `Look` / `Reveal` e `Soft` / `Firm` / `Lock` è OTTIMA, pulita e ricalca le migliori pratiche (Cinemachine Target Group, Mario Galaxy `wpoint`).
- **Parametri/Casi d'uso mancanti**:
  1. *Inibizione in volo (`bSuppressInAir`)*: Il Look-bias deve disattivarsi automaticamente quando il personaggio è in salto o caduta per non alterare il calcolo della traiettoria parabolica.
  2. *Cono di ingaggio frontale (`MaxEngageAngleDeg`)*: Un focus point deve attivarsi SOLO se il punto rientra in un cono visivo utile (es. entro 60° dal forward del personaggio); se il punto viene oltrepassato o è alle spalle, non deve voltare la camera all'indietro.
  3. *Smorzamento target mobile (`TargetDampingSeconds`)*: Se il punto è vincolato a un nemico in corsa, serve un filtro per non trasmettere sobbalzi bruschi alla telecamera.
- **Fattori di nausea e fastidio**:
  1. *Rubber-banding*: La modalità "Firm" non deve mai combattere attivamente contro lo stick del giocatore; contrastare la mano genera frustrazione immediata.
  2. *Rotazioni a 180° violente*: Se il giocatore oltrepassa un punto di focus, la camera rischia di girarsi repentinamente indietro (errore Bananza).
  3. *Oscillazione (Hunting)*: In presenza di punti multipli contigui, serve un'isteresi spaziale prima di trasferire l'influenza da un punto all'altro.
  - *FONTE*: Nesky GDC 2014; documentazione Unity Cinemachine; Ricerca agy §5.
  - *OPINIONE*: L'hint deve solo "suggerire": se il giocatore impugna la levetta, il controllo manuale deve prevalere sempre.

---

### 6. Modalità Platform (§3.2) e Valori (§4) vs Comportamento Reale Astro/Odyssey
- **Verdetto**: Alcune assunzioni e parametri contraddicono il reale comportamento fluido di Astro Bot e Mario Odyssey.
- **Contraddizioni riscontrate**:
  1. *Rotazione automatica "spenta di default" (Astro)*: Errore d'interpretazione. In Astro Bot l'auto-rotazione non è zero: durante la corsa continua in curva la telecamera esegue un morbido allineamento inerziale dietro al personaggio per non costringere a continui aggiustamenti di stick nei tornanti. Spegnerla al 100% crea una camera rigida e punitiva nei percorsi 3D non rettilinei.
  2. *`FallFollowCm` a 50 cm*: 50 cm in Unreal Engine equivalgono a due gradini. Far scendere la telecamera istantaneamente dopo soli 50 cm di discesa provocherà continui sobbalzi verticali su scale e dossi. In Odyssey e Galaxy la deadzone inferiore è di almeno **120-150 cm** o filtrata da un'isteresi temporale (es. discesa solo dopo 0.3s di caduta libera).
  3. *Riallineamento all'atterraggio a 0.2s*: Troppo rapido. Un riallineamento in 200 ms genera uno scatto verticale sgradevole ("camera pop"). Astro e Odyssey impiegano **0.5 - 0.8s** con smorzamento esponenziale.
  4. *Pitch automatico in caduta (+12°)*: Con pitch base a 18°-25°, +12° portano la vista a 30°-37° (quasi zenitale). In Odyssey il pitch verso il basso è proporzionale alla velocità verticale di caduta, non un gradino fisso che scatta al superamento del bordo.
  - *FONTE*: Analisi video a fotogrammi di Astro Bot e Mario Odyssey; parametri BCSV Galaxy (`pushdelay`, `lplay`).
  - *OPINIONE*: Portare subito `FallFollowCm` ad almeno 120 cm per evitare la cinetosi sui piccoli dislivelli.

---

### 7. Ostacoli (§3.5): Retino, Silhouette e Pull-In su Hardware Debole
- **Verdetto**: Scelta concettualmente valida ma con RISCHI CRITICI di prestazioni su hardware debole (low-spec PC / Steam Deck / Switch 2).
- **Analisi tecnica**:
  1. *Retino (Dithered Opacity)*: Su GPU con basso fill-rate, il materiale Masked con `DitherTemporalAA` disabilita l'Early-Z pass / Nanite write ottimizzato e genera pesanti cali di frame rate per overdraw se grandi pareti entrano contemporaneamente in dissolvenza. Il retino va riservato a prop leggeri (colonne, rami, cartelli), NON usato come sostituto universale della collisione.
  2. *Silhouette (CustomDepth / Post-Process)*: Il rendering a raggi X della silhouette richiede la scrittura nel buffer `CustomDepth` e un pass post-process fullscreen aggiuntivo. Su hardware modesto questo comporta un costo fisso (0.5 - 1.0 ms GPU). Deve essere reso opzionale e disattivabile nei profili grafici bassi.
  3. *Pull-In come prima difesa*: Per grandi pareti di roccia e muri maestri, lo sphere-sweep che accorcia dolcemente la camera (pull-in smorzato) resta la tecnica più performante e a costo zero per la GPU.
  4. *Costo CPU Collision*: La combinazione di una sweep sferica + 2-4 raggi (whiskers) a frame solo per il client locale ha un impatto CPU trascurabile (< 0.05 ms).
  - *FONTE*: Architettura rendering Unreal Engine (Nanite Early-Z, CustomDepth); Ricerca agy §7.
  - *OPINIONE*: Utilizzare il pull-in per i volumi principali del livello e limitare il dither ai soli prop marcati dal tag `GSTN.CameraFade`.

---

### 8. Piano e Costi (§5): Valutazione Fasi e Tagli Proposti
- **Verdetto**: Fasi ben articolate; la sequenza P0->P4 è logica, ma P3 va ridimensionata e anticipata.
- **Cosa togliere o spostare**:
  1. *Eliminare il flag `Enclosed` da P3*: Implementare la camera sotterranea a 360° con voxel trasparenti stile Bananza è del tutto prematuro per un progetto che non ha ancora meccaniche di scavo. Risparmio: **~3-4 $**.
  2. *Posticipare la Silhouette Post-Process a P5*: Il solo retino dithered sui prop è sufficiente per il playtest di P4; la silhouette è un vezzo cosmetico che appesantisce lo shader pipeline. Risparmio: **~3-4 $**.
  3. *Invertire parzialmente P2 e P3*: Integrare la gestione ostacoli (pull-in pulito e dither base) PRIMA dei punti di focus (P2). Testare aree e focus point in un livello senza collisioni funzionanti falsa la taratura visiva.
  - *FONTE*: Analisi costi/fasi in `design.md` §5.
  - *OPINIONE*: Tagliando `Enclosed` e silhouette, il budget totale P0-P4 scende realisticamente a **~32-42 $**.

---

### 9. Decisioni per l'Owner (§8): Valutazione e Decisione Mancante
- **Valutazione opzioni esistenti**:
  - *Decisione 1 (Su cosa costruire)*: **Opzione A (consigliata)** sacrosanta. `GSTNCamera` è già nel repo, gestisce l'Up arbitrario e ha 44 test; scartare Gameplay Cameras di Epic.
  - *Decisione 2 (Rotazione automatica)*: **Opzione A (consigliata)** ragionevole, purché si intenda "come Astro" (inerziale blanda solo in corsa prolungata, spenta da fermi) e non un blocco totale a 0°/s.
  - *Decisione 3 (Piano e tempi)*: **Opzione A (consigliata)** prudente ed efficace per non interferire con il ciclo di locomozione in corso.
- **Decisione fondamentale MANCANTE**:
  - **Autorità della Rotazione (`ControlRotation` vs `CameraOrbit`)**: Manca la decisione cardine: la telecamera comanda la `ControlRotation` del Controller o ne è completamente svincolata? Se è svincolata, come mirano le abilità GAS del giocatore (proiettili, attacchi orientati) e dove si orienta il Character Movement? Questa scelta architetturale deve essere deliberata dall'owner prima di iniziare P0.
  - *FONTE*: Architettura PlayerController / GAS / CharacterMovement in UE.
  - *OPINIONE*: Mancare di chiarire la relazione con la `ControlRotation` espone al rischio di dover riscrivere l'interfaccia di mira in P1.

---

### 10. Le tre modifiche più utili al documento
1. **Definire la sincronizzazione tra orbita della telecamera e `ControlRotation` per garantire la mira coerente delle abilità GAS.**
2. **Portare `FallFollowCm` da 50 cm a 120-150 cm e calibrare l'auto-rotazione come morbido allineamento inerziale solo in corsa continua.**
3. **Eliminare la modalità sotterranea (`Enclosed`) e concentrare P3 sul solo dithering leggero per prop, rinviando la silhouette a post-process.**

---

### Metodologia e Limiti della Review
- **Tempo impiegato**: Circa 45 minuti per analisi incrociata del documento `design.md`, riscontro sorgenti `Plugins/GSTNCamera/` e confronto con la ricerca indipendente.
- **Cosa non è stato possibile verificare**:
  - Implementazione sorgente di `UGSTN_CameraAnchorComponent` e `DA_CameraAnchor_Default` (presenti sul branch `origin/claude/ue59-sync-1119` e non sincronizzati nel worktree locale).
  - Test interattivo e resa visiva a schermo dei profili in Unreal Editor (rispettato il vincolo di non avviare l'editor e non modificare asset).
