# Telecamera da platform (Astro Bot / DK Bananza / Mario Odyssey): studio e design per GSTN
Thread cloud, Opus high, 2026-11-19. Richiesta owner 17:27Z (cmsg_01CRvUdrWm1dJrM4q54YDBLNS4hWh2JaDAF57c2RCWbav6): "telecamera alla Astro Bot / Bananza / Odyssey, con distanza regolabile dal personaggio ed eventualmente punti di autofocus decisi dal designer; la ADS è diversa". Solo studio e proposta: **niente codice**.
Ambito (decisione owner 11-17, 1A): spedizioni e livelli. Arene PvP e ADS restano come sono.
Legenda: **[F]** fonte (lista in fondo), **[A]** analisi mia da memoria dei giochi o deduzione, da verificare su video. **[R]** codice del repo (branch `claude/ue59-sync-1119`).
Stato peer review Gemini: ricerca alla cieca e review in corso sul desktop (§9); questo documento verrà aggiornato con i risultati.

## 0. In breve
- I tre giochi fanno la stessa cosa di fondo: **la telecamera è calma e lascia leggere il salto**, il designer la guida per tratti, e quando il mondo copre il personaggio **è il mondo a sparire, non la telecamera a saltare**. Differiscono su quanto la camera gira da sola e su quanto è vicina.
- **Nel repo c'è già quasi tutto lo scheletro**: il plugin `GSTNCamera` (pila di modalità, aree del designer `AGSTN_CameraSection` con Orbit/FixedPoint/Rail, modalità Free "alla Astro", fade degli ostacoli, `DistanceScale` nello stato dell'orbita) **non è mai stato collegato a un personaggio** [R README]. La camera che si vede oggi è quella del GASP (Gameplay Cameras) più il nostro `UGSTN_CameraAnchorComponent` (pivot indipendente dalla capsula, "air hold", tilt-shift) [R].
- **Proposta:** il plugin `GSTNCamera` diventa il cervello della camera da platform; l'anchor diventa il suo "soggetto" (fornisce il pivot già pulito da capsula e crouch). Mancano tre pezzi nuovi: **distanza regolabile dal giocatore**, **punti di focus del designer** (`AGSTN_CameraFocusPoint`), **occlusione a retino + silhouette**. Più due correzioni: impostazioni del plugin spostate da Project Settings a Data Asset (regola owner) e un solo "air hold" invece di due.
- Costo stimato P0-P4: **~44-58 $**, 5 thread. Tre decisioni per l'owner in §8.

## 1. Come funzionano le tre telecamere
### 1.1 Astro Bot (PS5, 2024)
- **Inquadratura:** dietro e sopra, pitch misurato **~15-26° in giù secondo il livello**, FOV orizzontale **~80-95° (grandangolo)**, personaggio piccolo nel terzo inferiore [R docs/notes/reviews/ref-astrobot-camera/, misura a punti di fuga].
- **Segue calma:** in verticale resta alla quota del terreno durante i salti e si riallinea all'atterraggio [A]; in orizzontale insegue con poco ritardo [A].
- **Rotazione:** stick destro libero quasi ovunque; da sola gira pochissimo [A]. Opzioni: inversione assi, **"Camera Assist"**: il tasto O ricentra, tenendolo premuto lo stick sinistro muove la camera (gioco con una mano) [F3].
- **Tratti autoriali:** molti tratti a vista laterale, dall'alto, binari per i boss e i panorami; passaggi morbidi, mai tagli durante il gioco [A]. Il livello è disegnato lungo l'asse della camera, così il giocatore tocca poco lo stick [A]. "Il percorso si legge da lontano perché è dipinto così" (Doucet) [F2]: la camera lavora insieme al colore.

### 1.2 Donkey Kong Bananza (Switch 2, 2025)
- **Vicina e costante:** in superficie la camera sta vicina dietro DK e mantiene la stessa distanza quasi sempre [F5][F6].
- **Sotto terra (talk CEDEC del team [F4]):** quando DK scava la camera entra nel terreno, lo schermo si scurisce, **rotazione libera a 360°**; il terreno fra camera e DK diventa **trasparente**; oltre una certa distanza nebbia, e **silhouette** indicano dove andare ("informazione vaga" come scelta di design). Le recensioni dicono che sotto terra la camera si allarga un po' e tiene DK al centro [F6], ma che negli spazi chiusi ci si perde [F5].
- **Tratti autoriali:** livelli bonus a vista laterale 2D, vista dall'alto o quasi isometrica nei puzzle e sulle piattaforme strette [F6].
- **Rischi noti:** segue e rifocalizza velocemente e non si può spegnere; alcuni giocatori hanno avuto nausea; le opzioni sono solo sensibilità orizzontale/verticale, giroscopio, scuotimento [F7]. Lezione: **ritmo lento delle correzioni automatiche e un'opzione per spegnerle**.

