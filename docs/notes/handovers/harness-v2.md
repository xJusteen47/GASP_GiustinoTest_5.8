# Handover: harness-v2 (setup multi-modello, guardie e export cloud)

- **Branch / Base:** `claude/harness-v2` off `claude/runstart-generic-desktop` (`3c779b08`)
- **Macchina:** fisso (senza editor, nessuna modifica a `GASP_Giustino2_ue59`)

## Modifiche completate
1. **Export cloud e regole:** copiati in repo `docs/notes/memoria-progetto.md`, `docs/notes/audit_setup_agentico.md`, `handoffs/INDICE.md`; normalizzato `handoffs/costi.csv` a 11 colonne (`provider`, `quota_pro_pct`, 52 righe storiche intatte); creato `handoffs/queue.json` (14 voci da `## Coda`).
2. **Governance unificata:** creata `docs/notes/regole-progetto-2026-11-22.md` (regole 1–15 + tier `M0`–`M3`, EULA Epic, segreti env-only, VRAM guard); marcate `SUPERSEDED` `regole-progetto-2026-11-20.md` e `docs/notes/anim-process.md` (backup `anim-process.backup-2026-11-22.md`, OK owner `1A`); aggiornato `CLAUDE.md` a UE 5.9 senza Rider MCP.
3. **Guardie git e runner:** aggiunti `Tools/CI/protected-paths.json`, `Test-GitGuard.ps1`, `Install-GitHooks.ps1`, `.githooks/pre-commit`, `.githooks/pre-push`; esteso `Run-Timed.ps1` con `-LockFile`/`-LockOwner` (exit 2) e `-PreCheck` (exit 4); creati `Invoke-Worker.ps1` e `Run-Queue.ps1` (R5-bis deterministica, 0 token).
4. **Hardening `C:\Users\jacop\Tools\agent-bridge\`:** `DEEPSEEK_API_KEY` solo env, guard EULA Epic (`--disallowedTools` + check path), guard VRAM su `call_local`.

## Verifiche (tutte verdi dopo ultima modifica)
- `Tools/CI/Run-Timed.ps1 -SelfTest`: PASS (casi 1–7, rc=0)
- `Tools/CI/Test-GitGuard.ps1 -SelfTest`: PASS (6/6 casi, rc=0)
- `Tools/CI/Invoke-Worker.ps1 -SelfTest`: PASS (routing, guardie, lock `IN-USO.txt`, CSV 11 col, rc=0)
- `Tools/CI/Run-Queue.ps1 -SelfTest`: PASS (dipendenze, blocked, R5-bis `needs_diagnosis`, dryrun, rc=0)
- `C:\Users\jacop\Tools\agent-bridge\test_bridge.py`: `ALL TESTS PASSED` (rc=0)
- Validazione Python `queue.json` (14 voci) + `costi.csv` (52 righe, 11 colonne): `JSON+CSV_OK`

## Prossimi passi
- Review cloud singolo giro su `origin/claude/harness-v2` (Claude Pro).
- Dopo merge/OK: `Tools\CI\Install-GitHooks.ps1`, test DeepSeek 10 min con `DEEPSEEK_API_KEY` in env, calibrazione cieca 2C `Gemini 4 Neon (High)` sulle 4 review.

COSTO: 0 $ | 0 min desktop | 0 tok review | provider=agy_student | quota_pro=0%
