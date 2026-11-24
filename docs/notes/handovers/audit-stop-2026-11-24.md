# audit-stop-2026-11-24 (cloud, sola lettura, 2026-11-24) - audit stop-and-go di tutte le clip
Owner "si audit" (2026-11-24 00:34 Roma). Base: claude/project-thread-wwnerm 891c65de (no_stop_gate.py + curves.py).
Riuso (regola 1): gstn_anim_mcp/no_stop_gate.py, animpipe/curves.py (soglie), animpipe/card.py (segmenti),
clip_joints_dump_ue.py (dump). Nuovi: flat_key_audit.py (rischio dalla carta), audit_stop_dump_ue.py (dump per nome),
audit_stop_table.py (tabella col cancello).
## Esito
- Tabella: docs/notes/audit-stop-2026-11-24/tabella.md. Nel repo non c'e' nessun dump di giunti per frame: nessuna
  fermata/salto misurabile in cloud; nessun numero inventato.
- Rischio dalla carta (tangenti piatte non dichiarate su intervalli >= 2 frame), logs/flat_keys.tsv, comando in
  logs/flat_keys.cmd.txt: K7 6 chiavi, settle K6 3, loop K6 1, ftilt v3.1 3, dair v4 4, dair v8-v10 7-11; 0 su jab v12,
  jab v3, atterraggi, tech bounce, 8 carte reaction. Taratura: K7 (fermo misurato ai passi 2-3 e 7) cade su chiavi a
  rischio 2, 4, 5, 7. Limite: K8 ha solo 2 chiavi a rischio ma a1-a5 fermi (chiavi quasi uguali): rischio 0 non e' PASS.
- w3: additivo per frame su w2 (smoothstep), nessuna tangente; misura ancora necessaria.
## Prossimo passo (fisso, ~5 min, editor chiuso)
`Save-Assets.ps1 -Script Tools\AnimPipeline\gstn_anim_mcp\audit_stop_dump_ue.py -Tag audit_stop`, poi
`audit_stop_table.py ... --resolved=...` (righe esatte in fondo alla tabella). Poi un thread cloud compila le colonne
fermate/salto e le priorita' finali.
## Aperti
- audit_stop_dump_ue.py mai girato in UE (API ARFilter a memoria): primo tentativo puo' fallire.
- Quale asset ftilt/dair suona nel kit (Build o Pilot): il dump li prende entrambi.
- Tenute dichiarate: solo il dash (affondo in movimento). Frame da fermo iniziali dei run start: owner decide.
SUBAGENTI: 0
COSTO: ~1,6 $ (stima) | 0 minuti desktop | 0 token review