### 1.3 Super Mario Odyssey (2017) e la famiglia Mario 3D
- **Libera ma autoriale:** stick destro libero; in molte zone la camera è guidata (tratti 2D a 8 bit, viste fisse in stanze, binari) [A].
- **Verticale:** come Galaxy, la camera non insegue il salto finché Mario non esce da una fascia [A]; Galaxy lo fa con soglie `uplay/lplay` e ritardo `pushdelay` per area [F1].
- **Occlusione:** gli oggetti fra camera e Mario non spostano la camera: diventano **trasparenti a retino** (dithering a pattern, Bayer per gli oggetti che sfumano) [F8]. Sunshine mostrava la **silhouette** di Mario attraverso i muri [F9].
- **Il modello del designer (Galaxy, documentato [F1]):** un file di camere per livello; ogni camera = **tipo + parametri** (distanza, angoli, offset, FOV, `camint` = frame per arrivare, soglie verticali, `flag.collisionoff`); le **aree** (box) la attivano, con **priorità** se si sovrappongono; camere di evento (tubi, stelle, dialoghi) con priorità propria. Tipi: Follow, XZ_PARA (angoli e distanza fissi, solo traslazione), Tower (orbita attorno a un punto, `wpoint`), Rail, EyePos_Fix (ferma che guarda Mario), Point_Fix, 2D_Slide. **Il "punto di autofocus" di Galaxy è il `wpoint` di Tower/EyePos_Fix**: un punto del mondo che la camera tiene in quadro insieme a Mario.
- Mario 3D World: camera fissa senza zoom, ottima per i salti, ma non si guarda da vicino [F9].

### 1.4 Regole generali (Nesky, "50 Game Camera Mistakes", GDC 2014 [F10][F11]; Epic tech blog [F12])
1. Una camera = 7 valori: yaw, pitch, (roll), distanza, offset laterale/verticale, FOV. **Si orbita attorno al personaggio, mai si ruota sul posto.**
2. **Distanza e FOV derivati subito dal pitch** (curve): una sola distanza per tutti gli angoli è un errore; in vista dall'alto allontanare un po', dal basso avvicinare dolcemente.
3. Valori collegati che "inseguono" l'uno l'altro sembrano in ritardo: il ritardo va sulla **posizione del pivot**, non sulla rotazione che il giocatore comanda.
4. **L'intento del giocatore vince** sull'evitamento e sugli automatismi; dopo un input manuale gli automatismi aspettano (cooldown) [F12].
5. Ostacoli: sfera semplice, "baffi" (raycast a ventaglio) per vederli prima, colline ≠ muri (normale), oggetti marcati che si possono ignorare/sfumare; se proprio è bloccata, accorciare la distanza.
6. Rotazione automatica solo mentre il personaggio si muove e mai contro lo stick [F12]. The Last Guardian come esempio negativo di camera che "si incolla" a un oggetto [F9].
7. In verticale la camera si muove solo quando il personaggio esce da una fascia dello schermo [F9][F1].

## 2. Cosa c'è già nel progetto [R]
| Pezzo | Dove | Stato |
|---|---|---|
| Camera viva | GASP `CameraAsset_SandboxCharacter` (Gameplay Cameras) + `UGSTN_PlaytestCameraModifier` | In uso nel playtest |
| `UGSTN_CameraAnchorComponent` + `DA_CameraAnchor_Default` | `Source/.../Camera/` | Pivot dalla mesh (indipendente dalla capsula), molla verticale, air hold (AirRiseBand 250), look-ahead (spento), distanza +400/altezza +45, tilt-shift, offset crouch solo in ADS |
| Plugin `GSTNCamera` 0.1 | `Plugins/GSTNCamera/` (abilitato nel .uproject) | Pila di modalità (Follow, **Free alla Astro**, Shoulder, Section, Duel, Arena), regole a tag, **aree `AGSTN_CameraSection`** (Orbit/FixedPoint/Rail, priorità, limiti manuali, ricentro, blend, StickHold), fade/pull-in degli ostacoli, `OrbitState.DistanceScale`, 44 test headless. **Mai collegato a un personaggio**; spike C1 (ordine dei tick con NPP) e C4 (control rotation) aperti; impostazioni in **Project Settings** (contro la regola owner) |
| Spec | `docs/architecture/camera.md` | D1-D7, domande CAM.1-CAM.11 in parte aperte |

