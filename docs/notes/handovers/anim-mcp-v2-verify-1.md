# Handover anim-mcp-v2-verify-1 (2026-11-23, desktop, Sonnet medium)
Branch: claude/anim-mcp-v2-turn4 (base 977f2fe1). Obiettivo: verificare gstn-anim-mcp v2 + COST TEST. Esito: PARZIALE, fermo al primo punto stabile.

## Fatto
- Unit test MCP: `Tools/CI/Run-Timed.ps1 -Kind Test -Exe python -ArgLine "-I Tools/AnimPipeline/tests/test_gstn_anim_mcp.py"` -> ESITO: OK, 14 test, 3 s (log: Saved/Logs/ci/mcp_unit.log, non committato).

## NON fatto (bloccato, 0 tentativi di fix usati)
- Smoke headless UE 5.9 e COST TEST: servono `GASP_Giustino2_ue59` (Run-SparseAnimStep.ps1 copia script/asset li').
  `_ue59/IN-USO.txt` dice "turnarounds-4 P5 codice piede R (thread desktop) dal 2026-11-21 23:56 UTC" ed e' modificato (non committato), con asset owner sporchi
  (DA_Locomotion_Default, w2t180Ls, CHT_PoseMatch, pie_capture_move.py). Nessun UnrealEditor in esecuzione al 2026-11-23 10:09, ma la regola 7 vieta di toccare _ue59 con un lock non mio.
- Worktree antigravity ha sporco non committato di Antigravity (uasset w2t090Lp/Rp/180Rp, build_base27 script): NON toccato, non committato.

## Serve per proseguire
1. Conferma che il lock e' scaduto (o che il thread turnarounds-4 ha finito), poi un thread con -Kind Test smoke + cost test.
2. Cost test: modello economico (deepseek-flash via bridge) su UNA posa (es. run start 29w3); registrare costo e giri di review vs percorso abituale in handoffs/costi.csv.
3. Condizione owner: se il costo per posa non scende, raccomandare di fermarsi (nessun dato ancora).

COSTO: ~0.5 $ | 5 min desktop | 0 token review
