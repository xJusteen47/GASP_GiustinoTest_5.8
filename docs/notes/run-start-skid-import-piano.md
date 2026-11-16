# Piano: partenza corsa scattante + clip skid/wall slide da GaspMoverProject (2026-11-16, solo piano)
Thread cloud, Opus medium, budget ~5 $. Fonti: codice di claude/kit-playtest-1116 9658b271 (GSTN_RunStageLogic.cpp, GSTN_RunStageSyncState.cpp, GSTN_AnalogWalkTypes.*), handoffs/anim-layer-skid-wallslide-piano.md, handoffs/corsa-mario-piano.md.
Gergo owner (ambito: corsa a stadi): stadio 1 = **run**, stadio 2 = **sprint 1**, stadio 3 = **sprint 2**. Niente "walk" come stadio.

## 0. Stato attuale (dal codice)
- Modalita' Mover a terra = la modalita' Blueprint del GASP, figlia di `USmoothWalkingMode` (smoothing a molla; handovers/corsa-mario-p1.md:8, m3-doc-deltas.md:142). AccelerationSmoothingTime e FacingSmoothingTime gia' portati a 0.03 s dall'Analog Walk. Cambiare modalita' (Walking classico o C++) = reparent di un BP binario condiviso e perdita della traiettoria liscia che il motion matching usa: NON consigliato prima di R0. Base = **run** 450 cm/s, accel 1800; sprint 1 650/1400; sprint 2 850 (GSTN_RunStageLogic.cpp:42-62).
- La camminata esiste solo come stick poco inclinato (Analog Walk): velocita' = curva dell'inclinazione; il gait walk del GASP viene scelto dall'ABP per la clip ma non limita (WalkSpeed = velocita' dello stadio, GSTN_RunStageSyncState.cpp:119).
- Partenza sim, conto su carta: boost x2.5 sotto il 50% della velocita' -> 0->225 in ~0.08 s, 225->450 in ~0.125 s, + smoothing 0.03 s: **~0.2 s (~13 frame)**. Probabile che il ritardo percepito sia soprattutto ANIMAZIONE (motion matching GASP sceglie start realistici e lenti, blend), non la sim. Va misurato prima di toccare numeri (regola 11).

## 0b. Problema percepito dall'owner (21:03Z): ZONA MORTA dello stick + accelerazione ISTANTANEA (non partenza lenta)
Cause trovate nel codice (tutte nostre, messe dal thread kit-playtest-1116):
- Zona morta a DUE strati in serie: modificatore Dead Zone di IMC_GSTN_Locomotion 0.12 (Tools/GAS/setup_player_input_contexts.py:135, rimappa 0.12..1 -> 0..1) + InnerDeadzone 0.05 dell'Analog Walk sul valore gia' rimappato -> sotto ~16% di inclinazione reale non succede nulla. Da verificare: tipo Axial/Radial del modificatore, e se il BP GASP ha una soglia sua sull'intento.
- Salto di velocita': MinSpeedFraction 0.12 (appena fuori dalla zona morta il bersaglio e' gia' 12% della velocita' di run) + StartAccelerationMultiplier 2.5 (4500 cm/s^2 da fermo) + AccelerationSmoothingTime 0.03 -> il bersaglio si raggiunge in ~1 frame. Il conto "0->450 in 0.2 s" sopra e' a stick pieno; il gradino si sente a stick parziale.
- RunStages.StickDeadzone 0.25 conta solo per gli stadi (stick "rilasciato"), non muove il personaggio.
Prova subito, senza codice: DA_Locomotion_Default (/Game/GASNPPTest/Combat) -> Analog Walk: MinSpeedFraction 0, StartAccelerationMultiplier 1, InnerDeadzone 0; poi dead zone dell'IMC.

