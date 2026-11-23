# Handover anim-mcp-v2-verify-1 (2026-11-23, desktop, Sonnet medium)
Branch: claude/anim-mcp-v2-turn4 (base 977f2fe1). Obiettivo: verificare gstn-anim-mcp v2 + COST TEST. Esito: PARZIALE, fermo al primo punto stabile.

## Fatto
- Unit test MCP: `Tools/CI/Run-Timed.ps1 -Kind Test -Exe python -ArgLine "-I Tools/AnimPipeline/tests/test_gstn_anim_mcp.py"` -> ESITO: OK, 14 test, 3 s (log: Saved/Logs/ci/mcp_unit.log, non committato).

- Smoke headless UE 5.9 (lock _ue59 preso con ok owner 11-23 09:10, poi liberato): `anim_apply_intent` reale src=`w3` dst=`w3m1`, intent `{head_lift_deg:5, frame_range:[105,109]}` -> returncode 0, `GATES: OK` (bone worst 0.5%), uasset copiato in worktree (log: _ue59/Saved/Logs/ci/sparse_intent_AS_GSTN_RunStart_Generic_rcj.log). `_ue59` ripristinato: stesso sporco di prima (DA_Locomotion_Default, w2t180Ls, CHT_PoseMatch, pie_capture_move.py non toccati).
- Smoke dry-run e `anim_inspect(w3)` OK.

## COST TEST (UNA posa: mento w3 f107 +5 gradi, nota 3 di runstart-w3-cheap-skill.md)
- DeepSeek flash (run_deepseek): richiesta -> intent JSON, $0.0920 (il bridge riporta model deepseek-v4-pro). 1 correzione: il mio prompt diceva frame_range come stringa, il tool vuole [start,end] (validate_intent lo ha rifiutato) = 1 giro, imputabile al prompt non al tool.
- Esecuzione UE headless: ~1 min, 0 LLM token. Nessuna cattura GPU ne review fatte: il confronto completo con il percorso abituale (video + review) NON e' misurato.
- LIMITE TROVATO: il dump joints non contiene head/neck/spine, quindi `anim_inspect`/diff non possono verificare l'edit della testa (joints w3 e w3m1 identici). Serve aggiungere head/neck al dump prima di fidarsi dei gate pre-GPU per le pose di testa. Edit quindi NON verificato.
- Valutazione onesta: costo LLM per la richiesta ~0.09 $ (vs 5-10 $ per tentativo M1 negli ultimi costi.csv, ma non confrontabili: quelli erano build di clip intere). Il costo per posa scende solo se anche la verifica e' economica; finche' head/neck non sono nel dump la riduzione e' non provata. Raccomandazione: non fermarsi, ma ripetere il test su una posa verificabile dal dump (es. gambe/braccia) con 1 review video prima di R2.

COSTO: ~0.6 $ | 25 min desktop | 0 token review
