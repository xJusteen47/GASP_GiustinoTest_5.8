---
name: gstn-anim-cycle-cheap
description: Variante economica del ciclo animazioni (gstn-anim-reviewer + gstn-anim-executor): delega solo l'authoring .py a DeepSeek (timeout corto, senza Glob/Grep) ed esegue tutto il resto deterministicamente a 0 token, usando Neon/Pro solo sulle 2 griglie .jpg finali.
---

# gstn-anim-cycle-cheap

Variante a basso consumo di `gstn-anim-reviewer` + `gstn-anim-executor`. **Non modifica né sostituisce le skill originali.**

## 1. Ruoli e modelli
- **Authoring Python (`run_deepseek` — `deepseek-v4-pro` / `deepseek-flash`):** scrive/modifica **solo** lo script `.py` di posa in `Tools/AnimPipeline/` dentro una sparse worktree (`Tools/CI/New-DeepSeekWorktree.ps1`) + `python -m py_compile`.
  - Timeout corto: `90–120s`, `disable_search_tools=true` (disabilita `Glob`/`Grep` e letture `Saved/`/`Content/` per evitare stalli su repo UE grandi).
  - Non deve lanciare `Save-Assets.ps1`, catture GPU o `ffmpeg` dentro la sessione LLM.
  - Per task >120s via MCP usare `async_mode=true` + `bridge_job_status` oppure `cli.py` direttamente.
  - Nei prompt di esclusione EULA è sicuro scrivere *"Non toccare UE_5.9 / Plugins/GASNPP"*, ma restano vietati accessi reali a path Epic.
- **Pipeline deterministica (`0` token, ~2.5 min):**
  - Eseguire `Tools\AnimPipeline\cards\runstart\Invoke-RunStartPipeline.ps1` (passando `-WorktreeDir` se si parte da una sparse worktree, che usa automaticamente `Tools\CI\Run-SparseAnimStep.ps1` per eseguire headless su `GASP_Giustino2_ue59` e riportare `.uasset`, `joints_*.json`, `gates_*.json` e griglie `.jpg`/`.mp4` nella worktree lasciando pulito `_ue59`).
  - L'ordine deterministico è: authoring UE → `runstart_clip_joints_ue.py` → `runstart_pivot_gates.py` (`GATES: OK` bloccante prima della GPU) → `runstart_batch_captures.ps1` (`ASSERT: OK`) → `runstart_ab_video.py`.
- **Giudizio estetico finale (`Neon High` / `Pro`):** **una sola passata** di **Critico Estetico cieco** ([critico-estetico.md](../../../docs/notes/review/critico-estetico.md)) + **Reviewer** ([rubric.md](../../../Tools/AnimPipeline/contract/rubric.md) + [principi-animazione-corporea.md](../../../docs/notes/anim-theory/principi-animazione-corporea.md)) sulle sole **2 griglie `.jpg`** della cattura finale (`grid_ab_az90.jpg`, `grid_ab_game.jpg`).

## 2. Flusso in 3 step
1. **Piano e istruzioni (`COORD` / `Reviewer`):** definisce le micro-istruzioni ordinate (`trunk`, `arm_r`, `arm_l`, `legs`, `head`, `feet`) rispettando `Tools/AnimPipeline/contract/`.
2. **Authoring LLM isolato + Pipeline deterministica:**
   - DeepSeek modifica solo lo script `.py` target e verifica `py_compile`.
   - `Invoke-RunStartPipeline.ps1` esegue headless UE, gate numerici, catture GPU e video/griglie A/B a `0` token.
3. **Review finale (`Pro`/`Neon`) + Push:**
   - Critico Estetico cieco + Reviewer emettono `PASS` o `FAIL` con max 3 gap sulle 2 griglie `.jpg`.
   - Su branch `claude/*`, `.githooks/pre-push` esegue automaticamente `git lfs pre-push` prima del push remoto.
