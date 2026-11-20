# run-stages-leggibili-1: piano stadi di corsa leggibili dal riferimento Mario 3D World (cloud, 2026-11-20)
Branch claude/project-thread-le0tc7 (da feature/gas-npp-5.8). Solo piano, nessun codice. _ue59 non toccata.

- Piano: docs/notes/run-stages-leggibili-piano.md (copia in project files handoffs/run-stages-leggibili-piano.md).
- Griglie riferimento: docs/notes/reference-dossiers/run-stages-mario3dw/ (ffmpeg dal video dell'owner IMG_9588.mov,
  3 griglie guardate + 3 di ricerca; tempi letti a occhio, +-1 frame a 30 fps, personaggio ~40 px).
- Fatti letti (rule 11, sola lettura): FGSTN_RunStage in GSTN_RunStageTypes.h (98c3b9f) ha già FramesToNextStage,
  NextStageSpeedFraction, AccelRampFrames/RampStartFraction; FGSTN_RunBoostDashTuning (burst) esiste; divergenza 13 =
  rampa stadio 1 62 f / 0,15 e stadio 2 30 f / 0,25 (runstart-v5-desktop.md §3).
- Tensione segnalata: rampa continua (Toad) vs gradini istantanei del riferimento -> decisione 1 del piano.
- Owner 20:38Z: ignorare che in 3D World lo sprint è a tasto. Owner 20:43Z: tutto regolabile (piano §4b).
- Da chiedere all'Unreal Assistant prima di S1: come GASP stock gestisce Run->Sprint (blend, transizione) - piano §4.
- Decisioni owner 20:47Z: 1A 2A 3B (piano §8). Owner vuole 3 loop Overlord: corsa lenta Ganon (stadio 1), braccia aperte
  (stadio 2, lettura scelta), sprint più veloce (stadio 3); riferimento in arrivo. Prossimo: thread S1 codice+dati (nuovo),
  poi S2 VFX, S3 clip dopo il riferimento.
COSTO: ~4 $ | 0 min desktop | 0 token review
