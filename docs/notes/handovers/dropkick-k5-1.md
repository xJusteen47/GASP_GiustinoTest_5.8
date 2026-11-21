# dropkick-k5-1 (portatile, 2026-11-21, Opus) - calcio del dropkick: braccia conserte, scatto, caduta compiaciuta. FERMO prima della build
Branch claude/project-thread-72dtra (da xbjsuj 18ad9e24), checkout GASP_Giustino2_n216qs. Nessun asset di gioco toccato.

## Fatto
1. Diagnosi (docs/notes/reviews/dropkick-kick-k5/diagnosis.md, immagini accanto): il video "hit" di r4 non colpisce mai il manichino
   (dummy fermo, history vuota) e il dropkick "low" parte a 7,8 m (AIR_GIVE_UP); le braccia nel calcio NON sono conserte (a0c7bf9f:
   gomiti ai fianchi, pugni al mento = guardia); la review r4b guardava solo il rimbalzo. Il calcio: lean lento + gambe insieme, niente carica.
2. Carte variante: cards/overlord/dropkick/overlord_dropkick_{startup,loop}_k5.json da k5/make_k5_cards.py (originali intatti).
   Braccia fittate in k5/fit.py (polsi a -6/+6 cm, gomiti +15/-16, avambracci piatti sul petto); preview k5_startup_keys_stick.png.
   Sheet delle curve: overlord_dropkick_k5_curves.md. Durate invariate (16 + 12 f), part_delays 0.
3. Cattura: pie_capture_move.py PIE_CAPTURE_AIR_GIVE_UP + sequenze dropkick_sweet_hit/whiff (pressione a 380 cm in caduta).
4. Variante di gioco pronta da lanciare: Tools/GAS/setup_dropkick_kick_variant.py -> AS/AM ...K5, TL/GA_Overlord_DropkickTBK5,
   KIT_Test_PlaytestTBK5 (copia di TBR4, rimbalzo R4). Mai lanciato.
5. Timing csv copiati in Saved/AnimPipeline/refs/ufd (OverlordDropkick_timing.csv, _loop_timing.csv).

## Bloccato
Avvio dell'editor (Run-Timed + editor_session.py) negato dai permessi del portatile: serve l'approvazione macchina (regola 9).
Prossimi passi, nello stesso checkout:
1. Run-Timed.ps1 -Preset Capture -Script Tools\AnimPipeline\editor_session.py -MaxMinutes 60 -SilentMinutes 20
2. da Tools/AnimPipeline: python -m animpipe author <card>; python -m animpipe build <move> (startup_k5, loop_k5); check (clearance braccia)
3. setup_dropkick_kick_variant.py nell'editor; quit.txt
4. cattura: PIE_CAPTURE_KIT=KIT_Test_PlaytestTBK5 PIE_CAPTURE_SEQUENCES=dropkick_sweet_hit,dropkick_sweet_whiff PIE_CAPTURE_AIR_GIVE_UP=200
   (e base con KIT_Test_PlaytestTBR4); controllare in frames.jsonl che il dummy venga colpito tra f15 e f18, se no tarare SWEET_H.
5. griglie base vs K5 (v intorno a f11-f18 e caduta), review cloud: tratti PRIMA (gambe, braccia, compiaciuto in caduta), poi scatto.

## Per l'owner
Sweetspot vero (colpo forte all'inizio, debole dopo) = modifica di gameplay sulla timeline (finestre hitbox/danno/hitstop): decisione sua.
Il clip di atterraggio a59 (1 frame al touchdown) ha ancora le braccia vecchie: si vede 1 frame, non toccato.

Prove: frames.jsonl e capture.log di Saved/DropkickTB4/r4e (comandi in diagnosis.md); numeri delle braccia da k5/chest.py.
COSTO: ~7 $ (stima) | 0 min desktop | 0 token review
