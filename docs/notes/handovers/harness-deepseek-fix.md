# Handover — harness-deepseek-fix (2026-11-23)

- **Branch:** `claude/harness-deepseek-fix`
- **Obiettivo:** risolvere timeout/diagnostica `claude.exe` + DeepSeek su `agent-bridge`, falso positivo EULA guard su negazioni, bridge sparse worktree ↔ `_ue59`, pipeline deterministica animazioni e push Git LFS automatico.
- **Modifiche `agent-bridge` (`C:\Users\jacop\Tools\agent-bridge\`):**
  - `bridge_core.py`: `--output-format stream-json --verbose` con log live in `logs/*.jsonl`; `_summarize_stream_log` su timeout; `_check_epic_eula_guard` ignora clausole di divieto esplicito; flag `disable_search_tools` (`Glob`/`Grep` + `Saved/`/`Content/`).
  - `mcp_server.py` + `cli.py`: `async_mode` + `bridge_job_status` per evitare deadline MCP 180s; `--no-search-tools`.
  - `test_bridge.py`: `ALL TESTS PASSED`.
- **Modifiche repo `GASP_Giustino2`:**
  - `Tools/CI/Run-SparseAnimStep.ps1`: esegue script `.py` sparse su `_ue59`, riporta `.uasset`/`joints_*.json`/`gates_*.json`, ripristina `_ue59`.
  - `Tools/AnimPipeline/cards/runstart/Invoke-RunStartPipeline.ps1`: pipeline deterministica 0-token (authoring → joints → gates → GPU capture → A/B video/jpg).
  - `.claude/skills/gstn-anim-cycle-cheap/SKILL.md`: authoring DeepSeek isolato + pipeline deterministica.
  - `.githooks/pre-push`: esegue `git lfs pre-push` dopo `Test-GitGuard.ps1` (reinstallato in `C:\Users\jacop\Tools\gstn-gitguard`).
- **Stato:** verificato (`test_bridge.py` PASS, PS parse 0 errori, hook reinstallati).

COSTO: 0.00 $ (diagnostica ~40k tok) | provider=agy_student | quota_pro=0%
