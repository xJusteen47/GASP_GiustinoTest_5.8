# Handover camera-p1a (cloud, Opus high, 2026-11-21)
Request: coordinator + owner 21/11 ("Libera di default, si puo bloccare o focussare su alcune parti del livello senza hard lock. 2 per ora no, 3 A"). P1a = code + headless tests only.
Branch claude/project-thread-w6l7mz from claude/camera-mario-research (03afc437). Only Plugins/GSTNCamera + docs touched; no game module, no Blueprint, no _ue59.
Doc: docs/notes/camera-platforming/p1a.md (what was built, decisions applied, desktop P0 test list).
Built in the plugin:
- UGSTN_CameraProfile Data Asset replaces Project Settings (UGSTN_CameraSettings and Config/DefaultGSTNCamera.ini deleted).
- Free is the default mode; asymmetric pivot lag, fall follow 120 cm / 0.25 s, fall pitch by speed, look-ahead 100 cm, pitch->distance/FOV curves.
- Player distance presets 0.8/1/1.25/Custom, eased 0.45 s, section limits; DistanceAction input.
- Auto follow behind (2A) with player switch; Camera Assist; stick latching (ResolveMoveReferenceRotation).
- AGSTN_CameraBiasVolume: soft framing (direction/focus/pitch/zoom/FOV/pivot shift, Soft/Firm, never a hard lock) = owner decision 1.
- FGSTN_CameraUserOptions + save slot per local player.
Tests: 8 new Core + 6 new Runtime, 5 old Runtime checks Follow->Free (list in plugin README). NOT compiled, NOT run: no engine in the cloud (rule 11: no numbers claimed).
Self-check done: brace/paren balance of every touched file (python count, 0 except a pre-existing comment paren in GSTN_CameraMath.h).
Risks: compile errors in UE 5.9 API spots (FRuntimeFloatCurve eval, UGameplayStatics save, TMap FindOrAdd with weak keys); the anchor adapter is still P0 (game module has no dependency on the plugin yet).
Next: desktop P0 (handoffs/camera-p0-brief.md + p1a.md list): build, run GSTNCamera.*, adapter, C1/C4, captures. Then P1b tuning on video.
COSTO: ~9 $ | 0 minuti desktop | 0 token review
