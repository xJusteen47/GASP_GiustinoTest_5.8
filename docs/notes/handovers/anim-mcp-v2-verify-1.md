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


---
# TURNO 2 (owner "vai", 2026-11-23): head/neck nel dump + cost test braccia
Codice di questo turno: `runstart_clip_joints_ue.py` dumpa anche pelvis, spine_01-05, neck_01, head; `anim_inspect` ha `head_neck` (head_pitch_rel_neck_deg, head_fwd_of_spine5_cm); `runstart_pivot_gates.py` ha il gate `head pitch step` (>12 gradi/frame = FAIL); `Run-SparseAnimStep.ps1` usa lo script di dump della WORKTREE (prima usava quello di _ue59 e ignorava le modifiche); +1 unit test (15/15 OK, Saved/Logs/ci/mcp_unit2.log). Verificato in UE: `joints_w3m4.json` contiene `head`.

## Scoperte sul tool (importanti)
1. `frame_range` e' CLIP-LOCALE (w3 ha 62 frame; "v107" = f30). Il test del turno 1 (frame 105-109, 107) cadeva fuori clip = no-op, senza errore. Va aggiunto un errore se frame_range > n.
2. Gli inviluppi in `geom_intent_ue.py` sono FISSI sui tempi del pivot dei turnaround (head f3.5-25, sh f0-31, arm_clr f6.5-16, arm_pv_fwd f0-13...); `frame_range` li taglia, non li sposta. Su un run start generico l'intent agisce solo nei primi ~40 frame. Misurato: arm_clr a f20-40 = w3m3 identico a w3.
3. Magnitudine non prevedibile: `arm_clr_deg 12`, frame_range [8,14] -> abduzione upperarm media +2.5 (L) / +1.4 (R) gradi, R max -4 (asimmetrico). Griglia stick da dietro f4-f18 (1 immagine, scratchpad grid_w3_vs_w3m4.png): differenza appena percettibile; servirebbe un giro in piu con ampiezza x4-5.
4. Gate pre-GPU: GATES OK, jitter knee_l 9.9 (w3: 8.95) in 4-18, bone worst 0.5%.

## COST TEST 2 (UNA posa: gomiti piu aperti, w3 f8-14, deepseek-flash via run_deepseek)
- 4 chiamate flash nel turno ($0.0974 ciascuna, ~0.39 $): 3 intent sbagliati per mio prompt/mancanza di doc (frame v- globali, envelope, range [107,107]); solo la 4a (finestra envelope nel prompt) e' valida.
- UE headless ~1 min per apply+dump+gate, 0 token. Video in-game NON fatto: servirebbe registrare w3m4 come variante del chooser (runstart_generic_rows_ue.py scrive tabelle Content) = fuori dal tool e sporca asset tracciati. Sostituto economico: griglia stick da joints (0 LLM).
- Il percorso abituale (w3 c2: pose + 3 build + capture + review 5/6) non e' stato rimisurato: non posso dire che il costo per posa cali. Provato: apply+gate ~1 min e 0 token. NON provato: qualita' finale e giri complessivi.
- RACCOMANDAZIONE: non estendere (R2) prima di: (a) errore su frame_range fuori clip, (b) envelope parametrici o frame_range che sposti davvero l'inviluppo, (c) tool `anim_register_variant` per poter catturare, (d) tempi degli envelope nella descrizione del tool cosi' il modello economico non indovina.

## Altre proposte per spendere meno senza calare la qualita'
- Cache dump joints/griglie per hash dell'asset (evita il re-dump di 1 min).
- Gate numerici bloccanti prima di ogni cattura (regola 15b): aggiunti head pitch step; aggiungere abduzione min/max per fase.
- Prompt template fisso per il modello economico (schema, unita', frame clip-locali): avrebbe evitato 3 giri su 4 qui.
- Griglia stick 0-LLM da joints come pre-review; video in-game solo se mostra la differenza attesa.
- Una sola sessione PIE per tutte le coppie (gia' in v2), cattura solo 180 L al primo giro.

COSTO: ~2.0 $ | 55 min desktop | 0 token review
