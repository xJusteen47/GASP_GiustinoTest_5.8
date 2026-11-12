# Handover: Jab 2 dell'Overlord sul colpo, correzione di sincronizzazione come dato (ANIM-JAB-4, 2026-11-12)
Branch locale `claude/desktop-anim-jab-4` (da `claude/desktop-anim-jab-3`, worktree `GASP_Giustino2_m7`), su origin come `claude/project-thread-ipc4gc`. Niente C++, nessuna mappa salvata, `_feature` non toccato.
## Verdetto (solo il difetto del colpo; le pose NON sono state giudicate: l'owner le rifà in un altro thread)
- **Difetto risolto nella cattura in gioco.** Griglia az270 v038-049, stessi frame di base e variante (`chain_hit_base_…` e `chain_hit_rate_…` in
  `docs/notes/reviews/overlord-jab-v2/`): nella base la mano torna indietro in v042-v045 (a8,6, ~40 cm dal bersaglio); nella variante resta sul
  bersaglio da v041 a v049, ferma ad a9,5 per tutto l'hitstop, nessun ritorno. Seconda cattura (solo numeri): ferma ad a9,33, stessa cosa.
- Jab 2 a vuoto: catturato, video committato, controllati solo i numeri (parte a v33, dura quanto la timeline). Griglia non guardata (fuori scopo).
- **Non dire all'owner che il jab è pronto**: le pose vanno rifatte, questo thread ha chiuso solo la sincronizzazione.
## Cosa è cambiato (dati e script)
1. `Tools/GAS/setup_overlord_jab_kit.py`: `MONTAGE_PLAY_RATE = 62,5 / 60` sulle due timeline. Ora frame della timeline N = frame di animazione N
   (prima il frame 9 mostrava a8,64). **Vale per ogni mossa fatta a 60 fps: va portato sulle animazioni rifatte** (stessa riga, o nel passo che
   crea le timeline). Effetto collaterale: in tempo reale l'animazione va il 4% più veloce (un frame della carta dura 16 ms, non 16,7).
2. Stesso script, errore al secondo lancio: il binding del jab 2 nasceva da quello del jab 1 e teneva il tag della catena sia richiesto sia
   ignorato, quindi il jab 2 non partiva più (prima cattura di questo thread buttata per questo). Corretto in `_with_chain`.
3. `pie_capture_move.py`: `PIE_CAPTURE_SEQUENCES` per catturare solo alcune sequenze. Asset rigenerati: 2 montage, 2 timeline.
## Resta da fare in un thread di CODICE (GSTNCombatNP, vale per ogni mossa; numeri in `chain_hit_rate_frames.jsonl`)
- Il montage sincronizzato parte ~2 frame avanti (prima immagine a2,04 con la timeline a 0). A volte viene riallineato alla seconda immagine
  (2,04→2,25), a volte no: allora corre ~1,25 frame avanti finché l'hitstop lo riporta indietro (jab 1: 7,25→6,0; non si vede solo perché a6-a7
  sono la stessa tenuta). Alla ripresa resta fermo un'immagine in più e poi salta ~2,2 frame.
- Lo sfasamento è una frazione di frame, quindi l'arrivo "in 1 frame" in gioco passa per un'immagine intermedia (v040, a8,3-8,5: braccio a metà).
- Con la riga 1 il punto di ritorno è la posa di contatto e non più quella prima: per questo il difetto non si vede più, ma il salto esiste ancora.
## Aperto, decide l'owner (invariato da ANIM-JAB-3)
Arrivo in 1 frame; gambe del Jab 1 (piegate e larghe); dita a ventaglio; danni, hitbox a sfera e finestra di cancel f9-13 provvisori; blend-out
del montage provvisorio (default 0,25 s: con la velocità nuova l'uscita verso l'idle comincia ad a12,4 nel jab 1).
## Da sapere
- 2 setup headless + 3 catture su m7 con l'editor dell'owner aperto su `_feature`; nessun altro editor era in esecuzione.
- `Content/Levels/L_Test_Hub.umap` in m7 è non tracciato da prima di questo thread: non toccato, non committato.

Tentativi di correzione: 1 su 2 (più la correzione dello script). Costo stimato: ~115k token di contesto (Fable, nessun subagente).
