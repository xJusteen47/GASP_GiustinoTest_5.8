---
name: gstn-anim-cycle-cheap
description: Variante economica del ciclo animazioni (gstn-anim-reviewer + gstn-anim-executor): delega solo l'authoring .py a DeepSeek (timeout corto, senza Glob/Grep) ed esegue tutto il resto deterministicamente a 0 token, usando Opus high solo sul pacchetto compatto finale.
---

# gstn-anim-cycle-cheap

Variante a basso consumo di `gstn-anim-reviewer` + `gstn-anim-executor`. **Non modifica né sostituisce le skill originali.**

## 1. Ruoli e modelli
- **Authoring semantico via MCP `gstn-anim-mcp` (preferito per modelli `flash`/Sonnet):** 11 tool in ordine fisso (prefisso stabile per la cache dei prompt), sopra il motore UE 5.9. Risposte compatte (<= 5 righe, dettagli in file) con `compact: true`.
  1. `anim_inspect(asset, frame_range)`: altezze, angoli, clearance, stato gate (dump `joints_*.json`).
  2. `anim_apply_intent(src, dst, intent_json, dry_run, allow_geom)`: modifica per intent JSON con snapshot; `follow_through` (catene braccia/testa, massa, overshoot, ancora hit/faf) e' generico; i parametri geometrici (`arm_clr_deg`, `torso_bank_deg`, ...) sono SOLO per clip run-start (nome con `RunStart`), altrimenti rifiutati salvo `allow_geom=true` (gli assi vengono dal moto del bacino: sbagliati sulle mosse sul posto).
  3. `anim_run_intent(src, dst, intent_json)`: snapshot + intent + gate in un colpo, PASS/FAIL; registra il rapporto nel manifest.
  4. `anim_gate_check(asset, ref_asset)`: `runstart_pivot_gates` + conservazione <= 15% + `clipcheck`.
  5. `anim_capture_ab(...)`: cattura A/B del run-start (`mv_run_start_v5`, `grid_pivot_v84_116`): solo locomotion.
  6. `anim_register_variant(asset, variant_id >= 40)` / `anim_unregister_variant`: solo chooser run-start; 30-39 sono le righe Handoff delle varianti approvate 20-29.
  7. `anim_rollback(asset)`.
  8. `anim_capture_move(kit, sequences, variant_runs, ...)`: **cattura di una mossa di combattimento** (montage da abilita'), base + variante in UNA sessione editor (`pie_capture_timed.ps1 -Assert`, `PIE_CAPTURE_RUNS`), viste `az90,az180,game`, output `docs/notes/reviews/<move>/<round>/` (mp4 real + slow4x, una griglia per vista con riga base sopra e variante sotto, max 8 immagini fitte attorno ad `anchor` e `faf`). Con `dry_run=true` restituisce solo cancello e piano. **Pose da carta** (animpipe author/build/check, senza `anim_run_intent`): nella variante `"card": "Tools/AnimPipeline/cards/.../x.json"` al posto di `intent_hash`; il rapporto e' il `animpipe check` PASS piu' recente di carta e build (`card_report`). Ogni run puo' avere il suo `"kit"` (base e variante in una sessione) e il suo `"assert"`. Scrive sempre `gate_report.json` (anche se FAIL) e `alignment.json`: griglie e video allineati all'inizio del montage di ogni run. Le run con `gstn.Combat.Debug.Hitboxes 1` hanno le hitbox disegnate nelle immagini (forme loggate per frame, `capture_overlay.py`; serve un editor compilato con `GetDebugHitboxShapes`).
  9. `anim_silhouette_check(asset, frame_range, views, base_asset, out)`: silhouette per vista (az90, az180, az315, game). "Illeggibile" = **palla informe**: nessuna estremita' esce dal torso di >= 12 cm e il contorno e' tondo (circolarita' >= 0,65); la riga FINDING dà la metrica (sporgenza massima, circolarita'). Le estremita' vicine al torso sono solo elencate (un tuck non e' illeggibile per questo). PNG + `silhouette.json` in `out` (la cartella della review, da committare) o in Saved.
  10. `anim_motion_overlay(asset, frame_range, views, base_asset)`: scie di polsi/caviglie/testa/bacino con frecce di velocita' e tangenti; segnala rotture d'arco (> 60 gradi) e pop (velocita' > 3x i vicini); PNG + JSON.
  - Gli ultimi due si leggono dai dump `joints_*.json` (per le mosse di combattimento: `clip_joints_dump_ue.py`, non ancora provato in UE) e **i loro PNG vanno nel pacchetto di review** insieme alle griglie.
- **Percorso mossa di combattimento (ordine dei cancelli, tutti a 0 token, falliscono in secondi):**
  1. `anim_apply_intent ... dry_run=true` (obbligatorio: l'intent hash va nel manifest `Saved/AnimPipeline/manifest/<asset>.json`);
  2. `anim_run_intent` vero: FOLLOW_THROUGH report OK + conservazione OK + gate OK = PASS (altrimenti `anim_rollback`);
  3. rilancia lo script hitbox/bake se il `.uasset` e' cambiato (`Saved/AnimR2/*hitboxes*.json` con lo sha256 corrente);
  4. `anim_capture_move dry_run=true`: controlla soglia di leggibilita' (>= 2 cm o >= 3 gradi su mano/piede/testa nella finestra, altrimenti "troppo piccolo da leggere", niente cattura) e contatore tentativi (max 2 per obiettivo, il 3o e' rifiutato);
  5. `anim_silhouette_check` + `anim_motion_overlay` (offline, gratis);
  6. `anim_capture_move` vero: la base si riusa dalla cache se clip, sequenza, cvar, viste e build (hash di Source/ e Plugins/*/Source) sono uguali; stesso intent hash gia' catturato = nessuna nuova cattura. Build solo se `manifest.needs_build()` (hash C++ diverso dal timbro).
  Il revisore riceve SOLO le griglie (+ PNG silhouette/motion), mai gli mp4 in blocco.
- **Fallback Authoring Python (`run_deepseek` — `deepseek-v4-pro` / `deepseek-flash`):** scrive/modifica **solo** lo script `.py` di posa in `Tools/AnimPipeline/` dentro una sparse worktree (`Tools/CI/New-DeepSeekWorktree.ps1`) + `python -m py_compile`.
  - Timeout corto: `90–120s`, `disable_search_tools=true` (disabilita `Glob`/`Grep` e letture `Saved/`/`Content/` per evitare stalli su repo UE grandi).
  - Non deve lanciare `Save-Assets.ps1`, catture GPU o `ffmpeg` dentro la sessione LLM.
  - Per task >120s via MCP usare `async_mode=true` + `bridge_job_status` oppure `cli.py` direttamente.
  - Nei prompt di esclusione EULA è sicuro scrivere *"Non toccare UE_5.9 / Plugins/GASNPP"*, ma restano vietati accessi reali a path Epic.
- **Pipeline deterministica (`0` token, ~2.5 min):**
  - Eseguire `Tools\AnimPipeline\cards\runstart\Invoke-RunStartPipeline.ps1` (passando `-WorktreeDir` se si parte da una sparse worktree, che usa automaticamente `Tools\CI\Run-SparseAnimStep.ps1` per eseguire headless su `GASP_Giustino2_ue59` e riportare `.uasset`, `joints_*.json`, `gates_*.json` e griglie `.jpg`/`.mp4` nella worktree lasciando pulito `_ue59`).
  - L'ordine deterministico (hard gate bloccanti a 0 token) è: authoring UE → `runstart_clip_joints_ue.py` → `runstart_pivot_gates.py` (`GATES: OK`) → `clipcheck` + `Conservation Guard (<= 15%)` → `runstart_batch_captures.ps1` (`ASSERT: OK`) → `runstart_ab_video.py`.
  - **Viste e artifact obbligatori:** mantenere **sempre** `az180,az90,game` (**MAI** togliere `az180`: un gomito "ad ala" a 90° passa inosservato in `az90`), generare la griglia fitta sul tratto critico (`grid_pivot_v84_116.jpg`, passo 4 frame) e i video `_slow4x.mp4`.
- **Giudizio (regola 3 del 2026-11-23):** giudice **Opus high** + **critico cieco Opus high** (subagente indipendente, mai chi ha editato; [critico-estetico.md](../../../docs/notes/review/critico-estetico.md) + [rubric.md](../../../Tools/AnimPipeline/contract/rubric.md) + [principi-animazione-corporea.md](../../../docs/notes/anim-theory/principi-animazione-corporea.md)), **una sola passata** sul pacchetto compatto. **Fable solo dopo 2 fallimenti di Opus.** Gemini Neon solo se i crediti sono confermati. Prima la vita (leggibilita', silhouette), poi i tecnici.
- **I 10 controlli anti-spreco:** (1) dry run obbligatorio con stesso intent hash; (2) cancello prima della cattura (report PASS, hitbox fresche); (3) soglia di leggibilita'; (4) cache della base e dell'intent gia' catturato; (5) build solo se cambia il C++; (6) una sola sessione editor per base, variante e tutte le viste; (7) pacchetto review compatto (max 8 immagini per vista, fitte attorno ad anchor e FAF); (8) max 2 tentativi per obiettivo nel manifest; (9) costi automatici: minuti per passo e `usage` nel manifest (`manifest.record_step`, `manifest.costi_line` produce la riga di `handoffs/costi.csv`, la aggiunge il coordinatore); (10) prefisso tool stabile + risposte compatte.

## 2. Flusso in 3 step
1. **Piano e istruzioni (`COORD` / `Reviewer`):** definisce le micro-istruzioni ordinate (`trunk`, `arm_r`, `arm_l`, `legs`, `head`, `feet`) rispettando `Tools/AnimPipeline/contract/`.
2. **Authoring LLM isolato + Pipeline deterministica:**
   - DeepSeek modifica solo lo script `.py` target e verifica `py_compile`.
   - `Invoke-RunStartPipeline.ps1` esegue headless UE, gate numerici, catture GPU e video/griglie A/B a `0` token.
3. **Review finale (Opus high + critico cieco Opus high) + Push:**
   - Emettono `PASS` o `FAIL` con max 3 gap sul pacchetto compatto (griglie per vista + PNG silhouette/motion).
   - Su branch `claude/*`, `.githooks/pre-push` esegue automaticamente `git lfs pre-push` prima del push remoto.
