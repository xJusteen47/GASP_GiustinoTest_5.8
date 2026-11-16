# Handover: piano partenza corsa + import skid/wall slide (2026-11-16, cloud, Opus medium, solo piano)
Branch claude/project-thread-rafd93 da claude/kit-playtest-1116 9658b271. Nessun codice.
- Piano: docs/notes/run-start-skid-import-piano.md (copia in project files handoffs/run-start-skid-import-piano.md). Stima ~15 $ (R0-R4), netto ~13 $.
- Gergo owner (ambito corsa a stadi): stadio 1 = run, stadio 2 = sprint 1, stadio 3 = sprint 2.
- Risposta all'owner: base = run (UWalkingMode e' solo il nome della modalita' Mover a terra); walk = solo stick poco inclinato.
  Prova: GSTN_RunStageLogic.cpp:42-62 (velocita' stadi), GSTN_RunStageSyncState.cpp:119 (WalkSpeed = velocita' stadio).
- Partenza sim ~0.2 s calcolata su carta da Acceleration 1800, StartAccelerationMultiplier 2.5, LowSpeedFraction 0.5, AccelerationSmoothingTime 0.03 (GSTN_AnalogWalkTypes.h); NON misurata: R0 la misura.
- Owner: in GaspMoverProject ci sono anche la sua wall slide e un'ABP modificata ("probabilmente male") -> R3 copia, R4 review tieni/scarta.
- Da confermare: fonte della clip di partenza.
