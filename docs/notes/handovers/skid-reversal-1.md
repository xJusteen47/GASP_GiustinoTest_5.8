# skid-reversal-1 (portatile, 2026-11-20, Opus high) - inversione stick -> corsa da 0,576 s a ~0,3 s, regolabile nel DA
Branch claude/project-thread-k5vbaf (da claude/project-thread-ucs9jl 657bd3a1), checkout GASP_Giustino2_n216qs. gstn.Skid.Sim 0, gstn.Loco.Base 1.

## Fatto
1. Misura: nuovo scenario `move_reversal` in Tools/GASNPPTest/movement_scenarios.py (corsa, stick a 180, tempo dal frame di inversione a
   420 cm/s nell'altro verso, tempo sim 16 ms esatti, srv e cli). Prima: 612 cm/s -> zero 0,192 s, 420 a 0,576 s (Saved/Logs/qam/batch-rev0e.log,
   `run_movement_qa.ps1 -Names move_reversal -Tag rev0e`, QAM_REV_RUN=7: il PIE del portatile è più lento del wall clock).
2. Causa (bug, non la clip): il calo di stadio della tight turn riavvia la rampa di stadio 1, e GetAccelerationRamp le sottraeva di nuovo i 21 frame
   dello stamp di partenza -> 21 frame al 30% dell'accelerazione. Fix in GSTN_RunStageLogic.cpp (offset solo per la rampa nata con lo stamp).
   Solo il fix: 0,352 s (batch-rev1fix.log). Modello offline Tools/GAS/reversal/reversal_model.py (SmoothWalkingMode 1-D) riproduce le due curve.
3. Regola 14: Assistant (docs/notes/assistant/2026-11-20_reversal.md): lo stock non ha frenata/accelerazione da pivot (IsPivoting solo ABP), un 180
   usa i parametri di camminata; per un'inversione selettiva consiglia un boost temporaneo nel sync state. Divergenza 12 in gasp-divergenze.md.
4. `FGSTN_RunReversalTuning` (RunStages.Reversal nel profilo, condiviso da tutti i kit via DA_Locomotion_Default): StickCos -0,5, MinStickMagnitude 0,5,
   MinSpeed 250, TurningStrength 16, AccelerationMultiplier 1,3, EndSpeedFraction 0,9, MaxFrames 40. Contatore ReverseFrames nel sync state degli
   stadi (==, rete, ShouldReconcile, ToString). Valori scritti da Tools/GAS/reversal/reversal_setup.py (REV_*; REV_ENABLED=0 = spento).
5. Build OK (Saved/Logs/ci/build_rev1.log); test RunStages+NP.RunStages+NP.Stats 13/13 Success (`grep -c 'Result={Success}' test_rev1.log.ue.log`),
   nuovo GSTNCombat.Core.RunStages.Reversal.
6. Con Reversal on: da 853 cm/s 420 a 0,32 s (batch-rev2on.log), da 532 cm/s 0,272 s (batch-rev3reg.log), PASS srv e cli, 0 reconcile.

## Da sapere
- move_run_stages FAIL "90% within 20 frames" (frame 37): il check è precedente allo stamp di 21 frame (11-18), percorso della spinta invariato
  (test "after the stamp" verde). Da aggiornare il check, non il gioco.
- La frenata ora dura ~0,11-0,18 s (scivolata 23-68 cm): se l'owner la vuole più lunga, abbassare Reversal.TurningStrength/AccelerationMultiplier.
- Animazione non guardata: sul percorso base 1 la pivot è quella stock del chooser (riga PivotSet disabilitata). Nessun video all'owner.

## Prossimo
1. Playtest owner del feel (solo dati, nessuna review video necessaria). 2. Clip pivot di CORSA / posa di frenata + mirror (skid-walkturn-3 §2),
   tempi della clip ora da allineare a ~0,3 s. 3. Aggiornare il check run start di move_run_stages allo stamp.

COSTO: ~10 $ | 0 min desktop | 0 token review
