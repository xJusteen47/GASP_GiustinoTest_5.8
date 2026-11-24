# skill-k8-fix-1 (cloud, 2026-11-23, Opus medium, budget 2 $) - skill di animazione: 5 miglioramenti dal giro 1 di K8
Branch claude/project-thread-btrk7b, su claude/project-thread-6blrlz (0c3769b8, K8 r2 prep). Solo testo e script, nessun editor.
## Fatto (dettagli: .claude/skills/gstn-anim-executor/project.md "After K8 round 1"; reviewer project.md idem)
1. Coerenza pose: animpipe/checks.py avviso `pose_coherence` (card con "base_card": bacino >= 3 gradi dalla base e piedi/pole
   delle gambe uguali; anche fra pose consecutive >= 9 gradi). Prova a secco: K8 r1 (0c3769b8~1) vs K7 -> "a11: pelvis 16 deg,
   leg_l/leg_r not re-keyed, ~21 cm"; K8 r2 vs K7 e K7 vs K6: nessun avviso. Solo avviso, verdetto invariato.
2. Cattura: pie_capture_move ordina TUTTI i whiff prima degli hit (ordinamento stabile, setup run rifatto a ogni cambio),
   parcheggia i manichini prima di ogni whiff; manichino entro 15 m -> dummy_in_view.txt -> capture_move rifiuta il pacchetto.
   PIE_CAPTURE_KEEP_ORDER=1 = ordine vecchio. NON provato in editor.
3. Cancello: capture_move.inherited_blocks: un blocco che fallisce con la STESSA riga sulla base approvata (in anim-approvate.md,
   check della carta base esistente) e' "ereditato", non blocca; nuovi fallimenti o numeri cambiati bloccano ancora.
   capture_k8.py ora passa approved_base_card = K7. Serve `animpipe check` sulla carta K7 una volta (sul PC).
   NON toccato: il file hitbox che non elenca lo startup (seconda causa del FAIL r1) resta bloccante.
4. Avviso `arm_jump`: mano/gomito (spazio torace) >= 20 cm in un frame e >= 2,5x i vicini, fuori dai frame attivi;
   esenzione `arm_jump` nella carta. Provato solo su dati sintetici (legbones K8 non in git).
5. Hitbox: ogni run deve impostare gstn.Combat.Debug.Hitboxes (0 review, 1 hitbox_view) o la cattura e' rifiutata
   (i cvar non si annullano e ora i whiff della run hitbox precedono gli hit); il pacchetto fallisce se griglie o video di
   review chiedono hitbox. Il fix di r1 (forme hitbox vuote, GSTN_CombatSimComponent) resta da riverificare.
## Prova: tests/test_anim_mcp_cheap.py 12/12 (pytest); py_compile dei 5 script; test a secco sopra (comandi nel thread).
## Aperto
- Prova reale sul giro 2 di K8 (thread K8): check con base_card, cattura con l'ordine nuovo, gate_report con gli ereditati.
- docs/INDICE-PROGETTO.md non e' su questo branch (regola 1): voce "skill animazione" da aggiornare al merge.
SUBAGENTI: 0
COSTO: ~1,8 $ (stima) | 0 minuti desktop | 0 token review
