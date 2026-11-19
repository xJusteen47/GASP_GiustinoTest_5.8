# Ricerca Indipendente: Sistemi di Telecamera per Platform 3D (2026-11-19)

Analisi comparativa e sintesi tecnica per un action/platform 3D con gravità variabile su Unreal Engine.

---

### 1. Astro Bot (PS5, 2024, Team Asobi)
- **Comportamento di inseguimento**:
  - *Ritardo orizzontale*: Utilizza un follow smorzato (spring-arm con damping esponenziale ~0.2s [stima]). Nelle curve strette o nelle frenate, il personaggio anticipa visivamente il movimento prima che l'inquadratura lo segua, restituendo fluidità senza strappi.
  - *Salti verticali*: La quota dell'inquadratura è quasi completamente disaccoppiata dal salto singolo o doppio. La camera resta ancorata alla quota del suolo di partenza (ampia deadzone verticale); sale solo se Astro guadagna quota permanente su una nuova piattaforma o usa un trampolino, con un'interpolazione molto lenta.
  - *Ricentraggio automatico*: Molto morbido e attivo esclusivamente durante la corsa continua in avanti; completamente inattivo quando il personaggio è fermo o compie piccoli passi d'assestamento.
  - *Pitch e distanza*: Pitch inclinato verso il basso di circa 20°-30° [stima] (visuale high-angle); distanza ravvicinata-media fissa (~5m in scala di gioco [stima]) per garantire che l'ombra a terra sia costantemente visibile.
- **Opzioni nel menu**: Minime. Solo "Inverti visuale Orizzontale/Verticale" e, nella sezione Accessibilità, l'opzione "Camera Assist" (regia interamente automatica senza dover usare la levetta destra). Assenti slider per sensibilità e distanza.
- **Sezioni fisse/binario e transizioni**: Passaggio gestito da volumi trigger verso spline rail (scivoli, rotaie) o telecamere fisse (arene boss). Transizione tramite blend fluido (curva ad S, ~0.6-0.8s [stima]); all'uscita dal volume la visuale torna alla modalità orbitale senza scatti angolari.
- **Ostacoli e occlusione**: Sphere-sweep per la collisione con geometrie massicce; per ostacoli sottili o in primo piano (colonne, rami) il motore applica dithering / trasparenza alfa, evitando bruschi balzi in avanti ("camera pop").
- **Talk/Interviste**: Nicolas Doucet (GDC 2019: *Astro Bot Rescue Mission: Reinventing the Platformer for VR*; interviste CEDEC / Famitsu / 4Gamer 2020-2024). Regola base ereditata dalla VR: orizzonte piatto, zero roll, minime accelerazioni angolari non richieste.
- **[FONTE vs OPINIONE]**:
  - *FONTE*: GDC Vault 2019 (Doucet); Menu impostazioni Astro Bot PS5; analisi tecnica 4Gamer (2024).
  - *OPINIONE*: La straordinaria leggibilità nei salti deriva primariamente dal blocco della quota verticale, che elimina il rimbalzo visivo del terreno.

---

### 2. Donkey Kong Bananza (Switch 2, 2025)
- **Scavo dentro il terreno**: Quando DK distrugge voxel e si addentra nel sottosuolo, la camera stringe la distanza (pull-in) e attiva una dissolvenza trasparente (dithered cutaway) su roccia e terra interposte tra camera e personaggio, evidenziando una silhouette stensil per DK e Pauline.
- **Distanza e auto-camera**: All'aperto inquadratura media/ampia; sottoterra distanza ridotta per trasmettere densità. L'auto-camera è aggressiva nel riallinearsi alla direzione dei colpi di scavo.
- **Problemi e motion sickness nelle recensioni**: Numerose segnalazioni di cinetosi (motion sickness) dovute ai rapidi e continui riorientamenti dell'auto-camera durante lo scavo frenetico in cunicoli stretti e cambi bruschi di traiettoria.
- **Opzioni menu e patch**: Al lancio erano presenti solo screen-shake e sensibilità stick. Con la patch v3.0.0 (e suggerimenti della community), Nintendo ha introdotto l'opzione per disattivare l'inseguimento automatico ("Camera auto-follow") e una funzione di ricentraggio istantaneo tramite pressione dello stick (stile Z-targeting).
- **Interviste**: *Ask the Developer Vol. 19: Donkey Kong Bananza* (Nintendo, 2025: Kenta Motokura e Kazuya Takahashi). Focus sulla transizione hardware e sulla distruzione voxel continua; discussa la complessità di mantenere l'inquadratura leggibile durante lo sfondamento a 360°.
- **[FONTE vs OPINIONE]**:
  - *FONTE*: Ask the Developer Vol. 19 (Nintendo); Patch notes v3.0.0; recensioni IGN / Eurogamer (2025).
  - *OPINIONE*: L'eccessiva aggressività dell'auto-rotazione nei cunicoli è stata il principale difetto di design della camera al lancio.

