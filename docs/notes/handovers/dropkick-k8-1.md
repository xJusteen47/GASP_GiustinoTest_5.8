# dropkick-k8-1 (portatile, 2026-11-23, Opus medium) - K8 = K7 + tutte le proposte Doom; K7 approvata e bloccata
Branch claude/project-thread-6blrlz = jcugce (skill-fix) + K8. Checkout cp1verify. Carta: cards/overlord/dropkick/k8/make_k8_cards.py.
## Fatto
- Build jcugce: ESITO OK 169 s (Saved/Logs/ci/k8-build.log). K7 scritta in docs/notes/anim-approvate.md; tag approved/dropkick-k7 NON
  pushato (bloccato dal classificatore): da fare su ac8d6201.
- K8 (solo a1-a12; snap a14-a15, settle K6, loop K6, gambe K7, hitbox K7 invariati): porta da calcio a1-a10 con tenuta che respira
  (gomiti e pugni salgono ~8 cm), chiusura a10-a12 con breakdown davanti al viso, conserte alte e larghe (fit con wide_cost), spalle su
  6 gradi attorno alla testa centrata e affondata (collo +7), suole piatte (foot_rot 0 = +22 gradi su K7, max 28), palla +16 gradi indietro.
- Tentativi: 1 (2 build animpipe: la prima co_motion FAIL, braccia ferme a0-a7 -> tenuta che respira). Check animpipe: restano 3 blocchi
  IDENTICI su K7 approvata (legibility da pugno su un calcio, contact_faf 15 vs finestra 14, hitstop_snap 35 cm 14->15 dello snap):
  ereditati, non toccati. Saved/AnimPipeline/animpipe/overlord_dropkick_startup_k{7,8}_check.txt.
- Cancello della skill (capture_k8.py, dry run): FAIL per quei blocchi + file hitbox che non elenca lo startup (lo script campiona solo
  loop/settle). NON forzato: cattura con il motore della skill (pie_capture_timed + PIE_CAPTURE_RUNS, kit per run) come K7.
- Cattura una sessione, 3 run (k7, k8, k8_hitbox), 9,6 min, ASSERT OK su tutte (Saved/AnimPipeline/captures/dropkick-kick-k6/k8/*/assert.txt).
  Colpo sul manichino in entrambe (si piega da v154, griglia hit az90). Pacchetto docs/notes/reviews/dropkick-kick-k6/k8 (build_pack).
## Numeri silhouette da dietro (anim_silhouette_check, dump UE CJD_FACING=y, silhouette.json; K7 con lo stesso tool in %TEMP%)
- Gomiti fuori dal busto a12 (f11) az180: K8 15/21 cm, K7 -1/3. Porta da calcio a4-a10: K8 34/21-28 cm, K7 7-11 (palla).
- Frame "palla informe" az180: K8 1/16 (f15, strike invariato), K7 8/16. Area dell'inviluppo +54 % az180, +56 % game.
- motion: 9 rotture d'arco ai polsi f9-f11 e 1 pop wrist_r f9 = la chiusura in 2 frame voluta (58 cm/frame).
## Test della skill (jcugce) - trovato
1. hitbox in cattura NON funziona: pie_capture_move chiamava get_debug_hitbox_shapes sul componente di test (AttributeError su 320/320
   frame, hb_err in frames.jsonl). Corretto (GSTN_CombatSimComponent dal pawn), NON riverificato: i video hitbox/ sono senza forme.
2. Kit per run e allineamento: OK (offset whiff k8 = 1, unknown vuoto). 3. Il manichino della run hit resta nelle run whiff successive
   (si vede sotto K8 nelle viste whiff): solo cornice. 4. Cancello: le carte con blocchi ereditati non passano mai (serve un'esenzione
   per mossa approvata o check sulle sole chiavi toccate). 5. Camera game troppo lontana sul salto (personaggio minuscolo, anche K7).
## Aperto
- Review cloud regola 10 (critico cieco): griglie startup/ (az180, az90) + silhouette_*.png. Poi decide l'owner. Clip atterraggio: thread a parte.
- Interpretazione "28 gradi": rotazione dei piedi verso il bersaglio (studio Doom), non il roll verso camera della critica Opus.
SUBAGENTI: 0
COSTO: ~8 $ (stima) | 35 minuti desktop (build 3, animpipe 12, cattura 10, resto) | 0 token review
