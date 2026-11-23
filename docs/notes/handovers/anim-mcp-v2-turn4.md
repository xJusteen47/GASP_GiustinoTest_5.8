# Handover anim-mcp-v2-turn4 (2026-11-23, desktop, Sonnet medium). Branch claude/anim-mcp-v2-turn4
Obiettivo: 4 fix dal cost test + UN retest "gomiti piu aperti, arm_clr 12". Esito: FIX 1-4 FATTI, RETEST OK (misura numerica), cattura GPU NON fatta.

## Fix (commit 5a0b71d0 + commit di questo turno)
1. frame_range fuori clip = errore: `anim_apply_intent` controlla n frame dal dump joints del src (ValueError, "clip-locale 0..n-1"); in UE `check_frame_range` stampa GEOM_INTENT ERROR e ritorna 2 (anim_ops lo propaga come ok:false). Provato: [105,109] su w3 (62 frame) -> errore.
2. Envelope parametrico: nuovo intent `envelope` = "range" | "turnaround". Con frame_range il default e' "range": trapezio sul range (rampe <=3 frame, plateau 1.0, uguale per tutti i canali), quindi i gradi chiesti si raggiungono dentro [a,b]. Senza frame_range resta "turnaround" (tempi fissi, invariato).
3. Nuovi tool `anim_register_variant(asset, variant_id=60)` / `anim_unregister_variant`: copia la clip in _ue59, backup byte-exact di CHT_GSTN_StockMover_PoseMatch, lancia runstart_generic_rows_ue.py (RSG_ASSET/RSG_K) via Run-SparseAnimStep.ps1 -KeepUe59Content (nuovo switch: non fa checkout/cleanup ne copia indietro); unregister ripristina i byte (sha256) e cancella la copia. Rifiuta se il chooser ha modifiche dell'owner (oggi e' dirty in _ue59) salvo allow_dirty_chooser=true. NON eseguito dal vivo: il brief vieta di toccare CHT_PoseMatch. Testato solo dry-run + restore su file finti. id ammessi 30..179, N+10 e' il sibling Handoff (tenere gli id registrati a distanza >=20).
4. Tempi envelope (clip-locali, turnaround e range) scritti nella descrizione di anim_apply_intent, con esempio JSON.

## BUG TROVATO dal retest (causa del "12 gradi -> 2.5")
`arm_clr` ruotava attorno all'asse avanti con `out_dir=lat_vec`, ma compute_character_axes inverte lat/fwd (segue la direzione di corsa) e lat_vec puo' puntare a DESTRA: i gomiti si chiudevano (-7 gradi, tentativo 1: w3m6). Nuova `arm_clr_quat` usa l'outward vero (spalla->spalla) e l'asse cross(braccio,out); inoltre con turn_side none apre ENTRAMBI i gomiti (prima solo L). Altri parametri (arm_pv_out, tuck, bank...) usano lo stesso lat_vec: NON toccati, da verificare uno a uno prima di R2.

## RETEST (w3 -> w3m7, {arm_clr_deg:12, frame_range:[8,20]}, UE headless ~1 min, 0 LLM)
- Spostamento gomito verso fuori vs w3: f10 L7.3/R7.1, f12 4.6/4.5, f14 6.0/5.4, f16 5.7/6.8, f18 4.2/4.4 cm; f4 e f22 = 0 (12 gradi su braccio ~28 cm = ~5.8 cm: coerente). Abduzione media f10-17: L 31.0->46.0, R 25.2->38.8 (+15/+14 sulla metrica di inspect). GATES OK, conservation OK.
- Prima del fix: w3m5 (solo range) L -1.7/R +2.4 gradi; w3m6 (arm_clr_quat con lat_vec) -7/-8. Log/numeri: Saved/AnimPipeline/runstart/joints_*w3m5|m6|m7.json (non committati), comando: anim_ops.anim_inspect(asset,[10,17]).
- Griglia stick da dietro (SVG, grigio w3 / rosso w3m7, f4-18): docs/notes/reviews/anim-mcp-v2-retest/grid_w3_vs_w3m7_behind.svg. NON l'ho vista renderizzata (il pane non apre SVG locali): verdetto basato sui numeri.
- Cattura GPU in-game NON fatta: serve anim_register_variant live = tocca CHT_PoseMatch dell'owner. Nessuno script di cattura dal laptop cercato su origin. Unit test: 20/20 OK (Run-Timed, log Saved/Logs/ci/mcp_unit4.log).

## Stato / DA DECIDERE (owner)
- D1: registrare w3m7 per la cattura = modificare temporaneamente CHT_PoseMatch dirty (backup+restore byte-exact). Consigliata A: si, a editor chiuso, 1 cattura 180 L. B: no, solo numeri.
- _ue59 lock preso e liberato; sporco _ue59 identico a prima (DA_Locomotion_Default, w2t180Ls, CHT_PoseMatch, pie_capture_move.py).
- Asset di prova w3m5/m6/m7 e w2t090* restano UNTRACKED nella worktree (non committati). Fermo prima di R2.

COSTO: ~2.2 $ | 50 min desktop | 0 token review

## AGGIORNAMENTO cattura (owner A, 2026-11-23 17:3x UTC)
- w3m7 registrata come variante 60 (anim_register_variant, allow_dirty_chooser), cattura UNICA mv_run_start_v5 (partenza in avanti: w3 non e' un turnaround, quindi non 180 L), viste az180+az90, a=variante 21 (w2 default, NON w3: confronto non omogeneo). ASSERT: OK per entrambe (Saved/CaptureSpeed/w3m7_t4/*/assert.txt, nel _ue59).
- Griglia da dietro (grid_ab_az180.jpg, guardata una volta): in w3m7 v92-v107 i gomiti sono visibilmente piu larghi della base; nessuna compenetrazione evidente. Review cloud indipendente (regola 10/15a) NON fatta: va aperta prima di mostrare all'owner.
- anim_unregister_variant: chooser ripristinato, sha256 OK (chooser_restored_sha_ok True), copia clip rimossa da _ue59; sporco _ue59 identico a prima; lock liberato.
COSTO: ~3.0 $ totali | 70 min desktop | 0 token review