Il vecchio piano `camera-astrobot-piano.md` (C0-C3, 34-42 $) non teneva conto del plugin: questo documento lo sostituisce (decisione 3).

## 3. Design proposto
### 3.1 Architettura
```
Personaggio ─► UGSTN_CameraAnchorComponent  (pivot pulito: niente capsula, niente crouch fuori ADS, niente scatti a 62,5 Hz)
                     │ implementa IGSTN_CameraSubject (GetPresentedPivot, GetUp)
                     ▼
UGSTN_CameraComponent (plugin) ─► regole a tag ─► modalità: Platform (nuova = Free migliorata) | Section | Shoulder (ADS, invariata)
                     ▲                     ▲
   AGSTN_CameraSection (aree)    AGSTN_CameraFocusPoint (nuovo, punti di focus)
                     ▼
   Opzioni del giocatore (distanza, velocità, inversione, rotazione automatica) = UGSTN_CameraUserOptions (locale, salvato)
```
- **Un solo posto per ogni cosa:** l'anchor smette di fare air hold/look-ahead/distanza (passano al plugin) e resta "pulizia del pivot" + tilt-shift. Il crouch che fa saltare la camera (bug noto) sparisce se il pivot viene dall'anchor anche per il plugin.
- **ADS separata:** regola `Camera.Request.Aim` → Shoulder a priorità 100, come oggi. L'offset crouch in ADS resta (bCrouchOffsetInAim).
- **Locale per giocatore, mai nella sim, mai replicata** (già così nel plugin). Le aree e i punti di focus si valutano sul pivot presentato del giocatore locale.
- **Tutto a dati, esposto a Blueprint, niente Project Settings:** `UGSTN_CameraProfile` (Data Asset) per i valori di default, per livello sovrascrivibile da un `AGSTN_CameraLevelSettings` o dalla World Settings; le aree e i punti sono attori piazzati nel livello.

### 3.2 Modalità Platform (Free migliorata)
| Comportamento | Regola | Da chi |
|---|---|---|
| Pivot orizzontale | Molla critica, mezza vita 0,10-0,20 s; ritardo sulla posizione, mai sulla rotazione | Nesky 3, Astro |
| Verticale | Resta alla quota dell'ultimo terreno; sale solo oltre `JumpDeadZoneUpCm` (250) o all'atterraggio più in alto; scende subito oltre `FallFollowCm` (50); riallineamento all'atterraggio 0,2 s | Galaxy/Odyssey/Astro (già nel plugin e nell'anchor: si unifica) |
| Pitch | Pitch del designer per livello/area (15-26°); il giocatore lo cambia entro limiti; **distanza e FOV derivati dal pitch con curve** | Astro misurato, Nesky 2 |
| Distanza | `Distanza finale = distanza del designer × scala del giocatore`, scala limitata dall'area (`DistanceScaleLimit`) | §3.3 |
| Look-ahead | Piccolo (≤ 100 cm), lento (0,6 s), ridotto in aria | spec §2.4 |
| Rotazione automatica | **Spenta di default** (Astro); opzione "segui dietro" morbida stile Odyssey: solo in corsa, dopo 1 s senza stick, max 30°/s | decisione 2 |
| Ricentro | Tasto: porta la camera dietro al personaggio in 0,25 s; opzione "Camera Assist" alla Astro (tenendo il tasto, lo stick sinistro muove la camera) | Astro [F3] |
| Pitch automatico | In caduta oltre il bordo inclina in giù fino a +12° per vedere dove si atterra | spec §2.4 |
| Riferimento dello stick | Dopo un cambio di inquadratura, la direzione dello stick resta quella vecchia finché lo stick non torna al centro (già previsto: StickHold) | Nesky, Mario 3D World |

### 3.3 Distanza regolabile dal personaggio
- **Il designer** decide la distanza per profilo/livello/area (in cm, con la curva dal pitch).
- **Il giocatore** ha una **scala**: preset **Vicina 0,75 / Media 1,0 / Lontana 1,3** ciclabili con un tasto (default proposto: croce direzionale su/giù; input da Input Action, mai cablato) **e** uno slider continuo 0,6-1,6 nelle opzioni. Owner: "quando due comportamenti sono plausibili, entrambi come opzione".
- **L'area può limitare** la scala (es. un corridoio laterale la blocca a 1,0) o forzarne una propria; uscendo, torna quella del giocatore.
- Cambio di distanza in 0,3 s (ease-in-out), FOV invariato (il "toy look" viene dal grandangolo e dalla distanza, non dallo zoom).
- Esiste già: `FGSTN_CameraOrbitState.DistanceScale` e `FGSTN_CameraOrbitLimits.DistanceScaleLimit`. Si aggiunge: input, preset, salvataggio nelle opzioni.

