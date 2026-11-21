# Starter kit dell'harness agentico

Il metodo di lavoro nato su GASP GiustinoTest (coordinatore + thread a checkpoint, brief/handover, costi, guardie,
script a tempo, review visiva indipendente) staccato dal gioco e reso portabile. Si copia in un repo nuovo come cartella
`harness/`, si compila **un** file di configurazione e si incollano le regole nelle istruzioni del progetto.

## Cosa c'è

| Percorso | Cosa | Generico? |
|---|---|---|
| `RULES.md` | Le 15 regole di lavoro e di spesa, senza nomi del progetto; da incollare nelle istruzioni del progetto | sì (regole 10, 13, 14 hanno un ambito: tienile se servono) |
| `kit.config.example.json` | Nome progetto, motore, percorsi, limiti di tempo, budget, file protetti | da compilare |
| `templates/` | brief, handover, `costi.csv`, `INDICE.md`, verdetto di review, messaggio decisioni all'owner, messaggio di inizio sessione, divergenze, versioni approvate, lock `IN-USO.txt`, indice memoria, `CLAUDE.md` | sì |
| `claude/settings.template.json` | Permessi Claude Code: push solo su `claude/*`, niente force/delete/reset/clean, script del kit senza prompt | sì |
| `claude/push-rules.json` + `scripts/windows/Apply-PushRules.ps1` | Le stesse regole di push nelle impostazioni UTENTE (valgono in ogni cartella e worktree) | sì |
| `scripts/windows/Run-Timed.ps1` | Build/test/cattura con tempo massimo, Job Object che chiude tutti i figli, `ESITO:` in max 5 righe, niente dialoghi bloccanti, ini PIE ripristinato | sì per Unreal; `-Exe` per qualunque programma |
| `scripts/windows/Check-TestLog.ps1` | PASS/FAIL leggendo il log di Automation (non si fida dell'exit code) | Unreal |
| `scripts/windows/Close-Editor.ps1` + `python/close_editor.py` | Chiude l'editor dell'owner solo se non è in Play e dopo aver salvato tutto; mai editor di agenti | Unreal 5.9+ |
| `scripts/python/ask_unreal_assistant.py` + `ue_mcp.py` | Domande all'Unreal Assistant dall'editor aperto, via MCP integrato; contesto dal config | Unreal 5.9+ |
| `scripts/python/precommit_guard.py` + `scripts/git/pre-commit` + `install_hooks.py` | Blocca commit di file protetti (asset locali dell'owner, segreti, file enormi) | sì |
| `scripts/python/cost_line.py` | Riga in `costi.csv` + riga `COSTO:` per l'handover | sì |
| `scripts/python/kit_check.py` | Controlla che il kit sia attaccato bene (passo 10) | sì |

Cosa **non** c'è, perché specifico di GASP: pipeline animazioni (`Tools/AnimPipeline`, card, Cascadeur), scenari PIE e
catture del gioco (`Tools/GASNPPTest`), cvar `gstn.*`, regole su Mover/GAS/NP, stile "Ganondorf in Astro Bot", decisioni
di design. Dettaglio in `MAPPA-GENERICO-VS-GASP.md`. Le skill di review anim/VFX generiche sono già un repo a parte:
`github.com/xJusteen47/game-anim-vfx-review-skills` (sorgente: `skills-public/` nel repo del gioco).

## Attaccarlo a un progetto nuovo in 10 passi

1. **Copia la cartella.** Nel repo nuovo: copia `starter-kit/` come `harness/` (stessa struttura, nessun file fuori).
   Su Windows: `xcopy /E /I <repo GASP>\starter-kit <repo nuovo>\harness`.
2. **Config.** Copia `harness/kit.config.example.json` nella ROOT del repo come `kit.config.json` e compila:
   `project_name`, `owner_name`, `engine.uproject`, `engine.editor_target`, `engine.test_filter`, `engine.dir`,
   `engine.assistant_context` (2 righe sul setup del progetto). Progetto non Unreal: metti `"type": "other"` e ignora
   `engine.*`.
3. **File protetti.** In `guard.protected_paths` elenca i file che solo l'owner tocca in locale (asset di tuning,
   `.env`, chiavi). Poi in ogni clone: `python harness/scripts/python/install_hooks.py`.
4. **CLAUDE.md.** Copia `harness/templates/CLAUDE.md.template` come `CLAUDE.md` nella root e compila mappa, convenzioni
   e invarianti del codice (max ~70 righe: la mappa, non il manuale).
5. **Permessi.** Copia `harness/claude/settings.template.json` come `.claude/settings.json` (unisci a quello esistente se
   c'è). L'OWNER, a mano, una volta per PC: `powershell -NoProfile -ExecutionPolicy Bypass -File harness\scripts\windows\Apply-PushRules.ps1`.
6. **Cartelle di lavoro.** Crea `docs/notes/handovers/`, `docs/notes/reviews/`, e copia `templates/divergenze.md` e
   `templates/approvate.md` nei percorsi di `paths.*`. Crea `handoffs/` (nei Claude Projects: la cartella condivisa
   del progetto) con `costi.csv`, `INDICE.md`, `IN-USO.txt` dai template; se `handoffs/` non è nel repo, metti in
   `paths.costs_csv` il percorso assoluto.
7. **Regole.** Apri `harness/RULES.md`, sostituisci i `<...>`, togli le regole d'ambito che non servono (es. 10 e 13 se
   non ci sono contenuti da giudicare a occhio) e incolla il blocco nelle **istruzioni del progetto**.
8. **Memoria e macchine.** Inizializza l'indice della memoria del progetto da `templates/memory-index.md` (lingua,
   owner, macchine con i loro id ambiente, regole git). Collega i PC che faranno build/editor e annota quale è "fisso".
9. **Skill.** Se servono review di animazioni o VFX: aggiungi il repo `game-anim-vfx-review-skills` e crea, accanto,
   uno strato di progetto `project.md` per skill (il nucleo non si modifica mai). Le altre skill del progetto nascono
   in `.claude/skills/` del repo nuovo.
10. **Verifica.** `python harness/scripts/python/kit_check.py` deve stampare `ESITO: OK`. Poi una prova a vuoto sul PC:
    `powershell -File harness\scripts\windows\Run-Timed.ps1 -SelfTest` (non serve il motore) e, con l'editor aperto,
    `python harness/scripts/python/ask_unreal_assistant.py "ping"`. Primo thread vero: scrivi un brief da
    `templates/brief.md` e lascialo al coordinatore.

## Come si aggiorna
Il kit vive in `starter-kit/` del repo d'origine; i progetti ne hanno una copia in `harness/`. Una miglioria generica si
fa nel kit e si ricopia; ciò che è del singolo progetto va in `kit.config.json`, `CLAUDE.md` o nei `project.md` delle
skill, mai nei file del kit.
