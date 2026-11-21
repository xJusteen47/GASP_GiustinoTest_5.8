# Handover run-stages-s1 (cloud, Opus, 2026-11-21) - stadi di corsa leggibili, S1 codice + dati

**Obiettivo**: S1 del piano docs/notes/run-stages-leggibili-piano.md (decisioni owner 1A 2A 3B). Base: claude/runstart-generic-desktop 8cb52ea7.
**Esito**: codice scritto, NON compilato (niente editor nel cloud). Piano di test: docs/notes/run-stages-s1-test-plan.md.

Fatto:
- Sim (GSTNCombatCore, FGSTN_RunStage > Readable Step): `EntryBurstFrames` + `EntryAccelMultiplier` (salita di stadio = gradino, stessa
  via del boost dash), `TellFrames`, `ChargeFrames` (finestre prima dello stadio successivo, solo presentazione). Uno stadio con burst non ha
  rampa (1A: la rampa resta solo da fermo). Stato: `EntryFrames`, `StageEnteredFrame` (in ==, rete compatta, ShouldReconcile per EntryFrames).
  Getter: IsStageEntryBurst, IsStageTell, GetStageTellAlpha, IsStageCharge; GetAccelerationRamp con parametro readable.
- Interruttore `gstn.RunStage.Readable` (1 default, 0 = corsa continua vecchia), letto da sim e presentazione, in GSTN Debug Options
  (prefisso gstn.RunStage., una casella; test OptionsWindow esteso).
- Presentazione (UGSTN_RunPresentationComponent): GetRunStage (1-based), IsRunStageTell/TellAlpha/Charge/EntryBurst, GetRunStageTime,
  GetRunStageEnteredFrame, delegate OnRunStageChanged; getter AnimOwner per ABP e chooser. Lean: `StageForwardLeanDeg` per stadio e
  `TellForwardLeanDeg` (si approfondisce col tell) sullo stesso layer additivo, spenti con Readable 0.
- Dati: Tools/GAS/setup_run_stage_steps.py (stadio 2 burst 3 f x6, tell 20 f, charge 6 f; stadio 3 burst 2 f x8; lean 0/6/8 + tell 6).
  Valori di partenza, da giudicare a video. Tutto a 0 nel C++ = comportamento attuale.
- Test: GSTNCombat.Core.RunStages.ReadableSteps (nuovo). Test component JSON: run.entry, run.stageFrom, run.inStage.
- gasp-divergenze.md riga 24. Unreal Assistant NON chiesto (cloud): passo 1 del piano di test, prima del merge.

Scelte applicate (regola 6): tell e charge sullo stadio che si LASCIA (come il piano §4); il delegate dice bUp, non "salita vs boost"
(per lo scoppio VFX si guarda IsRunStageEntryBurst); con Readable 1 anche un boost o una caduta nello stadio 2 non hanno rampa.
Non in S1: VFX (S2), clip/righe chooser RunStage e posa di carica (S3), blend corto al cambio stadio (S3, sua riga divergenze).

Prossimo: thread sul FISSO col piano di test (build, test, dati, misura, cattura dietro + lato). Poi S2.
COSTO: 4 | 0 | 0
