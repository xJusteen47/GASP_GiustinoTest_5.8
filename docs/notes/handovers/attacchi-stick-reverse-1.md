# Attacchi verso lo stick + finestra di reverse (thread 3j0pgk, 2026-11-16)
Branch `claude/project-thread-3j0pgk` (base `claude/integ-playtest-1116` b528f9a8). Cloud, Opus medium. NON COMPILATO (cloud senza UE).

## Cosa fa
- Owner 16-11: "stick a destra mentre guardi a sinistra → la mossa va a destra", con i trucchi del reverse.
- `AimPolicy` (prima solo dato, mai letto) ora gira. Nuovo valore e default `Auto`: ogni mossa che legge lo stick (attacchi, roll, schivate: owner 16-11 19:53) = `SnapIfAimOrTilt` (stick di mira col reticolo, se no stick di movimento); get-up (`bIgnoreMoveInput`) e mosse Forced (tech, AI, script) = `KeepFacing`. Snap istantaneo sui frame 0-1, limitato da `MaxTurnDegrees` (180).
- Schivate/dash (`UGSTN_MoveImpulseCreator`): l'impulso parte dallo yaw della move origin (già girato), non dal facing del frame prima.
- Niente back tilt (owner): gli slot standard hanno già "Directional = qualsiasi direzione inclinata", quindi con lo snap la tilt esce verso lo stick. Hitbox "dietro le spalle" = solo dato della mossa (hitbox piazzata dietro). Il reverse resta solo per mosse con hitbox.
- `FGSTN_ReverseWindow Reverse` per mossa: frame 1..`Frames` (3), tagliata prima del primo frame attivo; stick di movimento ≥ `StickThreshold` (0.7) e ≥ `MinAngleDegrees` (120°) dalla direzione della mossa → la mossa si gira verso lo stick. `Momentum`: `Keep` (default) o `TurnWithFacing` (B-reverse: la velocità ruota attorno all'up, 180 = inversione).
- Snap e reverse riscrivono lo yaw di `Hit.MoveOrigin` (sync state): cono di steer, hitbox WorldAtMoveStart e frecce partono dalla nuova direzione. Richiesta `SetFacing` o nuova `SetFacingTurnMomentum` (=23); tag `Combat.SetByCaller.FacingTurnVelocity`.
- Terra e aria: stessa logica (la sim non distingue). Il giro libero in aria fuori dagli attacchi è del thread sul fisso.

## Scelte applicate (regola 6)
1. Stick letto all'ESECUZIONE (frame in cui la mossa esce), non alla pressione bufferata: vince l'ultima intenzione; il reverse copre il flick tardivo.
2. Frame 0 = turnaround (snap, momento invariato); frame ≥1 = reverse (momento secondo dato). Come Smash.
3. Reverse senza flag "una volta sola": si riattiva solo se lo stick torna ≥120° dalla nuova direzione, dentro 3 frame. Nessun campo di sync nuovo.
4. Back-air / mosse col senso relativo al facing: autore mette `AimPolicy = KeepFacing` e `Reverse.Frames = 0`. KIT_Test_Hits (uasset) non verificato dal cloud: se ha binding "indietro" vanno tolti in editor.
6. Rotazione lenta col GASP ABP (owner): lo snap gira la capsula in 1 frame; il ritardo visivo è ABP/rotation rate del profilo di movimento, del thread sul fisso (non toccato qui).
5. Momento del reverse `Keep` di default (il thread Mario run tocca `MomentumSlide`, non toccato). Lo slide legge l'orientamento al suo primo step: dopo lo snap = "contro" se si corre e si attacca all'indietro. Da verificare in playtest.

## Build e test (sul fisso, dopo il thread con l'editor)
1. `git fetch origin claude/project-thread-3j0pgk` e merge nel branch di playtest.
2. Build editor (script a tempo, 45 min).
3. Test: `GSTNCombat.Core.AttackAim` (3 nuovi), `GSTNCombat.Core.StartupSteer`, `GSTNCombat.NP.Weakpoint`, `GSTNCombat.Editor.Validate.AimTurn`, `GSTNKitMods`.
4. PIE L_Test_Hub: guarda a sinistra, stick a destra + X → colpisce a destra; X poi flick indietro entro ~3 frame → si gira.

## File
GSTN_TimelineTypes.h, GSTN_StartupSteer.h/.cpp, GSTN_CombatSimComponent.h/.cpp (StepOriginAim), GSTN_SetFacingEffect.h/.cpp, GSTN_CombatActionTypes.h, GSTN_CombatNPTags, validatore, KitModBaking, test Core AttackAim + NP StartupSteer, README NP.
Costo stimato: ~6 $.
