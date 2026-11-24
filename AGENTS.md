# GASP GiustinoTest 5.8 — Regole Workspace (Antigravity)

Rispondi a Giustino (`xJusteen47`) in **italiano**, sintetico, con passi manuali numerati quando serve azione owner.
Questo file non copia le regole: dice cosa leggere, in quest'ordine, e solo ciò che serve al task.

1. `docs/notes/regole-progetto.md` — le regole del progetto in una pagina (fonte unica; se qui e altrove differiscono, vale questo file). Dettagli: `docs/notes/regole-processo.md` (costi, handover, macchine) e, per le animazioni, `docs/notes/regole-animazione.md`.
2. `CLAUDE.md` — mappa del codice e convenzioni.
3. `docs/INDICE-PROGETTO.md` — cosa esiste già e va riusato (prima di scrivere codice, regola 1).
4. `Tools/AnimPipeline/registro-clip.csv` — stato di ogni clip (approvata, bloccata, in review). Mai scriverlo a mano: lo scrivono solo `animpipe publish` e `animpipe approve`.
5. Il brief del tuo task (`handoffs/`) e, se ripari un lavoro, il suo handover in `docs/notes/handovers/`.

Mai rileggere vecchie chat o log interi se il brief non lo chiede.

## Guardrail rigidi
- **Tronco = `main`.** Se il tuo branch non parte dall'integrazione del giorno (`claude/integ-<data>`) o da `main`, fermati e scrivilo nell'handover.
- **MAI** modificare file sporchi dell'owner in `C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59` (`DA_Locomotion_Default.uasset`, `CHT_*_PoseMatch.uasset`, `_ue59/IN-USO.txt`, `Mixamo/`, ecc.).
- **MAI** push su `main` / `feature/*`, force-push, cancellazione o spostamento di tag, commit di segreti/chiavi. Si pusha solo su `claude/*`; i tag `approved/*` li crea solo `animpipe approve`.
- **Passi UE di animazione** (import, dump, cattura): solo su `_ue59` del fisso, tramite `Tools/CI/Run-Timed.ps1` (che fa il PreCheck e chiude l'editor su `ESITO:`). Mai da un worktree senza lo scheletro.
- **Confine EULA Epic:** `Plugins/EngineOverrides/`, `Plugins/GASNPP/`, `UE_5.9/` non vanno mai a DeepSeek (usa worktree sparse con `Tools/CI/New-DeepSeekWorktree.ps1`). `DEEPSEEK_API_KEY` solo nella variabile d'ambiente utente di Windows, mai nel repo né in chat. Un modello locale non gira mentre `UnrealEditor*` è attivo (il PreCheck di `Run-Timed.ps1` rifiuta le catture se un LLM locale occupa la GPU).
- **Skill:** vivono in `.claude/skills/` nel repo; la cartella `~/.gemini/config/skills/` contiene solo junction verso di lì (`Tools/CI/Sync-AgentSkills.ps1`). Non copiare skill altrove.
- Review di animazioni: passa griglie `.jpg` al modello di review, non `.mp4` in blocco (`read_file` li campiona a 1 fps).

## Chiusura obbligatoria di fine task (prima di rispondere "fatto")
Scrivi `docs/notes/handovers/<task>.md` (<= 30 righe) con le righe finali `ESITO: <PASS|PARZIALE|FAIL> <una frase>`, `SUBAGENTI: <quanti, per cosa, modello>` e `COSTO: <$ stimati> | <minuti desktop> | <token review>`. **Non** toccare `handoffs/INDICE.md` né `handoffs/costi.csv`: li genera `Tools/CI/Build-Indice.py` dagli handover. Committa e pusha su `claude/*`.