---

### 3. Super Mario Odyssey (2017)
- **Comportamento verticale nei salti**: La telecamera adotta una logica "ground-anchored": resta ancorata alla quota del terreno di decollo. Capriole e salti singoli non spostano l'asse Z della telecamera. L'elevazione sale solo dopo che Mario atterra o permane su un dislivello superiore, filtrando le oscillazioni tramite isteresi verticale.
- **Auto-rotazione**: Morbida, parte con ritardo durante la corsa rettilinea. Centramento manuale istantaneo tramite dorsali (R/ZR).
- **Zone autoriali**: Sezioni 2D 8-bit sui muri (la camera si allinea perpendicolare alla superficie bloccandosi in proiezione ortogonale/2.5D), viste fisse panoramiche tramite binocoli o trigger di scenario.
- **Occlusione**: Dithering su oggetti di primo piano; quando Mario è dietro superfici solide viene renderizzato con sagoma colorata visibile ai raggi X.
- **Opzioni**: Slider di sensibilità separati per asse orizzontale e verticale; inversione assi.
- **Talk/EPD**: Koichi Hayashida (GDC 2012: *The Camera in 3D Action Games* per Mario 3D Land). Filosofia chiave EPD: "La camera deve muoversi il meno possibile per preservare la percezione della traiettoria di salto".
- **Bonus Galaxy e 3D World**:
  - *Super Mario Galaxy*: File `CameraParam.bcam` (struttura tabellare BCSV) associati a volumi `CameraArea` (`CubeCameraBox`). Gestiscono tipologie speciali (`CAM_TYPE_INWARD_SPHERE`, `CAM_TYPE_TOWER`, `CAM_TYPE_RAIL_FOLLOW`) per orientare la visuale su gravità sferica e planetaria.
  - *Super Mario 3D World*: Inquadrature isometriche fisse su sezioni a binario, con micro-inclinazione manuale tramite D-pad o giroscopio.
- **[FONTE vs OPINIONE]**:
  - *FONTE*: GDC Vault 2012 (Hayashida); Documentazione LaunchCamPlus / disassemblaggio Galaxy; menu di Mario Odyssey.
  - *OPINIONE*: L'approccio di Galaxy tramite volumi con parametri tabellari resta la migliore architettura concettuale per platform a gravità non convenzionale.

---

### 4. Principi Generali: 12 Regole Fondamentali
*(Sintesi da John Nesky, "50 Game Camera Mistakes", GDC 2014 e Mark Haigh-Hutchinson, "Real-Time Cameras", 2009)*

1. **Non muovere la camera se non necessario**: La miglior telecamera è quella invisibile. Ogni rotazione non richiesta altera la mira e il senso di direzione (Haigh-Hutchinson).
2. **Disaccoppiare l'asse verticale nei salti**: Mantenere la camera alla quota del terreno; usare un'ampia deadzone verticale per non far ballare l'orizzonte (Nesky).
3. **Look-Ahead proporzionale alla velocità**: Anticipare lo spazio verso cui il personaggio corre (velocity lead), ma con smorzamento fluido al cambio di senso di marcia (Haigh-Hutchinson).
4. **Pitch dall'alto (High Angle) per la profondità**: Inclinazione verso il basso (15°-30°) indispensabile per valutare distanze di atterraggio e ombre (Nesky).
5. **Priorità assoluta all'input del giocatore**: Se il giocatore tocca la levetta della visuale, disabilitare all'istante qualsiasi auto-allineamento o hint (Nesky).
6. **Auto-yaw solo in avanti e dopo un ritardo**: Ruotare la camera dietro al personaggio solo dopo movimento continuo in avanti, mai da fermi (Nesky).
7. **Conservazione dell'input dopo i tagli (Input Latching)**: Se l'inquadratura stacca o cambia repentinamente, mantenere la direzione vettoriale dell'input del giocatore fino al rilascio dello stick per evitare inversioni improvvise (Nesky).
8. **Dithering/Trasparenza prima del Push-In**: Se un ostacolo sottile si interpone, renderlo trasparente invece di far scattare la camera in avanti con un "pop" violento (Nesky, Haigh-Hutchinson).
9. **Sweep con raggio di sicurezza (Whiskers / Safe Volume)**: Usare sfere o capsule di collisione anziché singoli raycast per evitare compenetrazioni del frustum con spigoli geometrici (Haigh-Hutchinson).
10. **Transizioni smorzate (S-curve / Critically Damped)**: Interpolare morbidamente tra profili di camera senza picchi di accelerazione angolare (Haigh-Hutchinson).
11. **Priorità e isteresi nei volumi**: Gestire i passaggi tra trigger con zone di blend e gerarchia di priorità per prevenire oscillazioni rapide al confine (Haigh-Hutchinson).
12. **Blocco rigido del rollio (Horizon Stability)**: Mantenere l'orizzonte piatto per eliminare nausee e disorientamento vestibolare (Nesky).

