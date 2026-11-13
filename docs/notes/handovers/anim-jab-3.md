# Handover: jab Overlord visto in gioco, kit di test e cattura PIE (ANIM-JAB-3, 2026-11-12)
Branch `claude/desktop-anim-jab-3` (da b00a916e, worktree `GASP_Giustino2_m7`). `_feature` e lo stash non toccati. Niente C++, nessuna mappa salvata.

## Verdetto (video in-game guardato frame per frame, 6 viste + telecamera di gioco; giri di correzione animazione: 0)
- **Jab 1: PASS con note.** Parte dall'idle, piedi fermi, sul colpo il fermo cade sulla posa di contatto (a6), nessuno scatto dopo i frame attivi,
  tenuta f6-13 pulita, rientro pulito. Note per l'owner: lo schiaffo percorre tutto l'arco in 1 frame (a5 ancora in caricamento, a6 braccio teso);
  gambe molto piegate e larghe per tutta la mossa (è nell'asset: piedi a 78-79 cm dal bacino); dita della mano sul ventre aperte a ventaglio.
- **Jab 2: FAIL in gioco sul colpo, a vuoto è a posto.** A vuoto: caricamento fermo a3-a8, affondo di tutto il corpo in 1 frame (a8→a9), tenuta,
  mano al viso, tremore e chiusura leggibili. Sul colpo: la mano arriva (a9, a10), poi **torna indietro a a8,64** (posa prima del contatto, mano
  a ~40 cm dal bersaglio), resta lì per i 4 frame di hitstop, poi riparte. Non sono le pose dell'owner: è la sincronizzazione (sotto).

## Cause trovate (numeri in `docs/notes/reviews/overlord-jab-v2/chain_hit_frames.jsonl`)
1. La timeline conta frame di simulazione (16 ms), l'animazione frame a 60 fps: il frame 9 della timeline vale a8,64. Con l'arrivo in 1 frame,
   0,36 frame sono decine di cm. **Prossimo passo consigliato (dati, 1 riga):** `montage_play_rate` = 62,5/60 = 1,0417 in
   `Tools/GAS/setup_overlord_jab_kit.py` (`_timeline`), così frame carta = frame timeline; poi rifare setup, cattura e guardare solo il colpo.
2. Il montage sincronizzato parte ~2 frame avanti (alla prima immagine la timeline è a 0, il montage a 1,96) e sull'hitstop viene riportato al
   tempo della simulazione: da qui il salto all'indietro. È codice di GSTNCombatNP, vale per ogni mossa: serve un thread di codice.
3. Scelta mia da rivedere: i montage hanno il blend-out di default (0,25 s), quindi gli ultimi 15 frame di ogni animazione sono mischiati con
   l'idle. Per vedere il rientro com'è nell'asset: `blend_out` a 2 frame nello stesso script e ricattura.

## Fatto
- `Tools/GAS/setup_overlord_jab_kit.py` (headless, idempotente): copia gli AS costruiti in `/Game/GASNPPTest/Combat/Overlord`, montage,
  `TL_Overlord_Jab{1,2}`, `GA_Overlord_Jab{1,2}` (figli BP senza grafo), `WPN_Test_Overlord`, `KIT_Test_Overlord`. Jab 2 = stesso comando
  `Command.Attack.Neutral` con requisito `Test.State.OverlordJabChain` (track Tags f8-13 del jab 1, finestra di cancel f9-13). Verificato in gioco:
  ripressione nella tenuta e pressione anticipata (buffer) danno entrambe il jab 2.
- **Placeholder, decide l'owner:** danni/knockback/hitstop, hitbox a sfera, finestra di cancel f9-13, blend-in 2 frame.
- Kit su una pedana dell'hub (800, 700) e in `KitsWithDefaultBindings` di GSTNHeld (si raccoglie e si scambia come gli altri). 3 tag in `GSTNCombatTest.ini`.
- `Tools/GASNPPTest/pie_capture_move.py` (Play vero in L_Test_Hub, 60 fps esatti, 7 telecamere, stato per frame) e `pie_capture_grid.py` (griglie, mp4).
- Video e griglie guardate: `docs/notes/reviews/overlord-jab-v2/`. Nessuna base: sono mosse nuove.

## Da sapere
- `Content/AnimPipeline` è UNA cartella sola (junction) per tutti i worktree: gli asset dell'owner si leggono da m7, non copiarli.
- L'editor dell'owner su `_feature` è rimasto aperto (DefaultLevel sporco dal thread prima: alla chiusura **Don't Save**); ho lanciato un secondo
  editor su m7 (1 setup headless + 4 catture). La cattura aggiunge 3 luci solo nel Play (hub senza cielo, manichino metallico).

Costo stimato: ~190k token di contesto (Fable, nessun subagente), oltre la soglia dei 150k per le 4 catture: chiudo qui senza il giro di correzione.
