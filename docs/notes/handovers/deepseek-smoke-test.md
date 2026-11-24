# Handover: deepseek-smoke-test (M0 + M1_CODE + EULA guard)

- **Branch / Commit:** `deepseek_smoke_test` (base `a5b20950` -> questo commit)
- **Macchina:** fisso (editor chiuso; `GASP_Giustino2_ue59` intatto)

## Risultati smoke test DeepSeek (via `Invoke-Worker.ps1` + `New-DeepSeekWorktree.ps1`)
1. **Guardia EULA Epic (filesystem + testo):**
   - Su worktree pieno (`deepseek_smoke_test`): rifiutato con `EXIT=2` (`'Plugins/EngineOverrides' esiste su disco`).
   - Su prompt con riferimento Epic: rifiutato prima della chiamata (`riferimento bloccato ('plugins/engineoverrides')`).
2. **Worktree sparse EULA-safe:** creato `%TEMP%\ds-smoke-wt` su branch temporaneo `claude/ds-smoke-tmp` senza `EngineOverrides`/`GASNPP`, poi rimosso pulito (`-Remove` + `branch -D`).
3. **M0 (`deepseek-flash[1m]`):** `PASS` in `12s`, costo `$0.1746` (`Saved/Logs/ci/worker_deepseek-smoke-m0_20261122-194128.log`). Elencati i 12 `.ps1` in `Tools/CI/` (1853 righe) e le 38 cartelle `GSTN*` + `UE_MCP_Bridge` in `Plugins/`.
4. **M1_CODE (`deepseek-v4-pro[1m]`):** `PASS` in `59s`, costo `$0.4233` (`Saved/Logs/ci/worker_deepseek-smoke-m1_20261122-194150.log`). Creazione e lettura di `Tools/CI/_smoke_m1_tmp.txt` OK; cancellazione (`rm`/`Remove-Item`/`git clean`) bloccata in `acceptEdits` dai permessi Claude Code (nota operativa: i worker DeepSeek possono creare/modificare file, non cancellarli senza allowlist).
5. **`handoffs/costi.csv`:** registrate automaticamente le 2 righe `worker-M0` e `worker-M1_CODE` a 11 colonne.

## Nota operativa
- DeepSeek è pronto per i task M0/M1_CODE della coda (`Run-Queue.ps1` / `Invoke-Worker.ps1`) usando worktree sparse `New-DeepSeekWorktree.ps1` e `MaxUsd >= 1.0`.

COSTO: 0.60 $ | 3 min desktop | 0 tok review | provider=deepseek+agy_student | quota_pro=0%
