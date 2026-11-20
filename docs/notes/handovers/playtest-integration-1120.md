# Playtest integration 11-20 (fisso, Opus as Sonnet-medium task, 2026-11-20 ~21:15-21:40 UTC)
Branch `claude/playtest-cam-render-skid` from `claude/runstart-v6-desktop` 0c487b54 (run start v4r6 active). Checkout _ue59.

## Fatto
1. Merge: hiq1zx (camera anchor early + GSTN|Camera pivot BP), xmve9g (render profiles, default deferred no Lumen + TSR), k5vbaf (run reversal).
   Conflitti: speed-up.md / gasp-divergenze.md uniti (k5 righe rinumerate 15-16); DA_Locomotion_Default e CHT_GSTN_StockMover_PoseMatch = NOSTRI
   (DA dell'owner, chooser di run start v4r6). Quindi la riga PivotSet di skid-walkturn-3 (era disabilitata, FAIL) NON c'è: divergenza 15 lo dice.
2. Valori reversal scritti nel DA dell'owner con `Save-Assets.ps1 -Script Tools\GAS\reversal\reversal_setup.py` (Saved/Reversal/reversal_setup.txt):
   enabled False -> True, TurningStrength 30 -> 16, AccelerationMultiplier 2 -> 1.3; run_stages.stages identici prima/dopo (stadio 1 FramesToNextStage 180).
3. Build OK (Saved/Logs/ci/build_playtest1.log, 321 s). Test 76/76 Success
   (`Run-Timed -Preset Test -Tests "GSTN.Camera+GSTNCamera+GSTNCombat.NP.BodyCapsule+GSTNCombat.Core.RunStages+GSTNCombat.NP.RunStages+GSTNCombat.NP.Stats"`,
   `grep -c 'Result={Success}' Saved/Logs/ci/test_playtest1.log` = 76).
4. move_reversal PASS srv e cli: 472 cm/s -> zero 0,112 s, 420 nell'altro verso 0,256 s, scivolata 19 cm (Saved/Logs/qam/batch-pt_early1.log;
   `run_movement_qa.ps1 -Names move_reversal,move_cam_jitter -Tag pt_early1`). Nota: con lo stadio 1 lungo dell'owner la corsa resta a 472.
5. Nuovo scenario `move_cam_jitter` (POV del PlayerCameraManager del client, |seconda differenza| tra frame, limite 5 cm; salto, salto in corsa, atterraggio):
   Early 1: 220 frame a 29 fps, max 4,2 cm, 0 kick (batch-pt_early1.log). Early 0: 281 frame a 39 fps, max 65,3 cm, 13 kick a 1,40-1,54 s e 3,95-4,62 s
   (salto e salto in corsa) (batch-pt_early0.log, QAM_CAM_CVARS='gstn.Camera.Anchor.Early 0;gstn.Loco.Base 1'). Dropkick NON misurato.

## Non verificato
- Flicker della luce: non misurabile headless; passi per l'owner (Render.Profile 1/2) nel messaggio di playtest.
- Look senza GI su L_Test_Hub (sky light / fill) e se un post process volume rimette Lumen: da guardare nel playtest (`gstn.Render.Info`).
- gstn.Skid.Sim è 1 di default: l'inversione da 0,3 s è misurata con `gstn.Skid.Sim 0`.

## Prossimo
Playtest owner; se ok, merge del branch nella linea del fisso. move_run_stages check "90% in 20 frames" ancora da aggiornare (skid-reversal-1).

COSTO: ~6 $ | 15 min desktop | 0 token review
