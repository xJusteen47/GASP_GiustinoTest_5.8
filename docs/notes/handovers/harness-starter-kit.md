# Handover harness-starter-kit (cloud, Opus medium, 2026-11-21)

**Obiettivo**: kit portabile dell'harness agentico per altri progetti (owner 21:37 "si, va bene. Conviene."; nota coordinatore).
Fonti: `claude/runstart-generic-desktop` 84cc0057 (Tools/CI, Tools/UE59, ue_builtin.py, .claude/settings.json), project files
`handoffs/` (costi.csv, INDICE.md, brief e handover reali, skills-portabilita.md, skills-split.md), regole 1-15.
**Esito**: PASS (solo file nuovi in `starter-kit/`, branch da `main`; nessun codice di gioco toccato).

Fatto:
- `starter-kit/README.md` (cosa c'è + guida in 10 passi), `RULES.md` (15 regole generiche, ambiti), `MAPPA-GENERICO-VS-GASP.md`,
  `kit.config.example.json`, `claude/` (settings + push rules), `templates/` (12 modelli).
- Script parametrizzati da `kit.config.json`: Run-Timed, Check-TestLog, Apply-PushRules, Close-Editor (+ close_editor.py),
  ask_unreal_assistant + ue_mcp (sottoinsieme di ue_builtin). Nuovi: precommit_guard + hook + install_hooks, cost_line, kit_check.

Prove:
- Guardia: repo di prova, commit con `Content/X/DA_Loco_Default.uasset` -> `GUARDIA: commit BLOCCATO` exit 1 (scratchpad kittest).
- cost_line scrive header + riga; kit_check segnala i 5 passi mancanti nel repo di prova; `python -m py_compile` OK su tutti i .py.
- `grep -rniE 'gstn|giustino|gasp_|overlord|ganon|fable|agy'` su .py/.ps1/.json/.template del kit -> 0 righe.
- NON provati (niente Windows nel cloud): Run-Timed (anche -SelfTest), Close-Editor, ask_unreal_assistant. Passo 10 della guida.

Scelte applicate (regola 6): kit in italiano come il resto del metodo; si installa come `harness/` nel repo nuovo; cattura/review
pack e capture_assert NON inclusi (troppo legati a GASP), elencati come v2; la guardia è generica e GASP può adottarla con il
suo elenco (voce in coda "guardia pre-commit"): basta un `kit.config.json` alla root con `guard.protected_paths`.

Prossimo: owner prova i 10 passi su un progetto vuoto, o il thread di turno del fisso esegue `Run-Timed.ps1 -SelfTest` dal kit.
COSTO: 3 | 0 | 0
