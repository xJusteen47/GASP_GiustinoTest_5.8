---
name: gstn-anim-cycle-cheap
description: Variante economica del ciclo animazioni (gstn-anim-reviewer + gstn-anim-executor): delega modifiche fisiche, gate e catture a DeepSeek (zero quota Antigravity) e usa il modello capace (Neon/Pro) SOLO sulle griglie .jpg finali di inizio/fine ciclo.
---

# gstn-anim-cycle-cheap

Variante a basso consumo di `gstn-anim-reviewer` + `gstn-anim-executor`. **Non modifica né sostituisce le skill originali.**

## 1. Ruoli e modelli
- **Esecutore fisico (`run_deepseek` — `deepseek-v4-pro` / `deepseek-flash`):** applica le modifiche in `Tools/AnimPipeline/` (mai su path EULA Epic: `UE_5.9/`, `Plugins/EngineOverrides/`, `Plugins/GASNPP/`), esegue `PreCheck` (`GATES: OK`), importa in UE 5.9 headless e lancia la cattura Play A/B tramite `Tools/CI/Run-Timed.ps1` (`ASSERT: OK`).
- **Verifica intermedia (`deepseek-flash` o `flash`):** controlla i gate numerici (`esito.json`) tra un micro-passo e l'altro senza chiamare modelli `Pro`/`Neon` su `before_after.png` intermedi.
- **Giudizio estetico finale (`Neon High` / `Pro`):** **una sola passata** di **Critico Estetico cieco** ([critico-estetico.md](../../../docs/notes/review/critico-estetico.md)) + **Reviewer** ([rubric.md](../../../Tools/AnimPipeline/contract/rubric.md) + [principi-animazione-corporea.md](../../../docs/notes/anim-theory/principi-animazione-corporea.md)) sulle sole **2 griglie `.jpg`** della cattura finale (`fine/grid_ab_az90.jpg`, `fine/grid_ab_game.jpg`).

## 2. Flusso in 3 step (max ~80k token totali di giudizio)
1. **Piano e istruzioni (`COORD` / `Reviewer`):** definisce le micro-istruzioni ordinate (`trunk`, `arm_r`, `arm_l`, `legs`, `head`, `feet`) rispettando il contratto (`Tools/AnimPipeline/contract/`).
2. **Esecuzione batch su DeepSeek + porzione deterministica (`0` token):** l'esecutore DeepSeek applica le istruzioni in sequenza sullo script sorgente, poi esegue **deterministicamente** `runstart_pivot_gates.py` (incl. check opposizione di fase braccio–gamba su clip di partenza/additive), `clipcheck`, import headless UE 5.9 (`Save-Assets`), cattura GPU Play A/B (`runstart_batch_captures.ps1` / `Run-Timed.ps1`), estrazione griglie `.jpg` e `capture_assert.py` (`ASSERT: OK`).
3. **Review finale (`Pro`/`Neon`):** Critico Estetico cieco + Reviewer valutano le 2 griglie `.jpg` finali secondo l'asse `vita` (stella polare *"Ganondorf dentro Astro Bot"*), `peso`, `tempo`, `root`, `aggancio`, `pulizia` e emettono `PASS` o `FAIL` con max 3 gap.
