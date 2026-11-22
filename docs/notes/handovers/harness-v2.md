# Handover: harness-v2 checkpoint (hook _ue59 attivi, calibrazione 2C 1/4, memoria AGENTS.md)

- **Branch / Commit:** `claude/harness-v2` (`52256143` -> `a0134cfc` Claude PASS r2 -> `01d060d3` fix post-PASS -> questo checkpoint)
- **Macchina:** fisso (editor chiuso; `GASP_Giustino2_ue59` intatto, 0 file sporchi toccati; `_ue59/IN-USO.txt` allineato)

## Completato dopo PASS Claude r2 (`a0134cfc`)
1. `Invoke-Worker.ps1` (`01d060d3`): recupero automatico di `_ue59/IN-USO.txt` dopo crash worker su stesso host con PID morto (lock manuali/PID attivi/altri host restano bloccati); normalizzazione exit figlio `cli.py` `2 -> 1` per evitare loop infiniti in `Run-Queue.ps1`.
2. `.githooks/pre-commit`, `.githooks/pre-push` (LF-only UTF-8) + `Install-GitHooks.ps1`: installati in `C:/Users/jacop/Tools/gstn-gitguard/.githooks` con fallback a `Tools/CI/Test-GitGuard.ps1`; verificati attivi anche dentro `_ue59` (`pre-commit` OK rc=0, `pre-push` su `main` bloccato rc=2).
3. Verifiche verdi dopo ultima modifica PowerShell: tutti i 5 `-SelfTest` (`Test-GitGuard`, `Run-Timed`, `New-DeepSeekWorktree`, `Invoke-Worker` 6/6, `Run-Queue`) + `agent-bridge/test_bridge.py` (`ALL TESTS PASSED`).
4. `AGENTS.md` root creato per auto-bootstrap della memoria indicizzata nelle nuove chat (`INDICE.md` -> handover -> `memoria-progetto.md` -> `regole-progetto-2026-11-22.md`).
5. Calibrazione cieca 2C `gemini-4-neon-high` (`docs/notes/reviews/calibrazione-2c-neon-2026-11-22.md`): **`K6 r1` completato (`FAIL posa` = match perfetto 1/1 con Fable `2bd9b2ef`, 0 falsi PASS, `0 $`)**; pausa voluta al ~60% finestra 5h.

## Prossimi passi alla ripresa (in chat nuova su `Hydrogen High`)
1. Eseguire i 3 casi ciechi restanti (`K6 r2` `84b7c354`, `v27 r2` `22300411`, `v28 r3` `3c779b08`) **solo con griglie `.jpg`** (no `.mp4` in blocco; cartelle pronte in `%TEMP%\cal2c\`, fonti anche in git) e chiudere `calibrazione-2c-neon-2026-11-22.md` (`>=3/4` + `0` falsi PASS).
2. Quando l'owner ha completato il pagamento DeepSeek e impostato `DEEPSEEK_API_KEY` (env User Windows), eseguire test 10 min su worktree sparse via `Tools/CI/New-DeepSeekWorktree.ps1`.

COSTO: 0 $ | 7 min desktop | 0 tok review | provider=agy_student | quota_pro=0%