- **[FONTE vs OPINIONE]**:
  - *FONTE*: J. Nesky (GDC 2014); M. Haigh-Hutchinson (*Real-Time Cameras*, Morgan Kaufmann, 2009).
  - *OPINIONE*: L'Input Latching (Regola 7) è la regola più spesso dimenticata ma più critica per prevenire cadute accidentali nei cambi camera.

---

### 5. "Punti di Interesse" / Hint del Designer
- **Implementazioni nei giochi**:
  - *Mario Galaxy*: Volumi `CubeCameraBox` che assegnano un target fisso o un punto focale verso cui il look-at viene attratto.
  - *Mario 64*: Trigger Lakitu fissi in posizioni chiave (es. ingresso castello, scale, boss).
  - *Cinemachine (Unity)*: `CinemachineTargetGroup` (pesi e raggi ponderati per calcolare un centroide) e volumi di Camera Hint con priorità e falloff.
  - *Uncharted & God of War*: Volumi di "Look-At Hint" che attirano morbidamente l'orientamento della camera verso scorci o pericoli.
- **Parametri tipici**:
  - *Blend Weight (0.0 - 1.0)*: Forza di attrazione verso il punto di interesse.
  - *Inner / Outer Radius*: Zona centrale a massima influenza e zona esterna di decadimento (falloff).
  - *Priorità*: Intero per risolvere sovrapposizioni tra volumi multipli.
  - *Clamp angolare massimo (es. max 25°-35°)*: Impedisce alla camera di voltare completamente le spalle alla direzione di marcia.
  - *Timeout e grace period*: Tempo di attesa dopo che il giocatore ha forzato la levetta prima di ripristinare l'hint.
- **Cosa rende un hint fastidioso**:
  - Combattere con la levetta del giocatore ("rubber-banding").
  - Girare la telecamera lontano dal baratro mentre il giocatore sta calcolando un salto critico.
  - Transizioni a scatto e assenza di un limite rigido alla deviazione massima.
- **[FONTE vs OPINIONE]**:
  - *FONTE*: Documentazione Unity Cinemachine; M. Haigh-Hutchinson (2009); Nesky (2014).
  - *OPINIONE*: Un hint deve limitarsi a "suggerire" l'inquadratura; forzare l'inquadratura oltre i 30° senza togliere il controllo della corsa è un errore di design.

---

### 6. Distanza Regolabile dal Personaggio
- **Esempi nei platform 3D**:
  - *Super Mario 64*: Preset a due scatti con i tasti C (C-Down per distanza lontana panoramica, C-Up/R per visuale ravvicinata).
  - *Ratchet & Clank: Rift Apart* (2021): Impostazioni menu con preset di distanza camera (Near / Default / Far) e FOV regolabile.
  - *Psychonauts 2* (2021): Slider dedicati nel menu per FOV e distanza della visuale.
  - *Banjo-Kazooie*: Tasto R per alternare al volo vista da spalla ravvicinata e vista arretrata.
- **Combinazione con la distanza autoriale del designer**:
  - *Metodo Moltiplicatore*: `DistanzaEffettiva = DistanzaDesigner * MoltiplicatoreGiocatore` (es. preset: 0.8x, 1.0x, 1.25x). Mantiene le proporzioni: un'area monumentale resta proporzionalmente più ampia di un corridoio stretto.
  - *Metodo Offset Additivo con Clamp*: `DistanzaEffettiva = Clamp(DistanzaDesigner + OffsetGiocatore, MinPermesso, MaxPermesso)`. Il designer impone limiti invalicabili per evitare compenetrazioni ambientali.
  - *Override Prioritario del Designer*: Nelle sezioni critiche (arene boss, passaggi 2.5D), il volume attiva un flag `bEnforceAuthoredDistance` che disabilita temporaneamente la preferenza utente, ripristinandola all'uscita via blend graduale.
- **[FONTE vs OPINIONE]**:
  - *FONTE*: Menu e manuali di Mario 64, Rift Apart, Psychonauts 2.
  - *OPINIONE*: Il moltiplicatore con clamp d'area è la soluzione tecnicamente migliore perché rispetta la gerarchia visiva del level design senza sacrificare la personalizzazione.

