# Playtest fixes 1117c (fisso, _ue59, Opus, 2026-11-17 01:15-~04:00Z). Branch `claude/playtest-fixes-1117c` da `claude/playtest-fixes-1117` c2a2c1d6
Diag in PIE: `Saved/PF1117c/diag_launch.py` (RT strong su manichino a 0/40/100 %, KIT_Test_Light), `Saved/PF1117c/diag_dair.py`, `Saved/PF1117b/diag_pie.py`
(lanciati con `python Saved/AnimR2/run_in_editor.py <script>`; righe in `Saved/PF1117c/diag/*.jsonl`, riassunto `python Saved/PF1117c/summ.py <jsonl>`).
Ricontrollo regola 11: claim 1117b "crouch+X a terra = jab, probe tolta" confermato (Saved/PF1117c/kits_dump.txt: Attack.Neutral -> GSTN_GA_TestHitMove).
## Fatto
1. Back air (C++ `GSTN_CombatSimComponent::ProduceInput`): in una mossa senza steer la facing resta quella che ha (o quella del SetFacing in coda,
   lo snap iniziale). Prova: crouch.jsonl f279-330 yaw 180 fisso (prima tornava a 0 a ~5°/frame).
2. A terra: durante una mossa forzata con Lock_Movement (rimbalzo dopo knockdown, atterraggio del dropkick) lo stick non cammina né ruota. Non misurato in PIE.
3. Lancio/rimbalzo/a terra. CAUSA (log Verbose `LogGSTNKnockbackMove`, GASP_Giustino2.log: "elapsed 1 mode Falling want (1952 0 1128) have (0 0 0)"):
   la velocità di fine dell'hitstop freeze azzerava il lancio nel suo primo frame; il manichino andava Falling con v=0 e tornava Walking (tumble a terra).
   Fix `FGSTN_LayeredMove_Knockback::GenerateMove`: al 2° tick, se il modo è quello forzato e la velocità verticale è persa, rilancia (deterministico).
   Poi: colpito mentre è a terra il knockdown finisce (il tag teneva la posa sdraiata in volo); a fine MaxDownedFrames parte il get-up neutro
   (`FGSTN_TumbleLanding::AfterClocks`, `GetupNeutral` nell'entity data); `DA_DefenceRules_Test(PvE)` MaxDownedFrames 0 -> 90 (setup_playtest_downed_1117.py).
   Prova strong_40.jsonl: f35 Falling, f68 GroundHit (rimbalzo), f83 LieUp, f153 GetUp, f179 idle. ABP/slot: i montage Tumble/GroundHit/LieUp/GetUp
   partono tutti (righe 'm'): i blocchi erano lato simulazione, nessuna modifica ABP.
   RT non arrivava al gioco: `Input.Button.Strong` mancava in IC_GASNPP_Test (aggiunto con movement_setup.setup_buttons; ha anche messo IA_GASNPP_Shield sul bottone Shield, prima vuoto).
4. Kit playtest (setup_playtest_launch_1117.py, out docs/notes/reviews/playtest-hurtbox-1117/launch_1117.txt): base 780 (lancia a 0 %), 30° per X a terra
   del Playtest (GA_Playtest_GroundHit, figlio di TestHitMove), tilt di Light/Medium/Heavy; special: solo base >= 780, angolo invariato; strong: copia
   GA_Playtest_Strong 30° base 1100 nei 3 archetipi. Jab archetipi invariati. Nuove funzioni C++ Describe/WriteMoveKnockback.
5. Camera: modificatore `UGSTN_PlaytestCameraModifier` (+300 cm indietro, +60 su; cvar gstn.Camera.PlaytestExtraDistance/Height in DefaultEngine.ini),
   asset GASP intatti. Distanza misurata 228 -> 430-525 cm (crouch.jsonl colonna cam).
6. Hurtbox HBS_Test_Pawn: piedi r20 da pavimento, busto r22, testa r12 fino a ~170 cm (schema prima/dopo docs/notes/reviews/playtest-hurtbox-1117/; è un grafico, non una cattura in-engine).
7. DAir V9: solo diagnosi, brief `docs/notes/handovers/brief-dair-v9-charge-jitter.md` (blend air crouch -> frame 0 +34 cm, capsula ±12 cm).
## Scelte applicate (regola 6)
Jab non lancia a 0 %; special mantiene l'angolo; auto get-up a 90 frame anche per il giocatore (regola globale dei test); bottone Strong aggiunto all'IC.
## Da confermare (owner)
- Auto get-up 90 frame va bene? A 0 % il lancio è un saltello con animazione HitStand (non tumble): ok?
## Test: `Run-Timed.ps1 -Preset Test -Tests GSTN -Log Saved\Logs\ci\pf1117c-test1.log` ESITO OK, 1719 Success / 0 altri (Select-String "Test Completed. Result=\{(\w+)\}"). Build: pf1117c-build1..4 (build2 FAIL: categoria di log, corretto). Costo stimato ~20 $ (Opus, 4 build, ~6 PIE).
