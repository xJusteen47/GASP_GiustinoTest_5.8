# anim-mcp cheap actions 1 (cloud, Sonnet medium) — branch claude/anim-mcp-v2-turn4

Fatto (offline, nessun UE):
- Revert 0d950c70: tolti i 2 file spuri di 4842bacf (turnaround_all_ue.py ripristinato, runstart_build_base27_ue.py rimosso). Nessun force push.
- (1) Risposte compatte: `gstn_anim_mcp/compact.py`, max 5 righe (verdetto, fatti chiave, `details: <path>`); JSON completo in `Saved/AnimPipeline/mcp_out/`.
- (2) Prefisso stabile: ordine tool fisso (nuovo tool in 3a posizione, testo costante, nessun dato variabile), test di identità byte-per-byte.
- (3) `dry_run` su anim_apply_intent: ora SENZA effetti (niente snapshot, niente file) e restituisce `plan`: per parametro ossa, frame attivi e di picco, gradi raggiunti dopo l'inviluppo (usa `compute_envelopes` del motore, non può divergere).
- (4) `anim_run_intent` (snapshot + intent + gate in una chiamata, PASS/FAIL, su FAIL suggerisce anim_rollback; non fa rollback da solo).
- (5) `cache_log.py`: `record_usage(usage, tag)` / CLI `record|summary`, scrive `Saved/AnimPipeline/cache_usage.jsonl` con hit/miss/ratio; senza campi logga None. Il server MCP non chiama provider: serve che il bridge/router passi `usage` (non trovato nel repo, da cablare sul desktop).
Test: `python -m unittest test_gstn_anim_mcp test_anim_mcp_cheap` in Tools/AnimPipeline/tests: 29 OK, 1 errore (test_ps1_gate_fail_propagation: manca `pwsh` nel cloud, da rilanciare sul desktop). Gate invariati.
Cambio test esistenti (2): lista tool con anim_run_intent; il test rollback ora fa lo snapshot esplicito (dry_run non lo fa più); il test MCP legge il file details.
Piano docs/notes/anim-mcp-costo-velocita.md: NON presente sul branch (cercato), implementato dal brief del coordinatore.

Inventario follow-through (nome | cosa fa | esposto in MCP):
- geom_intent_ue.py `swing_lag_cm`/`catch_fwd_cm`/`head_lift_deg` (+ envelope lag/catch/head) | ritardo piede in aria, recupero, testa che segue; inviluppi smoothstep a tempi fissi | SI (anim_apply_intent)
- geom_intent_ue.py `smooth_upper_passes`/`smooth_leg_passes` | assestamento per smoothing | SI
- geom_intent_ue.py `arm_*_elbow`, envelope `range` | ritocco ampiezza in finestra | SI
- easing.py `strike`, `rec(lag)` | curve di avvicinamento/rientro con lag per i colpi | NO
- spacing_rules.py (`recovery_spacing`, ARRIVE_CM) | misura se l'arto arriva morbido; controllo, non crea | NO
- sync_cards.py (frame `followthrough`, `recovery_settle`) | marcatori di scheda, non curve | NO
- jab_v6_tailtry_build.py | follow-through organico nella coda del jab (Cascadeur) | NO
- v6_targets.py `ORIENT_LAG` | lag orientamento polso/testa | NO
- cards/jumpland/land_restyle_casc_v1.py, land_retime_v2_ue.py | overshoot/assestamento atterraggio | NO
- cards/runstart/runstart_curves_v3.md | tabella overshoot/follow-through/assestato | NO (doc)
- dropkick-capsule-2: DROPKICK_HOLD_FOLLOW_THROUGH (hold, non curva) | NO
- crouch-veloce: BLP_GSTN_FollowThrough (blend profile) | NO
Nessun tool generico "overshoot/ritardo per catena su clip qualsiasi" esposto: in MCP c'è solo il lag piedi/testa dentro l'intent del run-start.

COSTO: ~1.5 $ | 0 minuti desktop | 0 token review