## 1. Fasi
| # | Cosa | Macchina, modello | Stima |
|---|---|---|---|
| R0 | Misura: log CSV per frame di stick grezzo, stick dopo l'IMC, intento, velocita' sim, a inclinazioni 0.1/0.2/0.4/1.0 (+ primo cambio di posa). Prima/dopo i valori di "prova subito" | fisso, Haiku/Sonnet + guardata cloud | ~2 $ |
| R1a | ZONA MORTA DEL GIOCATORE (owner 21:06Z, ambito: stick di movimento, opzione per giocatore): valore per LocalPlayer, salvato, nel `UGSTN_InputOptionsSubsystem` previsto da input-opzioni-per-giocatore.md (IO-1). Applicata in UN punto (sorgente input, prima di impacchettare lo stick nel comando), radiale con rimappa 0..1; il modificatore Dead Zone dell'IMC portato a 0 dallo script. Lato locale, mai nella sim: costo NPP zero, ogni giocatore in split-screen ha la sua. InnerDeadzone dell'Analog Walk -> 0 (non piu' una seconda zona morta) | cloud, Opus medium | ~3 $ |
| R1b | Rampa di accelerazione alla Mario/Astro Bot (dati del kit, profilo di locomozione): `AccelerationCurve` (FRuntimeFloatCurve, frazione di velocita' -> moltiplicatore) + `AccelRampFrames` (la spinta cresce da 0 al pieno in N frame dalla partenza, niente gradino). Default nuovi: MinSpeedFraction 0, StartAccelerationMultiplier 1. Preset "Mario" come profilo separato. Solo dati nella sim, rollback-safe, 62.5 Hz invariati, test sulla logica pura. Tolta l'opzione "velocita' piena al frame 1" | cloud, Opus medium | ~3 $ |
| R2 | Clip run-start: presentazione, non sim. Override "Start" (stesso sistema a slot del piano anim-layer, priorita' sotto skid/wall) che parte quando lo stato run passa da fermo a in moto; root motion off, play rate dalla velocita' sim (distance matching), uscita con inertialization nella corsa GASP. FEEL RULE: frame 1 cambia chiaramente posa. Fonte clip: vedi R3 (GaspMoverProject se c'e'), altrimenti start GASP esistente tagliato; l'owner da' le pose chiave se nessuna va bene | cloud Opus medium + fisso per cattura/review (regola 10) | ~5 $ |
| R3 | = L0 del piano anim-layer, ESTESO: copia da GaspMoverProject (originali intatti) in Ported57/: Skid (Skid_Revised, Driving_Skid_Revised, SS_180_Turn x3), WallSlide (clip dell'owner + CR_WallIkControlRig) ed eventuale clip di partenza. Stesso scheletro? Se no retarget (IK Retargeter), mai modificare le pose | fisso, editor 5.8 libero, Sonnet medium | ~2.5 $ |
| R4 | Review ABP modificata dell'owner (ABP_WallSliding e simili): solo lettura, tabella "tieni / scarta / perche'" (stati, transizioni, IK, curve). Si porta solo cio' che e' buono nei layer (wall/skid), niente copia dell'ABP | cloud su export testuale (fisso esporta), Opus medium | ~1.5 $ |
Poi L1-L3 del piano anim-layer (slot override, wall layer con clip vere, skid layer) usano R3/R4.

Totale nuovo: ~21 $ (R0-R4 + R1c). R3 sostituisce L0 (~2 $), quindi aggiunta netta ~18 $.
Ordine: R0 e R3 sul fisso dopo il playtest (stessa sessione editor), R1 e R4 in cloud in parallelo, poi R2.

## 1b. Soglie neutro/direzionale degli attacchi (owner 21:06Z: valore DIVERSO dalla zona morta; 21:07Z: anche questo OPZIONE DEL GIOCATORE)
Esistono gia': `UGSTN_InputSettings` (Project Settings, GSTN Input) StickDeadZone 0.2 (sotto = Neutral), AimZone 0.5 (0.2-0.5 = Aim: attacco neutro orientato; sopra = Tilt: direzionale), FlickFrames 3; `FGSTN_DirectionClassifier::ClassifyZone`.
Vincolo tecnico: la classificazione gira DENTRO la simulazione di input (GSTN_InputSimulator.cpp:64), su server e client, e la zona entra nello stato sincronizzato. Quindi un valore per giocatore deve essere identico su tutte le macchine: non basta un'opzione locale come la zona morta (R1a).
- **R1c**: soglie Neutral/Aim per giocatore. Valore scelto nelle opzioni del giocatore (stesso sottosistema di R1a), copiato in `FGSTN_LoadoutData` (gia' per LocalPlayer, inviato al server e compilato su ogni macchina, IO-3 del doc opzioni) e letto dal classificatore al posto dei Project Settings. Default = dati delle regole di gioco (asset regole), non Project Settings. Cambiarlo a meta' partita = nuovo loadout (come il cambio kit). FlickFrames (finestra del flick) e' anche lei opzione del giocatore (owner 21:10Z), stesso percorso via loadout. Il classificatore legge lo stick dopo la zona morta del giocatore, rimappato 0..1. I campi in UGSTN_InputSettings vengono tolti. Test: due giocatori con soglie diverse, stessa classificazione su server e client. cloud, Opus medium, ~3 $.

## 1c. Reverse, rotazioni, svolte: restano A PARTE (owner 21:08Z, ambito: tutte le soglie stick non di zona morta/attacco)
Controllo nel codice: nessuno legge le soglie degli attacchi (StickDeadZone/AimZone di UGSTN_InputSettings).
- Reverse: dati per mossa `FGSTN_ReverseWindow` (Frames 3, StickThreshold 0.7, MinAngleDegrees 120). Snap a inizio mossa (frame 0-1, AimPolicy): soglia propria. Ma entrambi hanno un pavimento HARDCODED `FGSTN_StartupSteer::DeadZone = 0.25` (GSTN_StartupSteer.h:80).
- Rotazioni: profilo di locomozione (StartFacingTime/RunFacingTime, AirTurnRateDegPerSec), stadi (TurningStrength, TightTurnCos), skid; Mover FacingDeadzoneThreshold 0.1. Tutti separati.
- Altre soglie hardcoded da rendere dati: MomentumSlide `StickDeadzone = 0.25` (GSTN_MomentumSlide.cpp:50). Gia' dati: RunStages.StickDeadzone 0.25, Projectile StickAimDeadzone 0.3, Interact StickThreshold 0.3, menu loadout 0.5.
- Unico legame comune: oggi tutti leggono lo stick DOPO la zona morta dell'IMC (0.12, rimappa). Con R1a leggeranno lo stick dopo la zona morta del giocatore, rimappato 0..1: e' voluto (la zona morta hardware viene prima di tutto) e i loro numeri non cambiano significato.
- In R1c (+~0.5 $): le due costanti 0.25 diventano campi dati (regole di combattimento per lo snap/reverse, dato della mossa per il momentum slide), con default identici. Nessun valore di reverse/rotazione diventa opzione del giocatore.

## 1d. Principio owner (21:10Z, ambito: sensazione dell'input in PvP) e proposte (NON in questo piano)
"In PvP le impostazioni di sensazione dell'input devono essere regolabili dal giocatore" (es. Street Fighter: negative edge disattivabile). Regola tecnica: cio' che legge la sim passa dal loadout (IO-3), cio' che e' locale resta nel sottosistema opzioni (IO-1).
Proposte da portare all'owner: tap jump on/off (salto con lo stick su); stick destro = attacchi forti / tilt / niente; scudo tenuto vs toggle; corsa sprint 2 con L3 vs automatica; sensibilita' del reverse come moltiplicatore per giocatore sopra il dato della mossa; vibrazione. Stima ~1-2 $ ciascuna dopo R1a/R1c (il percorso c'e').

## 2. Vincoli
- Nessun thread tocca il fisso finche' l'owner fa playtest. GaspMoverProject e' 5.7: lo si apre solo per esportare/copiare, mai salvarlo.
- Clip dell'owner: solo copie; curve dati (Distance) solo sulle copie.
- Default di R1 cambiano (niente gradino, zona morta piccola): e' la richiesta dell'owner.

## 3. Da confermare (al coordinatore, regola 13d)
- Clip di partenza: nessuna in GaspMoverProject (lettura del coordinatore di "no, si") -> start GASP; confermare. R2 ha priorita' bassa: il problema e' zona morta + gradino, non il ritardo.
