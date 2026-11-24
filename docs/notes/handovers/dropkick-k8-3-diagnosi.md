# dropkick-k8-3-diagnosi (portatile, offline, 2026-11-24, Opus medium) - perche' K7 e K8 sono "scattosi e per niente naturali"
Owner 23:02Z/23:03Z: K8 giro 2 scattosa e innaturale, e K7 ha lo stesso problema. Nessun editor, nessuna posa nuova.
## 1. Catena di cattura: NON e' la causa (stessa su K7 e K8)
- frames.jsonl (Saved/AnimPipeline/captures/dropkick-kick-k6/k8r2/<run>/<seq>/): un fotogramma per tick (62,5 Hz), posizione del
  montage che avanza regolare (1,00/1,04 per tick) da v138 in k7_whiff, k8_whiff, k7_hit, k8_hit: nessun frame doppio o saltato.
  Hitstop solo nelle run hit (montage fermo a 14,99 da v153), voluto. Il video "real" e' a 60 fps = 0,96x, uguale per tutti.
## 2. Causa comune: il build mette uno STOP su ogni chiave (staccato)
Velocita' per frame della clip (dump UE joints_AS_OverlordDropkick_Startup{K7,K8}.json, mani/avambracci/testa/piede relativi al bacino):
- frame in cui TUTTO il corpo si muove < 3,5 cm: K7 f7-f8, K8 f2-f3 e f11. Bacino K7: 3 2 0 0 5 3 3 0 1 2 3 1 4 9 12 (cm/frame).
- salto massimo / mediana: K7 8,1x, K8 16,4x (mano K8 f9->f10 56-59 cm in un frame, poi 0-1 cm a f11).
- Gli stop cadono sulle chiavi della carta (K7 0,1,2,4,5,7,11; K8 0,1,2,4,7,9,10,11). Perche': in ue_rig.py i profili di spacing
  pesati (hold_recoil, arrive_1_2, charge_long) mettono TANGENTI PIATTE sulle chiavi ("their keys are pose hits"): ogni chiave
  che confina con un segmento pesato diventa una fermata. Con 8-10 chiavi in 16 frame la mossa e' stop-and-go, non un'onda.
- In piu' la trappola nota del build (startup ritemprato, f1-f2 = posa di f0): K8 a1-a4 quasi fermo (mani 2-3 cm/frame).
- K8 lo peggiora: tenuta della porta da calcio con chiavi vicine e quasi uguali, chiusura concentrata in un frame, palla ferma a12.
- NON verificato: settle K6 e loop K6 (condivisi da K7/K8) non sono nei dump; prossima sessione con editor: CJD_ASSETS su quelle due.
## 3. Opzioni (prima curve e parametri, regola 15c)
A (consigliata) Curve: solo le vere pose d'impatto restano piatte (a15 strike; a12 chiusa con un piccolo overshoot, non ferma);
  tutte le altre chiavi con tangenti automatiche (profilo None) o ease_from/ease_into a un lato, e la chiusura su 3 frame (a9-a12,
  picco ~30 cm/frame). Cancello numerico nuovo prima di catturare: nessun frame dello startup con tutto il corpo < 3,5 cm/frame fuori
  dalle tenute dichiarate, salto max/mediana <= 4x. Vale per K7 (come variante, K7 resta bloccata) e K8. ~2 $, 1 build + 1 cattura.
B Follow-through procedurale (anim_apply_intent follow_through, gia' nella skill) sulla clip cotta: braccia e testa in ritardo di 1-2
  frame, cosi' nessun frame e' fermo insieme. Piu' veloce ma non toglie gli stop del bacino. ~1,5 $.
C Chiavi su ogni frame da una spline monotona (pose_plan.py: passa per le chiavi senza fermarsi) invece delle chiavi sparse del build.
  Cura piu' profonda, tocca la pipeline per tutte le mosse da carta: thread di codice a parte. ~4-5 $.
SUBAGENTI: 0
COSTO: ~0,8 $ (stima, offline) | 0 minuti desktop | 0 token review
