# Handover anim-skill-t0 (2026-11-23, cloud, Opus high + 1 subagente Sonnet). Branch claude/project-thread-k9d2l7
Obiettivo: preparazione T0 della skill (MCP per mosse di combattimento + anti-spreco + silhouette/vettori) e hitbox
provvisorie del dropkick. Esito: CODICE FATTO, test offline OK; niente eseguito in UE.
- Branch = integ-2026-11-23 + anim-mcp-followthrough + T0 + hitbox. NON contiene il ramo dropkick (conflitto binario
  CHT_PoseMatch, vedi docs/notes/dropkick-k7-skill-test-brief.md).
- Hitbox: Tools/GAS/setup_dropkick_hitboxes.py + UGSTN_TimelineUtils::RemoveAttackHitboxTracks (+ test
  GSTNCombat.Core.Timeline.RemoveAttackHitboxTracks). Sweet sfera r24 sulle piante, sour capsula r17 anche-caviglie,
  forte 4 frame da startup f15; late capsula r13 ginocchia-caviglie fino all'atterraggio. Hitstop 13/11/8 entrambi.
  Riferimenti: ssbwiki Ganondorf (SSBU) down aerial (ID0 r7 19%, ID1 r6 17%, attivo 16-18); j.HK Big Band NON leggibile
  (mizuumi blocca i bot): 13 = freeze tipico dei pesanti (sonichurricane.com/?p=1043). Provvisorio, l'owner le ritocca.
- MCP (subagente Sonnet, 5b425c56 + a7584d3b): anim_capture_move con gate (report, dry run, hitbox sync, leggibilita',
  tentativi), cache base, needs_build, review_pack max 8 per vista, manifest con minuti/costi, id variante >= 40, guardia
  geom solo RunStart, anim_silhouette_check e anim_motion_overlay (PIL), clip_joints_dump_ue.py, SKILL cheap aggiornata.
- Test: `python3 -W ignore -m unittest test_follow_through test_gstn_anim_mcp test_anim_mcp_cheap test_t0_prep` da
  Tools/AnimPipeline/tests: 73, 1 errore (pwsh assente nel cloud). C++ non compilato qui.
- Non verificato: cattura reale e PIE_CAPTURE_RUNS, dump UE, orientamento viste su dati veri, padding busto 7 cm.
- Prossimo: thread fisso dal brief docs/notes/dropkick-k7-skill-test-brief.md, poi review cloud.
SUBAGENTI: 2 (Sonnet: codice T0; web-fetch: dati Ganon/Skullgirls)
COSTO: ~6 $ (audit 2.5 + T0 3.5) | 0 minuti desktop | 0 token review
