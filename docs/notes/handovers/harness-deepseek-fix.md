# Handover — harness-deepseek-fix (2026-11-23)

- **Branch:** `claude/harness-deepseek-fix`
- **Obiettivo:** risolvere timeout/diagnostica `claude.exe` + DeepSeek su `agent-bridge`, falso positivo EULA guard su negazioni, bridge sparse worktree ↔ `_ue59`, pipeline deterministica animazioni e push Git LFS automatico.
- **Modifiche `agent-bridge` (`C:\Users\jacop\Tools\agent-bridge\`):**
  - `bridge_core.py`: `--output-format stream-json --verbose` con log live in `logs/*.jsonl`; `_summarize_stream_log` su timeout; `_check_epic_eula_guard` ignora clausole di divieto esplicito; flag `disable_search_tools` (`Glob`/`Grep` + `Saved/`/`Content/`).
  - `mcp_server.py` + `cli.py`: `async_mode` + `bridge_job_status` per evitare deadline MCP 180s; `--no-search-tools`.
  - `test_bridge.py`: `ALL TESTS PASSED`.
- **Modifiche repo `GASP_Giustino2`:**
  - `Tools/CI/Run-SparseAnimStep.ps1`: esegue script `.py` sparse su `_ue59`, riporta `.uasset`/`joints_*.json`/`gates_*.json`, ripristina `_ue59`.
  - `Tools/AnimPipeline/cards/runstart/Invoke-RunStartPipeline.ps1` + `runstart_ab_video.py`: pipeline deterministica 0-token con `az180,az90,game`, `grid_pivot_v84_116.jpg` (passo 4), `_slow4x.mp4`, e `clipcheck --conserve` (`<= 15%`).
  - `.claude/skills/gstn-anim-cycle-cheap/SKILL.md`: authoring DeepSeek isolato + hard gate 0-token + review indipendente su `Gemini 4 Neon (High)` (`M3`) sulle griglie `.jpg`.
  - `Tools/AnimPipeline/gstn_anim_mcp/` (`anim_ops.py`, `geom_intent_ue.py`, `mcp_server.py`) + `Tools/AnimPipeline/tests/test_gstn_anim_mcp.py`: server MCP stdio a 5 tool semantici (`anim_inspect`, `anim_apply_intent`, `anim_gate_check`, `anim_capture_ab`, `anim_rollback`).
  - `.githooks/pre-push`: esegue `git lfs pre-push` dopo `Test-GitGuard.ps1` (reinstallato in `C:\Users\jacop\Tools\gstn-gitguard`).
- **Stato:** verificato (`test_bridge.py` PASS, `test_gstn_anim_mcp.py` 4/4 OK, `py_compile` OK, `PS_PARSE: OK`, hook reinstallati).

COSTO: 0.00 $ (diagnostica ~40k tok) | provider=agy_student | quota_pro=0%
