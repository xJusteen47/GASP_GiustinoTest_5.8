# Handover anim-mcp-follow-through-1 (2026-11-23, cloud, Sonnet medium). Branch claude/anim-mcp-followthrough (base claude/anim-mcp-v2-turn4 1243e7ee)
Obiettivo: tool GENERALE di follow-through per modelli deboli (matematica deterministica, il modello sceglie solo l'intento). Esito: CODICE + TEST OFFLINE FATTI; NON eseguito in UE (serve desktop).

## Cosa c'e'
- `gstn_anim_mcp/follow_through.py` (puro Python): intent `{"follow_through": {chain, mass, overshoot_pct|damping, delay_frames, settle_frames, anchor, ...}}`.
  Per ogni link: rotvec relativo alla posa all'ANCHOR, ritardo per link, molla smorzata (overshoot% -> zeta esatto, periodo da mass), peso 0 sull'anchor e a fine finestra, tetto max_extra_deg.
  chain: preset (arm_l, arms, arms_fingers, shoulder_arms, spine_head, neck_head), "shoulder>elbow>wrist>fingers"+side, o lista ossa. Solo busto/braccia/testa: root, pelvis, gambe, IK rifiutati -> piedi piantati per costruzione.
  anchor: "hit"+hit_frame, "faf"+faf_frame, frame clip-locale, oppure frame_range [anchor, fine]. Nessuna rotazione/quaternione passa dal modello.
- `follow_through_ue.py`: motore UE (legge rotazioni LOCAL, risolve, scrive solo le ossa della catena, report in Saved/AnimPipeline/runstart/followthru_<dst>.json). Importa follow_through da GSTN_MCP_DIR (lo script sparse copia solo se stesso in _ue59).
- `anim_ops.py`: validate_intent (follow_through da solo), plan_intent (dry_run: ossa, frame, picchi in gradi), apply sceglie lo script, run_intent -> `follow_through_verdict` (PASS/FAIL dal report; i gate run-start non si applicano). `mcp_server.py`: descrizione con esempio.
- Dry-run: senza dati della clip modella un arto che ARRIVA all'anchor da `assume_arrival_deg` (def 40) e riporta picchi + peak_per_100deg (lineare); il run reale riporta i picchi MISURATI per osso.
- Conservazione (`conservation_report`, sui dati): posa all'anchor identica (tol 0.01 deg), nulla fuori finestra, solo ossa della catena, nessun osso vietato. Hit frame invariato e' esatto (peso 0).
- Test: `Tools/AnimPipeline/tests/test_follow_through.py` 13/13 OK + test_gstn_anim_mcp 19/20 (l'unico errore e' `pwsh` mancante nel cloud, preesistente). Comando: `python3 -m unittest test_follow_through test_gstn_anim_mcp` da Tools/AnimPipeline/tests.

## Da fare sul desktop (non verificabile qui)
1. Un run vero: `anim_run_intent` su una clip (es. dropkick o w3) con `{"follow_through":{"chain":"arm_fingers_l","mass":"light","overshoot_pct":30,"anchor":"hit","hit_frame":N}}`; controllare che `LOCAL` pose + set_bone_track_keys scrivano giusto (API UE non provata offline).
2. Guardare il VIDEO (rule 10) prima di mostrare all'owner; il tool e' un parametro (rule 13c/15), non una clip nuova. Nessuna posa approved/* toccata.
3. Taratura: valori di default (periodo 9 f * sqrt(mass), overshoot 25%) sono ragionevoli ma NON giudicati a occhio.
## Note
- Limite: la molla filtra la rotazione RELATIVA all'anchor; se l'osso e' fermo dopo l'anchor la risposta e' solo drag+overshoot dell'arrivo. Per un whip sulla punta usare link_gain>1 e delay_frames.
- Idee non fatte: leve su gambe (escluse per i piedi), per-link overshoot, curve ops su tangenti.

COSTO: ~1.6 $ | 0 minuti desktop | 0 token review
