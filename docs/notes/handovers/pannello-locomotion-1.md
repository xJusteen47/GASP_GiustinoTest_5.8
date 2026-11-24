# Pannello Locomotion (DA_Locomotion_*) riordinato

Branch `claude/project-thread-hhhtlc`, thread "Riordinare il pannello Locomotion" (owner 2026-11-24: duplicati, opzioni superflue, array poco leggibili).

## Causa dei duplicati
Le tre struct del profilo (`RunStages`, `AnalogWalk`, `JumpDesigner`) hanno `ShowOnlyInnerProperties`: il pannello mette ogni proprietà interna nella categoria
della SUA struct, mentre la struct restava anche sotto `Locomotion|...`. Da qui Run Stages, Analog Walk, Jump Designer e Walking Mode Properties due volte.

## Cosa cambia (solo metadati, nessuna proprietà rinominata: i valori degli asset e gli script `set_editor_property` restano validi)
- Categorie finali: Run, Skid, Walk, Air, Jump, Mode Bindings (`PrioritizeCategories` sulla classe). Le struct annidate (StartStamp, BoostDash, Skid,
  Reversal, Air, Momentum) sono anch'esse appiattite e le loro categorie interne puntano già al layout finale.
- Array di nomi delle variabili dei mode (Speed/Acceleration/Turning/Brake, AirSpeed/AirControl, WalkSpeed/WalkAcceleration/FacingTime/AccelerationSmoothing):
  tutti in `Mode Bindings`, `AdvancedDisplay`.
- `Stages` con `TitleProperty` (velocità e frame allo stadio dopo).
- `EditCondition`: Reversal su bEnabled, BoostWindowFrames su InitialWindow, MinAirSpeed su TakeoffCap, decelerazioni neutre per stile, campi Analog Walk su
  bEnabled, modelli vecchi dello skid su Motion != Physics.
- Rotazione in aria mostrata sotto Air ma ancora spenta da "Analog Walk Enabled" (detto all'owner; separarla = sua decisione).

## Decisioni owner (1A 2A)
- 1A tolte: `AnalogWalk.InnerDeadzone` (0), `AnalogWalk.StartAccelerationMultiplier` (1), `Skid.bResetsStage` (ignorata con ResumeStageAfterSkid). Nessuna
  era salvata in DA_Locomotion_Default (nomi assenti dalla tabella nomi dell'asset, `strings` sul file). Senza Resume lo skid torna sempre allo stadio 1.
- 2A: skid ClipRoot/BrakeExit e aria Fixed/MoverLegacy restano (cvar gstn.Skid.Motion) ma con DisplayName "(Legacy)" e i loro valori in `Skid|Legacy Models` avanzate.

## Da fare
Build e verifica sul fisso NON fatte: _ue59 occupato dal thread capsula-a-terra, poi l'owner stava giocando sul fisso. Al rilancio: dump dei valori di
DA_Locomotion_Default prima/dopo (export_text delle struct), `Run-Timed.ps1 -Preset Build`, test `GSTNCombat.Core.AnalogWalk` e `GSTNCombat.Core.RunSkid`,
screenshot del pannello.

ESITO: PARZIALE codice pronto su due commit, non compilato né provato sul fisso (desktop occupato)
SUBAGENTI: 0
COSTO: ~3 $ | 0 min desktop | 0
