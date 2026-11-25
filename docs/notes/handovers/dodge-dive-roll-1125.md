# Schivata dive + rotolata (cloud, Opus, 2026-11-25). Branch `claude/project-thread-g997re` da `main` 152d25e6
Owner 09:21Z: "l'animazione di dive + roll per rimettersi in piedi di Mario 64, con l'handling e fisica del long jump di Mario Odyssey"; 09:22Z: "sempre sulla capsula".

## Riuso (INDICE 5.2)
La schivata esistente era già il long jump (dodge-fallback-1117, feel-f1-dodge-lag): impulso 800 avanti + 300 su, velocità di corsa portata fino a 1400,
controllo in aria pieno, 8 f di lag con scivolata di pancia (4000 cm/s²), lock 10 f. Resta tutto. Si aggiungono la rotolata per rialzarsi e la capsula.

## Fatto
1. `FGSTN_DodgeLandingKitExtension::GetUpRollFrames` (default 10): frame aggiunti in fondo alla lag per la rotolata. Lag totale = 8 pancia + 10 rotolata = 18 f.
   Stesse regole della lag (nessuna azione, vulnerabile, la scivolata continua). 0 = comportamento di prima. `ResolveEndLag` somma le due parti.
   Timeline dei dodge con 10 f di margine in più (`FGSTN_MoveTimelines::DefaultDodgeGetUpRollFrames`).
2. `FGSTN_CombatStateView::DodgeEndLagFrame` / `DodgeEndLagFrames` (dal sync state, solo lettura per presentazione/BP).
3. Capsula greybox (`UGSTN_GreyboxAnimInstance::UpdateDive`, pura `DiveAngle`): in aria durante un dodge (ability con `bDodgeLanding`) si inclina di testa verso
   la direzione di volo a 75° in 6 f; all'atterraggio si sbatte di pancia (90° in 3 f) e scivola; negli ultimi `DiveGetUpRollFrames` (10) della lag rotola in avanti
   fino in piedi (giro intero). Lag tagliata (roll-out a comando, hit) = finisce il giro a 1080°/s o si ferma (hit). Niente squash d'atterraggio durante il tuffo.
   Impostazioni in `ShapeSettings` categoria Greybox|Dive.

## Test (NON compilati qui)
Nuovo `GSTNCombat.NP.Greybox.Dive`. Aggiornati `GSTNCombat.Core.DodgeLanding.EndLagBlocksActions` (lag 18, override 24/14/0/10) e
`GSTNCombat.Core.MoveTimelines` (air dodge bloccante fino a 120 + 8 + 10).

## Da fare sul fisso
Build, `-Tests GSTNCombat`, cattura capsula di un dodge da corsa e da fermo (pie_capture_move). Guardare: il tuffo legge da dive, la rotolata non sprofonda nel
pavimento (floor lift), il giro non salta.

## Scelte aperte (owner)
Rotolata automatica vs a comando, invulnerabilità, traiettoria/controllo: vedi il thread "Rifare la schivata come dive e roll".

ESITO: PARZIALE codice scritto, non compilato né provato (fisso offline)
SUBAGENTI: 1, lettura pagine web riferimenti SM64/SMO, Opus
COSTO: ~4 $ | 0 minuti desktop | 0 token review
