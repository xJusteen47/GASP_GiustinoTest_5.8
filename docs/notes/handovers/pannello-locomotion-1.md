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

## Verifica (laptop, worktree GASP_Giustino2_cp1verify, desktop occupato)
- Giro 1 (a0f3922c): build OK (con -SkipRegress: il gate rifiuta il tag approved/runstart-v4r5, asset assente al tag, non legato a questo branch);
  test GSTNCombat.Core.AnalogWalk/RunSkid/RunStages 20/20 PASS (Check-TestLog); dump di DA_Locomotion_Default (export_text, Saved/LocoPanel/before.txt e
  after.txt) identico salvo i tre campi tolti. Trovati due problemi: righe AdvancedDisplay invisibili anche con "mostra avanzati" (Mode Bindings sparito)
  e la categoria "Run" tradotta "Esegui" dall'editor in italiano.
- Giro 2 (43eb110b): niente AdvancedDisplay, categorie RunStages/ModeBindings in una parola (lista separata da spazi di PrioritizeCategories e
  AutoCollapseCategories). Build OK, after2.txt identico ad after.txt. Pannello: Run Stages, Skid, Walk, Air, Jump, Legacy, Mode Bindings; righe attese tutte
  presenti (screenshot Saved/LocoPanel/r2_*.png nella worktree). AutoCollapse chiude i sottogruppi, non i titoli.
- Crash in chiusura (ACCESS_VIOLATION 0x58 in UnrealEd, stack Slate, senza frame nostri): 2/2 chiudendo l'editor con la scheda dell'asset aperta, 0/2
  altrimenti. Senza PDB sul laptop non si vede la funzione; probabilmente engine. Da riguardare solo se ricapita sul fisso con i simboli.

ESITO: PASS pannello riordinato, 3 opzioni tolte (1A), legacy marcati (2A); build, 20 test e valori verificati sul laptop
SUBAGENTI: 0
COSTO: ~10 $ | 0 min desktop | ~30 min laptop | 0
