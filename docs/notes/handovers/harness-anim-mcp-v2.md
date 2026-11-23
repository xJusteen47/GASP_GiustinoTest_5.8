# Handoff harness-anim-mcp-v2 (2026-11-23): upgrade da turnaround-all-c2
Origine: 2 round `turnall_c2/c3` su `claude/deepseek-turn4` (`5cf1604e`, `0 $`, Hydrogen Medium + Pro review).

## Come sono andati i tool MCP v1 (osservato sul campo)
- `deepseek-flash` non è stato testato in questo giro; `anim_inspect` ha aiutato a leggere `hip/lean/bank`, ma per i turnaround abbiamo dovuto usare il fallback `turnaround_all_ue.py` + `runstart_batch_captures.ps1` (6 run in 1 PIE = 217s).

## 5 miglioramenti prioritari per `gstn-anim-mcp` v2 + `gstn-anim-cycle-cheap`
1. **Motore geometrico `geom_intent_ue.py`:** sostituire i delta Euler locali con l'engine di `turnaround_180l_ue.py` (`signed_axis_quat` su `fwd_vec/lat_vec/up_vec` + `solve_2bone_ik` gambe + `st/pv` asimmetrici per braccia/clavicole/gomiti + `start_from_frame` per non sporcare l'idle `v77-84`).
2. **`anim_inspect` pre-GPU (risparmia 217s a round):** aggiungere `lean_local_deg` (nel frame `fwd_vec` del personaggio, non world `+Z`), `upperarm_abduction_deg` (rileva *effetto ala* a `0` GPU!), `elbow_flex_deg`, `jitter_max` e split `f0-15 / f16-35`.
3. **Fix `Run-SparseAnimStep.ps1`:** supportare `-Assets` multipli (`"A,B,C"`) nella copia `.uasset` da `_ue59` alla worktree (oggi ha copiato `0 uasset` con lista virgola).
4. **`anim_capture_ab` multi-coppia:** accettare `pairs=[{a,b,seq}, ...]` per catturare `180R+090L+090R` in **1 sola sessione PIE** invece di 3 boot (~200s risparmiati a round).
5. **Rigenerazione `startL_stock_dump.npz`:** aggiungere script/step per estrarre `startL_stock_dump.npz` da UE così `runstart_pivot_all.py` può rigenerare i base 27 (`w2t*p`) senza dover partire dai `w2t*f` (base 26).

COSTO: 0 $ | provider=agy_student | quota_pro=0%
