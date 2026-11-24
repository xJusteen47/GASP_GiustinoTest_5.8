# T3 skill animazione unica - integrazione (fisso, 2026-11-24, Opus; brief handoffs/skill-anim-unica-brief-2026-11-24.md)
Branch `claude/anim-skill-t3` = T1 1a194c6c + T2 82514b2c (merge pulito) + fix. Worktree GASP_Giustino2_lifecal. _ue59 solo headless.
FATTO
- Build editor con GSTNCapture: `Run-Timed.ps1 -Preset Build -ExtraArgs -NoHotReloadFromIDE` = Result: Succeeded (Saved/Logs/ci/t3_build.log).
- Test offline (ogni `python tests/test_*.py`, log in scratch): 30 file, 7 rossi dopo i fix. Fix: ref carta dair v4 ->
  overlord/dair/overlord_dair_v4.json + percorsi in test_locks/test_casc_spacing/test_capture (cartelle per mossa, 899d7524).
  Rossi preesistenti: test_capture (manual_path), test_casc_roundtrip (sheets/raw mancante), test_verbs, test_vframes,
  test_principi_review; test_skill_gaps e test_t0_prep = WinError 32 (file temp bloccato, solo Windows).
- Dump (clip_joints_dump_ue.py, CJD_FACING=y, uasset presi da 6blrlz e tolti dopo): K7, K8 (r2, 44a40810), SettleK6,
  StartupK6, LoopK6, w3 -> docs/notes/life-calib-run/dumps/. life_calibration.json: K6 settle = AS_OverlordDropkick_SettleK6.
- `python life_calibrate.py`: CALIB FAIL 11/13 (docs/notes/life-calib-run/life_calibration_result_t3.json).
  dair_approved: il dump di AS_OverlordDAirV4_Build e' STATICO (bacino e testa 0,0 cm su 43 frame, mani/piedi assenti):
  difetto del dump/asset, non della metrica. dropkick_k6_settle: atteso FAIL, esce PASS (mai giudicato a video: non e'
  un difetto noto). => life_gate.json NON scritto ne' committato (regola 17: manca la prova completa).
- anim_layer in UE (prima volta): dump, 2 strati STEP_C1_02, build = PASS, conservazione fuori finestra 0,0 gradi.
  Confronto con w3 approvata (stesso script di dump): gomito destro entro 0,9 cm f1-21, ma mano destra fino a 30 cm (f7):
  la sorgente (w3 con W3_ARM_R_OUT=0) non e' "w3 meno lo strato", perche' nel codice w3 attuale la flessione del gomito di
  c2 passo-01 si calcola DOPO l'apertura del braccio. Prova di equivalenza non valida cosi'; va rifatta con lo src del
  codice w3 a 22a318f1. Dati: docs/notes/anim-layer/t3-ue-run-w3-compare.json, script gstn_anim_mcp/t3_layer_check.py.
- anim-approvate.md: righe w3 (967de748), jab v12, ftilt v3.1 (owner nel thread 2026-11-24 00:43Z: "sono approvate").
  jab v12 e ftilt v3.1: uasset NON in git (solo Content/AnimPipeline del fisso). K7 e' approvata ma e' anche un FAIL atteso.
PER L'OWNER
- Tag (la guardia li blocca): `git tag approved/runstart-w3 967de748`, `git tag approved/jab-v12 <commit righe>`,
  `git tag approved/ftilt-v31 <commit righe>`, `git tag approved/dropkick-k7 ac8d6201`, `git tag approved/dair-v9 916338ad`, poi `git push origin --tags`.
- dair: owner 01:26Z sceglie v9 (riga scritta, AM con la sua modifica locale = 916338ad). Migliorarla (silhouette,
  studio Doom) = variante nuova, thread separato. Taratura: dair_approved punta ancora a V4, va spostata su V9; le FAIL v9 (cp5, cp5b, blendout) erano lato gioco.
- Antigravity: togliere la vecchia skill (bloccato dal classificatore): `cmd /c rmdir "%USERPROFILE%\.gemini\config\skills\gstn-anim-cycle-cheap"`
  e `New-Item -ItemType Junction -Path "$HOME\.gemini\config\skills\gstn-anim-cycle" -Target "<GASP_Giustino2>\.claude\skills\gstn-anim-cycle"` (attiva dopo il merge).
SUBAGENTI: 0 (lavoro meccanico breve fatto con script a log, non delegato)
COSTO: ~6 $ (stima) | 40 minuti desktop (build 25 + 4 run headless) | 0 token review
