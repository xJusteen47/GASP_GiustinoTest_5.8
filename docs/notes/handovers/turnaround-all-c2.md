# turnaround-all-c2 (2026-11-23): 180R, 090L, 090R (2 round, ASSERT OK, Review Pro r2 FAIL)
Branch: `claude/deepseek-turn4`. Script: `Tools/AnimPipeline/cards/runstart/turnaround_all_ue.py`.

## Stato & metriche raggiunte
- **Numerica (`turnall_c3`):** `jitter max` sceso a `2.7` (`180Rs`), `4.1` (`090Ls`), `5.6` (`090Rs`) `<= 8.0`; `bone worst 0.5%`; `ASSERT: OK` su tutti e 6 i run (`180R/090L/090R`, `27` vs `28`).
- **Uscita `v122-v197`:** pulita su tutte e 3 le direzioni (`pulizia` OK in uscita).
- **Review indipendente (`Pro` cieco su griglie `.jpg` `turnall_c3`):** ancora **FAIL** nella finestra pivot `v88-v112`:
  - `180R`: braccio R effetto ala `v92-v96`; busto verticale `v88-v92` poi gobba `~55°` a `v104-v108` con risalita a `v112`; perno R tardivo `v96-v100`.
  - `090L`: braccia spalancate `v100-v104`; busto affonda `v107-v108` poi raddrizza a `v112`; braccio R pre-piegato `v77-v84` e perno L lento `v96-v104`.
  - `090R`: braccio R effetto ala `v88-v96` (`27` migliore); busto curvo `~45°` `v107-v108` poi scatto `v112`; braccio R pre-piegato `v77-v84`.

## Causa radice & prossimo passo (Regola 10 -> owner con video `slow4x`)
- Base `w2t*f` (26) manca di `startL_stock_dump.npz` su disco (impossibile rigenerare `w2t*p` base 27) e introduce la gobba `v104-v108` + pre-piegamento idle `v77-v84`.
- Inoltre `signed_axis_quat(fwd_vec, ARM_ST_TUCK_IN_DEG, ...)` ruota l'omero verso l'esterno in `w2t*f` a `v92`.
- Video A/B (`_real.mp4` + `_slow4x.mp4`) e griglie `.jpg` pronti in `GASP_Giustino2_ue59/Saved/CaptureSpeed/turnall_c3/ab_{180R,090L,090R}/` per ispezione owner.

COSTO: 0 $ | provider=agy_student | quota_pro=0%
