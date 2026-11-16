# Piano: partenza corsa scattante + clip skid/wall slide da GaspMoverProject (2026-11-16, solo piano)
Thread cloud, Opus medium, budget ~5 $. Fonti: codice di claude/kit-playtest-1116 9658b271 (GSTN_RunStageLogic.cpp, GSTN_RunStageSyncState.cpp, GSTN_AnalogWalkTypes.*), handoffs/anim-layer-skid-wallslide-piano.md, handoffs/corsa-mario-piano.md.
Gergo owner (ambito: corsa a stadi): stadio 1 = **run**, stadio 2 = **sprint 1**, stadio 3 = **sprint 2**. Niente "walk" come stadio.

## 0. Stato attuale (dal codice)
- Modalita' Mover a terra = `UWalkingMode` ("Walking" e' solo il nome della modalita' a terra, non il gait). Base = **run** 450 cm/s, accel 1800; sprint 1 650/1400; sprint 2 850 (GSTN_RunStageLogic.cpp:42-62).
- La camminata esiste solo come stick poco inclinato (Analog Walk): velocita' = curva dell'inclinazione; il gait walk del GASP viene scelto dall'ABP per la clip ma non limita (WalkSpeed = velocita' dello stadio, GSTN_RunStageSyncState.cpp:119).
- Partenza sim, conto su carta: boost x2.5 sotto il 50% della velocita' -> 0->225 in ~0.08 s, 225->450 in ~0.125 s, + smoothing 0.03 s: **~0.2 s (~13 frame)**. Probabile che il ritardo percepito sia soprattutto ANIMAZIONE (motion matching GASP sceglie start realistici e lenti, blend), non la sim. Va misurato prima di toccare numeri (regola 11).

## 1. Fasi
| # | Cosa | Macchina, modello | Stima |
|---|---|---|---|
| R0 | Misura: cattura frame per frame stick->velocita' sim->primo cambio di posa visibile (run, sprint 1 via L3), log CSV; confronto con riferimento Mario/Astro Bot (frame al picco). Decide quanto e' sim e quanto anim | fisso, Haiku/Sonnet + guardata cloud | ~2 $ |
| R1 | Accelerazione regolabile per stadio: `AccelerationCurve` (FRuntimeFloatCurve, frazione di velocita' 0..1 -> moltiplicatore) al posto del solo boost Analog Walk; opzione `StartSpeedFraction` (velocita' al frame 1, stile Mario) come IMPOSTAZIONE per kit (regola owner: due comportamenti = setting). Solo dati nella sim, rollback-safe, 62.5 Hz invariati. Test unitari sulla logica pura | cloud, Opus medium | ~4 $ |
| R2 | Clip run-start: presentazione, non sim. Override "Start" (stesso sistema a slot del piano anim-layer, priorita' sotto skid/wall) che parte quando lo stato run passa da fermo a in moto; root motion off, play rate dalla velocita' sim (distance matching), uscita con inertialization nella corsa GASP. FEEL RULE: frame 1 cambia chiaramente posa. Fonte clip: vedi R3 (GaspMoverProject se c'e'), altrimenti start GASP esistente tagliato; l'owner da' le pose chiave se nessuna va bene | cloud Opus medium + fisso per cattura/review (regola 10) | ~5 $ |
| R3 | = L0 del piano anim-layer, ESTESO: copia da GaspMoverProject (originali intatti) in Ported57/: Skid (Skid_Revised, Driving_Skid_Revised, SS_180_Turn x3), WallSlide (clip dell'owner + CR_WallIkControlRig) ed eventuale clip di partenza. Stesso scheletro? Se no retarget (IK Retargeter), mai modificare le pose | fisso, editor 5.8 libero, Sonnet medium | ~2.5 $ |
| R4 | Review ABP modificata dell'owner (ABP_WallSliding e simili): solo lettura, tabella "tieni / scarta / perche'" (stati, transizioni, IK, curve). Si porta solo cio' che e' buono nei layer (wall/skid), niente copia dell'ABP | cloud su export testuale (fisso esporta), Opus medium | ~1.5 $ |
Poi L1-L3 del piano anim-layer (slot override, wall layer con clip vere, skid layer) usano R3/R4.

Totale nuovo: ~15 $ (R0-R4). R3 sostituisce L0 (~2 $), quindi aggiunta netta ~13 $.
Ordine: R0 e R3 sul fisso dopo il playtest (stessa sessione editor), R1 e R4 in cloud in parallelo, poi R2.

## 2. Vincoli
- Nessun thread tocca il fisso finche' l'owner fa playtest. GaspMoverProject e' 5.7: lo si apre solo per esportare/copiare, mai salvarlo.
- Clip dell'owner: solo copie; curve dati (Distance) solo sulle copie.
- Valori di default di R1: quelli di oggi (nessun cambio di feel finche' l'owner non prova le curve). Preset "Mario" come asset di profilo separato.

## 3. Da confermare (al coordinatore, regola 13d)
- Quale clip di partenza vuole l'owner: c'e' in GaspMoverProject? Se no: start GASP tagliato vs pose chiave sue.
