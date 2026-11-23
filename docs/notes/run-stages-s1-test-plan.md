# Run stages leggibili S1: piano di test per il fisso (branch claude/project-thread-njoso3)

Codice NON compilato (thread cloud). Macchina: fisso, editor chiuso per build e script. Ordine:

1. **Unreal Assistant PRIMA del merge** (regola 14, riga 24 di gasp-divergenze.md): chiedere "how does stock GASP Mover handle Run -> Sprint
   (gait, acceleration, loop change) and is there a stock way to enter a gait with a transition clip?" con
   `Tools/UE59/ask_unreal_assistant.py`; risposta in docs/notes/research/unreal-assistant-run-stages.md, una riga nella colonna Stato.
2. **Build** incrementale reale con lo script a tempo (45 min, regola 5). Moduli toccati: GSTNCombatCore, GSTNCombatNP, GSTNCombatDebug,
   GASP_Giustino2. Header cambiati: GSTN_RunStageTypes.h, GSTN_RunStageSyncState.h, GSTN_RunPresentationComponent.h, GSTN_LocomotionLean.h.
3. **Test headless** (30 min): `Automation RunTests GSTNCombat.Core.RunStages` (atteso 9/9: gli 8 vecchi + `ReadableSteps` nuovo; conteggio: `grep -c IMPLEMENT_SIMPLE_AUTOMATION_TEST GSTN_CombatTests_RunStages.cpp` = 9), poi
   `GSTN.Debug` / `GSTNCombatDebug` (OptionsWindow: `gstn.RunStage.Readable` elencato, una sola casella), poi la suite GSTN completa
   (atteso = conteggio dell'ultimo handover + 1, 0 failed). Log in docs/notes/reviews/run-stages-s1/.
4. **Dati**: editor chiuso, `UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_run_stage_steps.py" -unattended -nullrhi`.
   Prima `STEPS_MODE=dump` per salvare i valori di ripristino. Atteso: "1 profiles, 1 anim sets set". Valori: stadio 2 burst 3 f x6, tell
   20 f, charge 6 f; stadio 3 burst 2 f x8; lean in avanti per stadio 0/6/8 gradi, tell +6 gradi. La rampa da fermo (stadio 1) non cambia.
5. **Misura sim** (PIE, `gasnpp_test.py` o la cattura col JSON del test component: campi nuovi `run.entry`, `run.stageFrom`, `run.inStage`):
   corsa dritta stick pieno su L_Test_Hub. Atteso: al passaggio 1->2 la velocità passa da ~90% a 100% del massimo dello stadio 2 in <= 4
   frame sim (prima: rampa 30 f); 2->3 in <= 3 frame; `inStage` nella finestra tell = ultimi 20 frame dello stadio 2. Con
   `gstn.RunStage.Readable 0` la curva di velocità deve tornare quella vecchia (A/B, stessi frame). Nessun "Sync/Aux mismatch" in 2 client.
6. **Cattura video** (regola 10 e regola dell'owner: vista da DIETRO + vista LATERALE, finestre sul secondo monitor): corsa 0 -> stadio 3,
   Readable 1 vs 0, stessi frame. Controllo a vista: lean che si approfondisce nel tell, gradino visibile, nessun pop del busto al cambio
   stadio (il limite del lean cresce con l'extra, il cambio stadio non deve far scattare il busto). Le VFX (S2) e le clip (S3) non ci sono:
   si giudica solo velocità + lean.
7. Se un numero non torna: max 2 tentativi di fix (regola 5), poi handover con cosa serve.

Getter per S2/S3 (ABP, chooser, VFX): GetAnimOwnerRunStage (1-based), IsAnimOwnerRunStageTell, GetAnimOwnerRunStageTellAlpha,
IsAnimOwnerRunStageCharge, GetAnimOwnerRunStageTime; sul componente anche IsRunStageEntryBurst, GetRunStageEnteredFrame, OnRunStageChanged.
