# Cosa è finito nel kit e cosa resta a GASP (2026-11-21)

| Origine (repo GASP o project files) | Nel kit | Cosa è cambiato | Resta solo a GASP |
|---|---|---|---|
| Istruzioni del progetto, 15 regole | `RULES.md` | nomi tolti; modelli come ruoli (economico/medio/forte/punta); regole 10/13/14/15 generalizzate con AMBITO | "Fable", "agy", Cascadeur, gasp-divergenze, regola baricentro FTilt, DA_Locomotion_Default |
| `Tools/CI/Run-Timed.ps1` | `scripts/windows/Run-Timed.ps1` | progetto, target, filtro test, motore e limiti da `kit.config.json`; Job class rinominata | default `GSTN` e `GASP_Giustino2Editor` |
| `Tools/CI/Check-TestLog.ps1` | idem | nessuno | |
| `Tools/CI/push-rules.json`, `Apply-PushRules.ps1` | `claude/`, `scripts/windows/` | percorsi | |
| `.claude/settings.json` | `claude/settings.template.json` | `Tools/*` -> `harness/scripts/*` | |
| `Tools/UE59/Close-OwnerEditor.ps1`, `close_owner_editor.py` | `Close-Editor.ps1`, `close_editor.py` | usa `ue_mcp.py`; root = repo che contiene `harness/` | |
| `Tools/UE59/ask_unreal_assistant.py` | idem | contesto da `engine.assistant_context`; output in `Saved/Assistant/` | contesto "Mover + NP 62.5 Hz, GAS fork" |
| `Tools/AnimPipeline/ue_builtin.py` | `scripts/python/ue_mcp.py` | solo `available()` ed `execute()`; URL dal config | `sample_pose`, `read_anim_sequence`, `import_animation` (pipeline animazioni) |
| guardia pre-commit (voce in coda, mai costruita) | `precommit_guard.py` + hook | nuova: elenco da `guard.protected_paths` | l'elenco di GASP va nel suo `kit.config.json` |
| `handoffs/costi.csv`, `INDICE.md`, brief/handover esistenti | `templates/` | struttura estratta dagli esempi reali | contenuti |
| `skills-public/` (repo `game-anim-vfx-review-skills`) | rimando (passo 9) | nessuno: è già generico | `.claude/skills/gstn-*` |
| `Tools/AnimPipeline/*`, `Tools/GASNPPTest/*` (catture, scenari, review pack, `capture_assert.py`) | no | troppo legati a scheletro, card e livelli di test | tutto; nel kit restano solo l'aggancio `-AssertFile` di Run-Timed e il principio `ASSERT: OK` |
| `Tools/UE59/cr_deprecated_59.json`, `ue59_editor_checks.py` | no | controlli di migrazione di questo progetto | |

Candidati per una versione 2 del kit (non fatti, servono prima sul gioco): cattura video GPU in tempo reale a 4 viste,
branch di integrazione automatico con build serale, `capture_assert.py` reso generico (clip attesa + finestra da JSON).