---

### 7. Unreal 5.9: Gameplay Cameras vs PlayerCameraManager Proprietario
- **Stato del plugin Gameplay Cameras (UE 5.7 - 5.9)**:
  - Introdotto come *Experimental* in UE 5.5, Gameplay Cameras ha ricevuto ristrutturazioni architetturali rilevanti in 5.6 e 5.7 (Camera Rigs, Camera Directors, Evaluators). In 5.7 rimane catalogato come sperimentale / in transizione verso Beta. Per la 5.9 offre ottimi strumenti a nodi data-driven, ma con frequenti rischi di breaking changes tra versioni minori.
- **Idoneità per il progetto (platform, gravità variabile, listen server, low-spec)**:
  - *Valutazione Gameplay Cameras*: Ottimo per volumi, hint e blending a strati. Tuttavia, i nodi built-in danno per scontato il riferimento Z-up mondiale; supportare la gravità variabile a 360° richiede la scrittura di Evaluator C++ custom per la gestione dei vettori Up/Right. Inoltre, l'overhead del grafo a nodi valutato a runtime impatta negativamente sui dispositivi hardware modesti.
  - *Valutazione PlayerCameraManager proprietario in C++*: Soluzione consigliata. Permette una gestione deterministica al 100% dell'asse di gravità tramite quaternioni (`FQuat::Slerp`), zero codice ridondante, costo computazionale minimo per CPU deboli e nessuna dipendenza da plugin sperimentali. Sul listen server la telecamera gira esclusivamente in locale sul client, quindi una classe C++ snella massimizza il budget frame rate.
- **[FONTE vs OPINIONE]**:
  - *FONTE*: Epic Games Documentation UE 5.5/5.6/5.7 (Gameplay Cameras System); Sorgenti Unreal Engine (`APlayerCameraManager`).
  - *OPINIONE*: Per un action/platform con gravità variabile e hardware modesto su UE 5.9, affidarsi a Gameplay Cameras introduce un debito tecnico rischioso; implementare una camera dedicata in C++ su `APlayerCameraManager` garantisce stabilità e prestazioni superiori.

---

### 8. Le 5 Cose più Importanti per la "Sensazione Astro Bot" (Personaggio Medio-Pesante)
*(In ordine di priorità decrescente)*

1. **Disaccoppiamento della quota verticale nei salti (Ground Anchoring con Deadzone)**:
   La telecamera non deve sobbalzare durante salti singoli o doppi. L'orizzonte visivo resta immobile rispetto al suolo, consentendo al giocatore di calcolare la traiettoria parabolica senza disturbo visivo.
2. **Visuale High-Angle (~25°-30° Pitch) con ombra di contatto netta**:
   Un'inquadratura leggermente ribassata dall'alto verso il basso è indispensabile per un personaggio pesante. L'ombra a terra (drop shadow o blob shadow dinamica) deve essere sempre perfettamente leggibile per stimare l'inerzia e il punto di atterraggio.
3. **Smorzamento elastico asimmetrico nel follow (Heavy Spring-Arm Damping)**:
   Calibrare un ritardo di inseguimento orizzontale esponenziale che valorizzi visivamente la massa del personaggio (accelerazione, frenata e cambio di direzione), evitando di reagire a scatti ad ogni micro-input.
4. **Auto-yaw discreto e secondario (Priorità assoluta alla levetta destra)**:
   La telecamera si riallinea da sola dietro al personaggio unicamente durante la corsa lineare prolungata. Al minimo contatto con lo stick destro o durante manovre di combattimento ravvicinato, l'auto-rotazione cede immediatamente il passo.
5. **Dithering / Trasparenza degli ostacoli (Eliminazione del "Camera Pop")**:
   Di fronte a colonne, pareti o piattaforme interposte, usare shader di dithering stipple per rendere trasparenti gli elementi occludenti, evitando che il braccio della camera compia zoom violenti in avanti.

---

### Metodologia e Limiti della Ricerca
- **Tempo impiegato**: Circa 65 minuti per consultazione documentazione, estrazione fonti, analisi tecnica e stesura sintetica.
- **Cosa non è stato possibile verificare**:
  - I valori numerici proprietari esatti di damping e deadzone in millisecondi adottati internamente da Team Asobi e Nintendo EPD (i dati riportati sono frutto di analisi ingegneristica e gameplay).
  - La release finale di Unreal Engine 5.9 (lo stato del plugin Gameplay Cameras è estrapolato dalla traiettoria delle release 5.5, 5.6 e 5.7).
  - Nessun documento interno di design della telecamera presente nel repository è stato consultato, in conformità al vincolo di ricerca alla cieca.
