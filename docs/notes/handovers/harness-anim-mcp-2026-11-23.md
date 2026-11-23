# Handoff Harness, Skill & Anim-MCP (2026-11-23)
Destinatario: thread di fix harness / pipeline multi-agente / authoring tool.
Origine: sessione `turnaround-180l-c1..c3` + `180R/090L/090R` su `deepseek_turn4` (`claude/deepseek-turn4`).

## 1. Bug e colli di bottiglia osservati su `agent-bridge` / DeepSeek
1. **`claude.exe` + `deepseek-v4-pro[1m]` in timeout sui tool call:** senza tool risponde in `1.3–2.9s` (`$0.103`); con tool call su `deepseek_turn4` è andato in timeout su 4 run consecutivi (`600s`, `480s`, `240s`, `180s`) con 0 file scritti e `session_id: null` / `cost_usd: null` in `usage.jsonl`.
2. **Manca logging live in `C:\Users\jacop\Tools\agent-bridge\bridge_core.py`:** usa `--output-format json` bufferizzato; al timeout si perde ogni traccia dell'ultimo tool call. Passare a `--output-format stream-json` scrivendo gli eventi su file per sessione + flag per disabilitare `Glob`/`Grep` su repo UE grandi (`Saved/`, `Content/`).
3. **Timeout HTTP MCP 3 min (`call_mcp_tool`):** `run_deepseek` via MCP va in `context deadline exceeded` a `180s` lasciando `claude.exe` orfano. Renderlo asincrono (`job_id`) o usare `cli.py` in background.
4. **Falso positivo EULA guard (`bridge_core.py:791`):** substring match su `ue_5.9` blocca anche prompt che vietano di toccarla.
5. **Sparse worktree vs `Save-Assets.ps1` + Git LFS:** `UnrealEditor-Cmd.exe` crasha con `exit=3` in `deepseek_turn4` (`missing AbilitySystemSimulation`). Serve un helper `Tools/CI/Run-InUe59.ps1` che esegua su `GASP_Giustino2_ue59`, riporti `.uasset` + `joints_*.json` nella sparse worktree e faccia `git lfs push` prima di `git push` (evita `GH008`).

## 2. Fix obbligatori alla skill (`gstn-anim-cycle-cheap` -> unificata con `gstn-anim-reviewer`)
- **MAI** togliere `az180` (senza `az180` un gomito "ad ala" a 90° passa inosservato in `az90`) né la griglia fitta sul tratto critico (es. `grid_pivot_v84_116.jpg` ogni 4 frame) né i video `slow4x`.
- **MAI** far valutare chi ha editato lo script: review finale sempre su agente separato (`gemini-4-neon-high`).
- **Hard gate deterministici (0 token):** `Run-InUe59` -> `runstart_pivot_gates.py` (`GATES: OK`) -> `clipcheck` -> `Conservation Guard (<= 15%)` -> `runstart_batch_captures.ps1` (`ASSERT: OK`) -> `runstart_ab_video.py`.

## 3. Proposta Tool / MCP dedicato: `gstn-anim-mcp` (per far lavorare modelli `flash` a $0 errori)
Obiettivo: eliminare lo scripting libero di quaternioni/IK da parte dell'LLM, esponendo 5 tool MCP ad alto livello su UE 5.9 headless + Python:
1. `anim_inspect(asset, frame_range)` -> restituisce metriche leggibili da `flash`: altezze `hip/foot/elbow/shoulder`, angoli busto (`lean`, `bank`), clearance e stato `GATES`/`clipcheck`.
2. `anim_apply_intent(src, dst, intent_json)` -> applica macro-intenti semantici (es. `com_shift_fwd_cm`, `torso_lean_pivot_deg`, `torso_bank_deg`, `shoulder_level_compensate`, `arm_back_arc`, `foot_pushoff_deg`, `w3_knee_drop`, `handoff_smooth_range`) usando un **unico motore geometrico testato** (assi ancorati a `pelvis/spine` + 2-bone IK + finger/length lock + smoothing automatico), così anche `deepseek-flash` / `inkling-2-mini` edita solo 8–10 numeri JSON senza scrivere codice.
3. `anim_gate_check(asset, ref_asset)` -> esegue `runstart_pivot_gates` + `clipcheck` + `conservation_guard (<=15%)` e ritorna JSON compatto `OK/FAIL`.
4. `anim_capture_ab(variant_a, variant_b, seq, window)` -> lancia `runstart_batch_captures.ps1` + `runstart_ab_video.py` + `grid_pivot` in un comando solo (`ASSERT: OK` + path griglie).
5. `anim_rollback(asset)` -> ripristina lo snapshot base se gate o review falliscono.

COSTO: 0 $ | provider=agy_student | quota_pro=0%
