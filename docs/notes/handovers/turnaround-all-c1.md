# turnaround-all-c1 (2026-11-23): 180R, 090L, 090R (GATES OK, ASSERT OK, Neon High FAIL r1)
Branch: `claude/deepseek-turn4`. Script: `Tools/AnimPipeline/cards/runstart/turnaround_all_ue.py`.

## Target artistico & fisico obbligatorio (versione turnaround della 29 w3, Link-OoT)
- **Stile:** personaggio agile platform stile **Link di Ocarina of Time** (NON Ganon pesante), identico al feeling della **`29 w3`** (`AS_GSTN_RunStart_Generic_w3`, già approvata).
- **Caratteristiche della `29 w3` da replicare nei turnaround (`180L c3` già PASS; restano `180R`, `090L`, `090R`):**
  1. **Caduta in avanti + recupero del peso:** corpo inclinato in avanti che cade già dal pivot e passi rapidi/frenetici che rincorrono il baricentro sbilanciato (mai accovacciarsi sul posto come la `27`).
  2. **Unico arco continuo `f0–f27+`:** nessun taglio tra pivot e corsa né raddrizzamento brusco a `v122–137`.
  3. **Spalle pari, alte e reattive:** banking leggero compensato dalla clavicola così nessuna spalla crolla verso il basso; mani chiuse a pugno morbido.
  4. **Braccio del lato di girata indietro oltre il piano del busto** senza effetto "ala" a `v92`, piede perno reattivo sull'avampiede (no "A" larga e piatta).

## Esiti per direzione (cattura `27` vs `28` + review `gemini-4-neon-high`)
- Nota: `180R/090L/090R` qui partivano da `w2t*f` (base 26) perche `startL_stock_dump.npz` mancava su disco dopo pulizia `Saved/`.
- **180R (`mv_turn_180b`):** `GATES: OK` (`gap 14.7`, `jit 4.6`), `ASSERT: OK`, `Neon High FAIL` (accovacciamento `v96-108`, braccio R `v92-96`, stacco `v122-137`).
- **090L (`mv_turn_090a`):** `GATES: OK` (`gap 13.6`, `jit 6.2`), `ASSERT: OK`, `Neon High FAIL` (tilt esterno `v88-96`, snap `v84->v88`, chiusura a coltello `v88-100`).
- **090R (`mv_turn_090b`):** `GATES: OK` (`gap 15.2`, `jit 4.6`), `ASSERT: OK`, `Neon High FAIL` (flip-flop `v96->100->104`, stallo `v104-108`, braccio R `v88-92`).

## Prossimo passo
- Rigenerare i base `27` (`w2t180Rp`, `w2t090Lp`, `w2t090Rp`) + applicare la ricetta `29 w3` / `gstn-anim-mcp` (`harness-anim-mcp-2026-11-23.md`) con parametri distinti per 90° vs 180°.

COSTO: 0 $ | provider=agy_student | quota_pro=0%
