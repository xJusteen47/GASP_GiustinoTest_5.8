# Handover: harness-v2 Round 2 (fix review Claude 52256143)

- **Branch / Base:** `claude/harness-v2` (commit Round 1 `52256143` -> Round 2)
- **Macchina:** fisso (senza editor; `GASP_Giustino2_ue59` intatto)

## Fix applicati su review Claude (obbligatori 1–7 + opz. c)
1. `Test-GitGuard.ps1`: `git -c core.quotepath=false diff --cached --name-only --no-renames -z` UTF-8 per intercettare `git mv` su file protetti e path non-ASCII.
2. `.githooks/pre-push` + `Test-GitGuard.ps1`: blocco force-push via `merge-base --is-ancestor`, `-RemoteSha`, e `</dev/null` su `powershell.exe`.
3. `Invoke-Worker.ps1` + `protected-paths.json`: lock atomico `FileMode.CreateNew` (`.lock` con host+PID, recupero stale PID morto, rispetto lock manuali `IN-USO:`), path assoluto unico `ue59_lock_path` condiviso tra worktree.
4. `Invoke-Worker.ps1` + `Run-Timed.ps1`: hard timeout proprio (`WaitForExit` + `taskkill /F /T`) in entrambi (`exit 124` + `ESITO: TIMEOUT`); `Test-LockFree` con match esatto su prefisso owner (opz. c).
5. `Invoke-Worker.ps1` + `New-DeepSeekWorktree.ps1` + `agent-bridge/bridge_core.py`: guard EULA Epic su filesystem (rifiuta DeepSeek se `Plugins/GASNPP` o `EngineOverrides` esistono sotto `Cwd`/repo); helper worktree sparse senza cartelle Epic.
6. `Invoke-Worker.ps1` + `bridge_core.py`: `GSTN_OWNER_UNLOCK` rimosso dall'env dei processi figli.
7. `Run-Queue.ps1` + `handoffs/queue.json`: `rc=2` (`RIFIUTATO`) non consuma tentativi R5-bis; `-Cwd` esplicito obbligatorio per voce (rifiuta `_ue59` se `uses_ue59=false`).

## Verifiche (tutte verdi dopo ultima modifica)
- `Test-GitGuard.ps1 -SelfTest`: PASS (`DA+Mv`, `Pose+Utf8`, `ApprovedAnim`, `SecretScan`, `CleanAllow`, `Push+Force`, rc=0)
- `Run-Timed.ps1 -SelfTest`: PASS (casi 1–7 incl. exact owner e PreCheck timeout 124, rc=0)
- `New-DeepSeekWorktree.ps1 -SelfTest`: PASS (sparse checkout senza `Plugins/GASNPP`/`EngineOverrides`, rc=0)
- `Invoke-Worker.ps1 -SelfTest`: PASS (`Route`, `Guards`, `Lock+Stale`, `Env+Timeout`, `CostCsv`, rc=0)
- `Run-Queue.ps1 -SelfTest`: PASS (`Rc2NoCount`, `TimeoutR5bis`, `BadCwd+Skip`, `ExplicitCwd`, rc=0)
- `C:\Users\jacop\Tools\agent-bridge\test_bridge.py`: `ALL TESTS PASSED` (rc=0)

COSTO: 0 $ | 0 min desktop | 0 tok review | provider=agy_student | quota_pro=0%
