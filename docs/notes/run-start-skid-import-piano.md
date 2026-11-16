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
| R1 | Zona morta UNICA e regolabile: una sola fonte (profilo di locomozione del kit, radiale), il modificatore dell'IMC portato a ~0 dallo script; curva inclinazione->velocita' gia' c'e' (ResponseExponent). Rampa di accelerazione alla Mario/Astro Bot per stadio: `AccelerationCurve` (FRuntimeFloatCurve, frazione di velocita' -> moltiplicatore) + `AccelRampFrames` (l'accelerazione cresce da 0 al valore pieno in N frame dalla partenza, niente gradino). Default nuovi: MinSpeedFraction 0, StartAccelerationMultiplier 1. Preset "Mario" come profilo separato. Solo dati nella sim, rollback-safe, 62.5 Hz invariati, test sulla logica pura. Tolta l'opzione "velocita' piena al frame 1" | cloud, Opus medium | ~4 $ |
| R2 | Clip run-start: presentazione, non sim. Override "Start" (stesso sistema a slot del piano anim-layer, priorita' sotto skid/wall) che parte quando lo stato run passa da fermo a in moto; root motion off, play rate dalla velocita' sim (distance matching), uscita con inertialization nella corsa GASP. FEEL RULE: frame 1 cambia chiaramente posa. Fonte clip: vedi R3 (GaspMoverProject se c'e'), altrimenti start GASP esistente tagliato; l'owner da' le pose chiave se nessuna va bene | cloud Opus medium + fisso per cattura/review (regola 10) | ~5 $ |
| R3 | = L0 del piano anim-layer, ESTESO: copia da GaspMoverProject (originali intatti) in Ported57/: Skid (Skid_Revised, Driving_Skid_Revised, SS_180_Turn x3), WallSlide (clip dell'owner + CR_WallIkControlRig) ed eventuale clip di partenza. Stesso scheletro? Se no retarget (IK Retargeter), mai modificare le pose | fisso, editor 5.8 libero, Sonnet medium | ~2.5 $ |
| R4 | Review ABP modificata dell'owner (ABP_WallSliding e simili): solo lettura, tabella "tieni / scarta / perche'" (stati, transizioni, IK, curve). Si porta solo cio' che e' buono nei layer (wall/skid), niente copia dell'ABP | cloud su export testuale (fisso esporta), Opus medium | ~1.5 $ |
Poi L1-L3 del piano anim-layer (slot override, wall layer con clip vere, skid layer) usano R3/R4.

Totale nuovo: ~15 $ (R0-R4). R3 sostituisce L0 (~2 $), quindi aggiunta netta ~13 $.
Ordine: R0 e R3 sul fisso dopo il playtest (stessa sessione editor), R1 e R4 in cloud in parallelo, poi R2.

## 2. Vincoli
- Nessun thread tocca il fisso finche' l'owner fa playtest. GaspMoverProject e' 5.7: lo si apre solo per esportare/copiare, mai salvarlo.
- Clip dell'owner: solo copie; curve dati (Distance) solo sulle copie.
- Default di R1 cambiano (niente gradino, zona morta piccola): e' la richiesta dell'owner.

## 3. Da confermare (al coordinatore, regola 13d)
- Clip di partenza: nessuna in GaspMoverProject (lettura del coordinatore di "no, si") -> start GASP; confermare. R2 ha priorita' bassa: il problema e' zona morta + gradino, non il ritardo.
