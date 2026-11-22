# Handover M12 parte 1 (cloud, 2026-11-21) - branch claude/project-thread-0f3p4m (base claude/runstart-generic-desktop 84cc0057)

OBIETTIVO: vite condivise, checkpoint, niente revive, logica missioni, senza dipendere da D5. ESITO: codice scritto, **NON compilato** (cloud senza UE).
- `Plugins/GSTNRespawn` (Core puro + Runtime) e `Plugins/GSTNMission` (Core puro + Runtime), README in ciascuno, righe in `Plugins/README.md`.
- Regole = pezzi del GameRules asset ("Respawn and lives", "Missions"): valgono con GameMode classico e con Experiences. Default OFF senza asset.
- Cvar nel GSTN Debug Options (`gstn.Respawn.*`, `gstn.Mission.*`, filtro in `SGSTN_DebugOptionsPanel.cpp`). Comandi Status/AddLives/Restart/Reset/CompleteActive.
- `.uproject`: abilitati GSTNRespawn e GSTNMission (niente GameFeatures). Scelte applicate (D-1, D-2, drop kit, invulnerabilità): plan m12 §7.

## Piano build + test (portatile o fisso, Haiku low; 1 tentativo = 1 build)
1. `_ue59/IN-USO.txt` segnato; `git fetch; git checkout claude/project-thread-0f3p4m` (o merge nella linea del playtest se l'owner lo vuole).
2. Build: `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\m12p1-build.log`. Errori probabili: include/firme UE 5.9
   (TActorIterator const, NewObject con template, TMap config). Max 2 tentativi (regola 5), poi handover con l'errore.
3. Test nuovi: `Run-Timed.ps1 -Preset Test -Tests "GSTNRespawn+GSTNMission" -Log Saved\Logs\ci\m12p1-test.log`, poi
   `Tools\CI\Check-TestLog.ps1 -UeLog Saved\Logs\ci\m12p1-test.log.ue.log -MinTests 7` (3 Respawn + 4 Mission).
4. Regressione: `-Tests GSTN` stesso schema, confronto col baseline dell'ultimo handover (react-hook-4: 1596/0) + 7 nuovi.
5. PIE nel hub, senza salvare asset (regola: salvare solo L_Test_Hub): console `gstn.Respawn.Enabled 2`, `gstn.Respawn.Lives 2`,
   `gstn.Respawn.Debug 1`; cadere in una KO zone due volte: 1a = ritorno dopo 2 s alla partenza, 2a = restart squadra dopo 3 s.
   Log atteso `LogGSTNRespawn: KO ... cost 1` e `Respawn ... (LevelStart, ok)`. Ripetere con `gstn.Respawn.Method 2` (in place).
   Da verificare: il pawn nuovo riceve il loadout giusto (drop kit con DropAndLose + RespawnKit agisce sul pawn vecchio).
6. Missione (G7 farà la mappa vera): GameRules asset di prova non salvato, una `AGSTN_ObjectiveZone` (Mission.Objective.Goal) via script; `gstn.Mission.Debug 1`.

## Aperto / per il coordinatore
- D5 resta all'owner: G1 (Experiences) e G2 `GSTNModeVerbs` (verbo Interact sugli obiettivi) non fatti.
- Invulnerabilità post-respawn solo dato; pod/rinforzi (Spedizione) non fatti; scenario harness `scenario_respawn_checkpoint` = G8.
Verifica (regola 11): test Core contati a mano nei file `Private/Tests` (3 + 4 IMPLEMENT_SIMPLE_AUTOMATION_TEST); nessun numero di build/test: non eseguiti.

COSTO: ~7 $ | 0 minuti desktop | 0 token review
