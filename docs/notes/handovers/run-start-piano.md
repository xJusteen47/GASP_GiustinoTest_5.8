# Handover: piano partenza corsa + import skid/wall slide (2026-11-16, cloud, Opus medium, solo piano)
Branch claude/project-thread-rafd93 da claude/kit-playtest-1116 9658b271. Nessun codice.
- Piano: docs/notes/run-start-skid-import-piano.md (copia in project files handoffs/run-start-skid-import-piano.md). Stima ~15 $ (R0-R4), netto ~13 $.
- Gergo owner (ambito corsa a stadi): stadio 1 = run, stadio 2 = sprint 1, stadio 3 = sprint 2.
- Risposta all'owner: base = run; modalita' Mover a terra = BP GASP figlia di USmoothWalkingMode (correzione: prima avevo scritto UWalkingMode, errato); cambiarla non consigliato prima della misura R0; walk = solo stick poco inclinato.
  Prova: GSTN_RunStageLogic.cpp:42-62 (velocita' stadi), GSTN_RunStageSyncState.cpp:119 (WalkSpeed = velocita' stadio).
- Partenza sim ~0.2 s calcolata su carta da Acceleration 1800, StartAccelerationMultiplier 2.5, LowSpeedFraction 0.5, AccelerationSmoothingTime 0.03 (GSTN_AnalogWalkTypes.h); NON misurata: R0 la misura.
- Owner: in GaspMoverProject ci sono anche la sua wall slide e un'ABP modificata ("probabilmente male") -> R3 copia, R4 review tieni/scarta.
- Da confermare: fonte della clip di partenza.
- Owner 21:03Z: il problema e' ZONA MORTA + accelerazione ISTANTANEA. Cause: IMC dead zone 0.12 + InnerDeadzone 0.05 in serie; MinSpeedFraction 0.12 + StartAccelerationMultiplier 2.5 (gradino). Piano rivisto: R1 = zona morta unica + rampa (AccelerationCurve, AccelRampFrames); tolta l'opzione velocita' al frame 1; R2 (clip) priorita' bassa.
- Owner 21:06Z: zona morta base = OPZIONE PER GIOCATORE (R1a, LocalPlayer, IO-1); soglie attacchi neutri/direzionali = valore diverso, gia' in UGSTN_InputSettings (StickDeadZone 0.2, AimZone 0.5) ma in Project Settings: da spostare (fuori piano). Totale ~17 $.
- Owner 21:07Z: anche le soglie neutro/direzionale sono opzione del giocatore -> R1c (via FGSTN_LoadoutData perche' classificate nella sim). Totale ~20 $.
- Owner 21:08Z: reverse/rotazioni restano a parte; controllato: non leggono le soglie attacchi; pavimenti hardcoded 0.25 (StartupSteer, MomentumSlide) -> dati in R1c. Totale ~20.5 $.