### 3.4 Punti di focus del designer (`AGSTN_CameraFocusPoint`)
Attore leggero (anche componente da mettere su un nemico o un oggetto) con:
| Gruppo | Campi |
|---|---|
| Dove agisce | Forma: sfera o box (raggio interno = forza piena, esterno = zero, curva di caduta) oppure "legato a un'area" |
| Cosa fa | `Mode`: **Frame** (tiene in quadro personaggio + punto: sposta il pivot verso il punto e, se serve, allarga la distanza fino a `MaxExtraDistanceCm`) · **Look** (piega yaw/pitch verso il punto, max `MaxYawBiasDeg`/`MaxPitchBiasDeg`) · **Reveal** (una volta sola: panoramica verso il punto per `HoldSeconds`, poi torna; per scoperte e porte che si aprono) |
| Quanto | `Weight` 0-1, `Priority`; più punti attivi: si sommano pesati, con un tetto; a parità vince il più vicino |
| Giocatore | `Strength`: **Soft** (si ferma appena il giocatore tocca lo stick, riprende dopo `ResumeDelay` 1,5 s) · **Firm** (lo stick sposta entro limiti, poi torna) · **Lock** (solo per Reveal brevi) |
| Quando | Sempre / solo a terra / solo in movimento / quando un tag gameplay è attivo / una volta per vita; attivabile da Blueprint (`SetFocusEnabled`) |
| Ritmo | `BlendInSeconds` 0,6, `BlendOutSeconds` 0,8, velocità angolare massima (anti-nausea, lezione Bananza) |
| Debug | Gizmo in editor (raggi, punto), `gstn.Camera.Debug` mostra i pesi attivi |
È il `wpoint` di Galaxy generalizzato (Tower/EyePos_Fix) e il "Target Group" di Cinemachine [A], con le regole di Nesky: mai contro lo stick, mai scatti, mai incollata (The Last Guardian).

### 3.5 Ostacoli e spazi chiusi
- Default: **retino** (dithering a pattern, materiale con funzione comune) sugli oggetti fra camera e personaggio, come Odyssey; **silhouette** del personaggio attraverso i muri (Sunshine, Bananza); pull-in solo per blocchi duri vicinissimi. Il plugin ha già la decisione fade/pull-in; manca il materiale.
- Spazi chiusi (stile Bananza sotto terra): area con flag `Enclosed` che allarga un po', scurisce, rende trasparente il terreno vicino e libera la rotazione a 360°. Utile solo se faremo tratti scavabili/sotterranei: fase P3, opzionale.
- Hardware "tostapane": una sweep sferica per frame + 2-4 raggi a ventaglio, solo per il giocatore locale.

