# GASP GiustinoTest 5.8 — Regole Workspace (Antigravity)

- **Lingua e stile:** rispondi a Giustino (`xJusteen47`) in **italiano**, sintetico, con passi manuali numerati quando serve azione owner.

## Memoria indicizzata su git (leggi SOLO ciò che serve al task)
1. `handoffs/INDICE.md` — indice thread, branch, commit, esiti, task attivi.
2. Handover del task attivo in `docs/notes/handovers/` (max 30 righe).
3. `docs/notes/memoria-progetto.md` — stato progetto, hardware, decisioni, coda aperta.
4. `docs/notes/regole-progetto-2026-11-22.md` — regole attive (Regole 1–15 + routing M0–M3).
5. `handoffs/queue.json` e `handoffs/costi.csv` — coda deterministica e registro costi.
- Mai rileggere vecchie chat/log completi se non espressamente richiesto dal brief.

## Guardrail rigidi
- **MAI** modificare file sporchi dell'owner in `C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59` (`DA_Locomotion_Default.uasset`, `CHT_*_PoseMatch.uasset`, `_ue59/IN-USO.txt`, `Mixamo/`, ecc.).
- **MAI** push su `main` / `feature/*`, force-push o commit di segreti/chiavi.
- **Confine EULA Epic:** `Plugins/EngineOverrides/`, `Plugins/GASNPP/`, `UE_5.9/` non vanno **MAI** a DeepSeek (usa worktree sparse con `Tools/CI/New-DeepSeekWorktree.ps1`).
- **Disciplina modelli/quota:** coordina le chat su **`Hydrogen High`** (mai chat lunghe su `Neon XHigh`); usa `gemini-4-neon-high` (`M3`) solo per review animazioni passando **griglie `.jpg`** (niente `.mp4` in blocco: `read_file` li campiona a 1 fps).

## Chiusura obbligatoria di fine task (prima di rispondere "fatto")
Aggiorna `docs/notes/handovers/<task>.md` (<= 30 righe, riga finale `COSTO: ... | provider=... | quota_pro=...`), aggiungi 1 riga a `handoffs/INDICE.md`, registra le righe in `handoffs/costi.csv`, committa e pusha su `claude/*`.
