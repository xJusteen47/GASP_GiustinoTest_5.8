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

## Scelte owner 09:46Z: "1A come opzione, ma B è si può continuare a scivolare di pancia finché l'attrito non lo ferma; 2A regolabile; 3B tarabile"
4. 1B default: `BellyMode` UntilStopped (pancia finché la scivolata si ferma: frame calcolati una volta all'atterraggio da velocità e
   `LandingSlide.Along.Deceleration`, min 8, max `MaxBellyFrames` 60, poi la rotolata 10), `bJumpRollsOut` (salto = rollout da frame 1; flag e non tag
   di default perché i tag nativi non sono pronti quando si costruiscono i default delle struct). 1A = `BellyMode` Fixed.
   Velocità d'atterraggio dal tracker (sim state, piano perpendicolare all'up). Lag 1400 cm/s = 22 + 10 f; 1800 = 29 + 10 f.
5. 2A: invariato. Regolabile nella traccia Hurtbox della timeline del dodge (EndFrame, Kind, tag) in un figlio BP di GA_GroundDodge/AirDodge.
6. 3B: `GroundDodgeMove` 1000 avanti / 250 su (prima 800/300), corsa portata x1.3 (prima x1), tetto 1800 (prima 1400). Nominale: 0,51 s, 32 cm di
   altezza, 510 cm da fermo, ~560 da corsa 850, ~730 da 1100. Tarabile sul GE del dodge (`UGSTN_MoveImpulseCreator::Move`).

## Input (owner 10:47Z/10:48Z: "avanti + salto da abbassato per la schivata"; "L per abbassarsi, ZL per lo scudo; nelle opzioni solo il metodo nuovo, solo quello di prima o entrambi")
7. `UGSTN_CombatSettings::DodgeInput` (Button / CrouchJump / Both, default Both), progetto, Combat|Dodge. Applicato alla compilazione della tabella
   (`ApplyDodgeInputMethod` in GSTN_CompiledBindingTable.cpp): ogni riga `Command.Defence.Dodge` ha una gemella `Command.Defence.Dodge.CrouchJump`
   (stessa ability, contesto, timeline) con lo slot `UGSTN_MovementSlots::MakeDodgeCrouchJumpSlot` = Press(Jump) + Hold(Crouch, anche dallo stesso
   frame) + stick inclinato in qualsiasi direzione; CrouchJump toglie le righe del tasto. Tag figlio: lock, tumble, cancel lo trovano per gerarchia.
   Funzione di layout nuova `Movement.Jump` (DefaultGSTNInput.ini). Crouch + salto con stick a riposo resta un salto (spazio per un backflip).
   In aria (air crouch + stick + salto) vale l'air dodge, non il doppio salto. Restano sul tasto schivata: rotolata da a terra (dodge+stick),
   posa della presa, zaino: con "solo CrouchJump" quei tre vanno rimappati (decisione aperta).
8. Pad: LB/L = abbassarsi, LT/ZL = scudo (prima il contrario). Solo negli script `Tools/GAS/setup_player_input_contexts.py` e
   `Tools/GASNPPTest/movement_setup.py`: vanno rilanciati sul fisso (IMC binari).
9. L'opzione per giocatore (menu) dovrà viaggiare col loadout: la tabella si compila uguale su ogni macchina, quindi oggi è un'impostazione di progetto.

## Test (NON compilati qui)
Nuovo `GSTNCombat.Core.Table.DodgeInputMethod`; `Table.ButtonLayout` fissa il metodo Button.
Nuovo `GSTNCombat.Core.DodgeLanding.BellyUntilStopped`; RollOut e Defaults aggiornati al salto-rollout di default; `Dodge.LongJumpCarriesRunSpeed` x1.3.
Nuovo `GSTNCombat.NP.Greybox.Dive`. Aggiornati `GSTNCombat.Core.DodgeLanding.EndLagBlocksActions` (lag 18, override 24/14/0/10) e
`GSTNCombat.Core.MoveTimelines` (air dodge bloccante fino a 120 + 8 + 10).

## Da fare sul fisso
Build, `-Tests GSTNCombat`, cattura capsula di un dodge da corsa e da fermo (pie_capture_move). Guardare: il tuffo legge da dive, la rotolata non sprofonda nel
pavimento (floor lift), il giro non salta.


ESITO: PARZIALE codice scritto, non compilato né provato (fisso offline)
SUBAGENTI: 1, lettura pagine web riferimenti SM64/SMO, Opus
COSTO: ~9 $ | 0 minuti desktop | 0 token review
