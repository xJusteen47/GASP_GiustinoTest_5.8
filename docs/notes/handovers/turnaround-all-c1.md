# turnaround-all-c1 (2026-11-23): 180R, 090L, 090R (GATES OK, ASSERT OK, Neon High FAIL r1)
Branch: `claude/deepseek-turn4`. Script: `Tools/AnimPipeline/cards/runstart/turnaround_all_ue.py`.
Nota: `180R/090L/090R` partono da `w2t*f` (base 26) perche `startL_stock_dump.npz` non era su disco dopo la pulizia `Saved/`.

## Esiti per direzione (cattura `27` vs `28` + review `gemini-4-neon-high`)
- **180R (`mv_turn_180b`):** `GATES: OK` (`gap 14.7`, `jit 4.6`), `ASSERT: OK`, `Neon High FAIL` — (1) accovacciamento `v96-108` prima di `v112-116`, (2) braccio R rigido `v92-96` + spalla R bassa `v96-104`, (3) raddrizzamento a `v122-137`.
- **090L (`mv_turn_090a`):** `GATES: OK` (`gap 13.6`, `jit 6.2`), `ASSERT: OK`, `Neon High FAIL` — (1) tilt laterale esterno a `v88-96`, (2) snap `v84->v88`, (3) busto chiuso a coltello `v88-100` (uscita `v104-197` promossa).
- **090R (`mv_turn_090b`):** `GATES: OK` (`gap 15.2`, `jit 4.6`), `ASSERT: OK`, `Neon High FAIL` — (1) flip-flop gambe/braccia `v96->100->104`, (2) stallo `v104-108` + salto a `v112`, (3) braccio R a bandiera `v88-92` (uscita `v122-197` promossa).

## Prossimo passo
- Rigenerare i base `27` (`w2t180Rp`, `w2t090Lp`, `w2t090Rp`) oppure applicare il nuovo `gstn-anim-mcp` (`harness-anim-mcp-2026-11-23.md`) con parametri distinti per 90° vs 180°.

COSTO: 0 $ (batch UE/catture + 3 review agy_student neon-high) | provider=agy_student | quota_pro=0%
