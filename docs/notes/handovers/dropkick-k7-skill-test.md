# dropkick-k7-skill-test (portatile, 2026-11-23, Opus medium) - giro 3 di K6 come test della skill
Branch claude/project-thread-j1twqo = 2d0gnu + k9d2l7 (conflitti come da brief: CHT_PoseMatch di k9d2l7/integ, entrambi i blocchi
di pie_capture_move.py, entrambe le righe di gasp-divergenze) + t1yn9x (camera dal pavimento, capsula che sale). Checkout cp1verify
(n216qs occupato dai get-up; copiati Content/AnimPipeline e Saved/AnimPipeline/refs da n216qs, non versionati).
## Fatto
- Build 349 s ESITO OK; test GSTN.Camera + Timeline.RemoveAttackHitboxTracks 19/19 Success (Saved/Logs/ci/k7-test.log.ue.log).
- Unit offline MCP 73: 2 errori di ambiente (pwsh assente; PNG ancora aperto nel teardown su Windows).
- k7/make_k7_cards.py: ali tenute a1-a3, pugno a lato testa, rilascio largo, bacino che guida, palla stretta da a5 -> a12.
  Build animpipe x3 nello stesso tentativo (co_motion gambe incollate, poi gomito sx 5,2 cm < 6): prima della cattura, 0 catture perse.
- Variante K7 (AM/TL/GA/KIT/WPN) e hitbox (sweet r24 piante, sour r17, late r13, hitstop 13/11/8), dry run prima.
- Cattura: 2 sessioni (kit diversi), 5 run, ASSERT OK, colpo a v153 in base e K7. Pacchetto docs/notes/reviews/dropkick-kick-k6/k7.
## Strumenti (etichetta strumento, non giro)
- clip_joints_dump_ue.py: fps letto male (30 invece di 60) e asse sbagliato (clip UE guardano +Y): corretti (CJD_FACING=y).
- anim_capture_move: il cancello vuole un report di anim_run_intent, che non esiste per pose fatte da carta -> RIFIUTA; e un
  solo kit per sessione, quindi niente A/B base+variante in una sessione per le mosse. Catture fatte con pie_capture_timed +
  PIE_CAPTURE_RUNS (prima prova reale: funziona). Il manifest NON e' stato forzato.
- silhouette: segna "illeggibile" quasi tutto il tuck (testa sempre nel busto): serve taratura per mosse raccolte.
- review_pack: 8 immagini attorno ad anchor/FAF saltano l'inizio dello startup: aggiunte 2 griglie fitte a mano.
## Aperto
- Review cloud (regola 10, Opus high + critico cieco): solo griglie + PNG + criteri. Poi decide l'owner (giro 3 = ultimo).
- Fix in coda: cancello per mosse da carta, kit per run in PIE_CAPTURE_RUNS, taratura silhouette, selezione frame startup.
SUBAGENTI: 0
COSTO: ~7 $ (stima) | 55 minuti desktop (build 6, test 1, animpipe 15, catture 11, resto attese) | 0 token review