### 3.6 Strumenti per il designer
Aree e punti piazzati nel livello con gizmo; anteprima della camera dal viewport ("Pilot this camera" su un'area); validatore che avvisa di aree sovrapposte con stessa priorità e di muri grandi senza tag di fade; profilo `DA_CameraProfile_Default` per tutti i kit (regola owner "tutti i kit uguali").

## 4. Valori di partenza (da tarare su video, [A] dove non misurati)
Pitch 18° (gamma 15-26 per livello); FOV orizzontale 85-90° [misurato 80-95]; distanza tale che il personaggio sia ~1/8 dell'altezza dello schermo (oggi +400 cm sul braccio GASP: da rimisurare); pivot mezza vita 0,15 s; fascia salto 250 cm; ricentro 0,25 s; blend fra aree 0,5-0,7 s ease-in-out orbitando attorno al personaggio; focus point blend 0,6/0,8 s, bias massimo 25° yaw / 10° pitch.

## 5. Piano a fasi
| Fase | Cosa | Macchina, modello | Stima |
|---|---|---|---|
| P0 | Spike sul fisso: `UGSTN_CameraComponent` su un figlio di prova dell'Overlord (nessun BP condiviso toccato), soggetto = anchor; ordine dei tick con NPP (C1), control rotation (C4); stato del plugin Gameplay Cameras in 5.9 (chiedere all'assistente AI dell'editor); video base | fisso, Sonnet | 3-5 $ |
| P1 | Nucleo Platform: profilo a Data Asset (via dai Project Settings), adapter anchor→soggetto, air hold unico, curve pitch→distanza/FOV, distanza regolabile (preset + slider + limiti d'area), opzioni giocatore, ricentro/Camera Assist, rotazione automatica opzionale; test headless | cloud, Opus high | 12-16 $ |
| P2 | `AGSTN_CameraFocusPoint` (Frame/Look/Reveal, pesi, Soft/Firm/Lock, ritmo anti-nausea, debug); test | cloud, Opus high | 9-12 $ |
| P3 | Retino + silhouette (funzione materiale, tag), flag Enclosed (opzionale) | cloud Opus medium + fisso per materiali | 8-11 $ |
| P4 | Montaggio su L_Test_Hub: 3 aree (laterale, dall'alto, Orbit) + 3 punti di focus; taratura; video in camera di gioco; review Fable contro un dossier di riferimento (regola dei dossier) | fisso, Sonnet + review cloud Fable | 8-10 $ |
| P5 (dopo) | Editor: gizmo, anteprima, validatore | cloud, Opus medium | 6-8 $ |
Totale P0-P4 **~40-54 $** (+ P5 6-8 $). Rischi: spike C1/C4 (se l'ordine dei tick con NPP fa tremare la camera, il pivot resta sull'anchor che è già PostUpdateWork); costo del retino su hardware debole (dithering opaco, niente traslucenza).

## 6. Cosa resta come oggi
ADS (Shoulder + offset crouch), arene PvP (Duel/Arena del plugin o la camera GASP), tilt-shift (resta nell'anchor), opzioni di input esistenti.

## 7. Decisioni applicate (regola 6, da confermare, 13d)
Impostazioni del plugin da Project Settings a Data Asset; un solo air hold (nel plugin); rotazione automatica spenta di default se l'owner non decide; preset distanza 0,75/1,0/1,3 + slider; croce su/giù per la distanza come default (rimappabile); retino come occlusione di default (CAM.6).

## 8. Decisioni per l'owner (rispondere "1A 2A 3A")
1. **Su cosa costruire.** **A (consigliata)** il nostro plugin `GSTNCamera` (ha già aree, binari, regole, test; zero dipendenze sperimentali), con l'anchor come soggetto. B il rig Gameplay Cameras del GASP con nodi nostri (meno codice nostro, ma plugin Epic forse ancora Experimental in 5.9 e aree/focus da rifare a nodi). Costo: A come §5; B ~+10 $ e P0 più lungo. Cosa resta fermo: niente.
2. **Rotazione automatica di default.** **A (consigliata)** come Astro: la camera non gira da sola, ricentro col tasto; "segui dietro" disponibile come opzione del giocatore. B come Odyssey: segue dietro dolcemente in corsa (opzione per spegnerla). C entrambe nel menu e scelta per livello dal designer. Costo uguale.
3. **Piano e tempi.** **A (consigliata)** questo piano sostituisce C0-C3 del 17/11 e parte con P0 dopo il ciclo di locomozione in corso. B parte subito in parallelo (occupa un posto cloud e il fisso per P0). C prima solo P0+P1 (distanza regolabile), focus point dopo un playtest.

## 9. Peer review Gemini
Prompt: `agy-camera-research-prompt.md` (ricerca alla cieca) e `agy-camera-review-prompt.md` (review di questo documento). Esiti attesi in `agy-research-esito.md` e `agy-review-esito.md` nella stessa cartella; da integrare qui prima di portare le decisioni all'owner.

## Fonti
F1 lumasworkshop.com/wiki/Cameras (formato camere di Super Mario Galaxy) · F2 videogameschronicle.com, intervista Doucet su Astro Bot (2024) · F3 access-ability.uk/2024/09/05/astro-bot-accessibility-review · F4 invenglobal.com/articles/24102 (talk CEDEC del team di DK Bananza) · F5 xda-developers.com "The camera in Donkey Kong Bananza is bad…" · F6 gamerant.com "Donkey Kong Bananza camera perspective unsung hero" · F7 notebookcheck.net "Donkey Kong Bananza is causing motion sickness…" · F8 threadreaderapp.com/scrolly/924061000919285761 (dettagli tecnici Odyssey) · F9 gamedeveloper.com "Third person camera view in games…" · F10 John Nesky, "50 Game Camera Mistakes", GDC 2014 (gdcvault.com/play/1020460) · F11 matellion.blogspot.com, riassunto del talk di Nesky · F12 unrealengine.com/tech-blog/six-ingredients-for-a-dynamic-third-person-camera
