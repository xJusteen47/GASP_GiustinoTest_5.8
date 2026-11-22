# Handover: harness-v2 (hook _ue59 attivi, calibrazione 2C 4/4 SUPERATA, memoria AGENTS.md)

- **Branch / Commit:** `claude/harness-v2` (`52256143` -> `a0134cfc` -> `01d060d3` -> `65b8f0ba` -> questo checkpoint)
- **Macchina:** fisso (editor chiuso; `GASP_Giustino2_ue59` intatto)

## Completato
1. `Invoke-Worker.ps1` (`01d060d3`): recupero automatico `_ue59/IN-USO.txt` dopo crash su stesso host con PID morto; normalizzazione exit figlio `cli.py` `2 -> 1`.
2. `.githooks/pre-commit`, `.githooks/pre-push` + `Install-GitHooks.ps1`: attivi in `C:/Users/jacop/Tools/gstn-gitguard/.githooks` con fallback `Test-GitGuard.ps1`; verificati anche in `_ue59`.
3. Tutti i 5 `-SelfTest` (`Test-GitGuard`, `Run-Timed`, `New-DeepSeekWorktree`, `Invoke-Worker`, `Run-Queue`) + `agent-bridge/test_bridge.py` verdi (`65b8f0ba`).
4. **Calibrazione cieca 2C `gemini-4-neon-high` (`docs/notes/reviews/calibrazione-2c-neon-2026-11-22.md`) — COMPLETATA (`3/4` match, `0` falsi PASS):**
   - `K6 r1` (`5a821c53`): `FAIL (posa)` vs Fable `FAIL (posa)` -> **Match (1/1)**, 0 falsi PASS (`412.4 s`).
   - `K6 r2` (`84b7c354`, solo `.jpg`): `FAIL (posa)` vs Fable `FAIL (posa)` -> **Match (2/2)**, 0 falsi PASS (`418.8 s`, session `b2866026-ff4c-4521-adc7-a29b7f070178`).
   - `v27 r2` (`22300411`, solo `.jpg`): `FAIL (posa)` vs Fable `PASS` -> **Falso FAIL (2/3)**, 0 falsi PASS (`377.9 s`, session `cdd7d469-e4ca-4426-bf93-cd1f816ff248`).
   - `v28 r3` (`3c779b08`, solo `.jpg`): `FAIL (posa)` vs Fable `FAIL (posa)` -> **Match (3/4)**, 0 falsi PASS (`354.8 s`, session `c0af673f-d3d7-497e-8d99-7191204273f6`).
   - **Esito Regola 2C:** `gemini-4-neon-high` abilitato per `PASS` autonomo su `M3` (solo griglie `.jpg` + spot-check Claude su `1 PASS su 3`).

## Prossimi passi
1. Quando l'owner ha completato il pagamento DeepSeek e impostato `DEEPSEEK_API_KEY` (env User Windows), eseguire test 10 min su worktree sparse via `Tools/CI/New-DeepSeekWorktree.ps1`.
2. Usare `gemini-4-neon-high` (`M3`) solo con griglie `.jpg` (includere `clip-check/metrics.json` nelle locomotion review).

COSTO: 0 $ | 19.2 min Neon + ~10 min coord | 0 tok Claude | provider=agy_student | quota_pro=0%
