# dropkick-k5-1 (portatile, 2026-11-21, Opus) - calcio del dropkick: braccia conserte, scatto, caduta compiaciuta. build + catture fatte, review da fare
Branch claude/project-thread-72dtra (da xbjsuj 18ad9e24), checkout GASP_Giustino2_n216qs. Nessun asset di gioco toccato.

## Fatto
1. Diagnosi (docs/notes/reviews/dropkick-kick-k5/diagnosis.md, immagini accanto): il video "hit" di r4 non colpisce mai il manichino
   (dummy fermo, damagePercent 0) e il dropkick "low" parte a 7,8 m (AIR_GIVE_UP); le braccia nel calcio NON sono conserte (a0c7bf9f:
   gomiti ai fianchi, pugni al mento = guardia); la review r4b guardava solo il rimbalzo. Il calcio: lean lento + gambe insieme, niente carica.
2. Carte variante: cards/overlord/dropkick/overlord_dropkick_{startup,loop}_k5.json da k5/make_k5_cards.py (originali intatti).
   Braccia fittate in k5/fit.py (polsi a -6/+6 cm, gomiti +15/-16, avambracci piatti sul petto); preview k5_startup_keys_stick.png.
   Sheet delle curve: overlord_dropkick_k5_curves.md. Durate invariate (16 + 12 f), part_delays 0.
3. Cattura: pie_capture_move.py PIE_CAPTURE_AIR_GIVE_UP + sequenze dropkick_sweet_hit/whiff (pressione a 380 cm in caduta).
4. Variante di gioco pronta da lanciare: Tools/GAS/setup_dropkick_kick_variant.py -> AS/AM ...K5, TL/GA_Overlord_DropkickTBK5,
   KIT_Test_PlaytestTBK5 (copia di TBR4, rimbalzo R4). Timing csv copiati in Saved/AnimPipeline/refs/ufd.

## Seguito (owner 14:54Z "apri l'editor", hitbox le mette lui)
- Build: animpipe author+build startup_k5/loop_k5 OK; check: 1 correzione (mani 5-8 cm fuori dal petto). Restano contatti 3-5 cm
  avambraccio/avambraccio e avambraccio dx/spine_03 a f11 (clearance.txt in Saved/AnimPipeline/animpipe/*_k5_check): da giudicare sul video.
  hitstop_snap FAIL 14->15 55 cm e co_motion (braccia ferme) = voluti (lo scatto e' entrare in f15).
- setup_dropkick_kick_variant.py via bridge: AS/AM ...K5, TL/GA_Overlord_DropkickTBK5, KIT_Test_PlaytestTBK5, WPN copy (log Saved/DropkickK5).
- Catture Saved/DropkickK5/{k5a (380 cm: atterra prima di f15), k5b, base} con SWEET_H=480 e AIR_GIVE_UP=200: ASSERT OK (k5b, base).
  Pacchetto review: docs/notes/reviews/dropkick-kick-k5/k5b/ (griglie base vs K5 allineate su a0 = inizio montage, mp4 game 1x e 4x).
- **Gioco**: il manichino NON reagisce mai al dropkick (piedi dentro la sua capsula a f15-f18 in k5b, dummy fermo, damagePercent 0)
  ne' in nessuna cattura dropkick sul portatile (r4..r4e, base): hitbox/registrazione colpi, lato owner (le hitbox le mette lui).
- Con la pressione a 480 cm si atterra ~3 frame dopo f15: la caduta "compiaciuta" (loop) si vede solo con una pressione alta
  (es. SWEET_H 700): da catturare per la review della caduta.
- Prossimo: review cloud (regola 10) su k5b: tratti PRIMA (gambe unite, braccia conserte a3-a19, compiaciuto in caduta), poi stacco a13->a15.

## Per l'owner
Sweetspot vero (colpo forte all'inizio, debole dopo) = modifica di gameplay sulla timeline (finestre hitbox/danno/hitstop): decisione sua.
Il clip di atterraggio a59 (1 frame al touchdown) ha ancora le braccia vecchie: si vede 1 frame, non toccato.
Prove: frames.jsonl e capture.log di Saved/DropkickTB4/r4e (comandi in diagnosis.md); numeri delle braccia da k5/chest.py.
COSTO: ~11 $ (stima, budget 8 $ superato per build+catture) | 0 min desktop | 0 token review
