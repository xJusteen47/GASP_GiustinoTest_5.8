# Handover polish M2 r4: giro mirato su palline, anello e bump (2026-11-17, portatile + editor UE 5.9, Opus medium)
Branch `claude/project-thread-vcepyw` da `claude/project-thread-6web8v` 0888d5f2. Brief: sezione "Decisione owner" di `polish-m2-r3.md`.
Review Fable 5.2 high: `docs/notes/reviews/polish-m2/review-fable-r4.md`. **FAIL di misura** solo per il bump muro (non giudicabile); il resto PASS.
## Fatto
- `setup_presentation_vfx.py`: `impact()` palline su cerchio 40 -> 58 cm, 18-24 -> 14-18 cm, 300 -> 440 cm/s, drag 7 -> 8; `land_ring()` palline su cerchio 38 -> 58, 14-18 cm, 440 cm/s, anello 182 -> 95 cm.
- `setup_presentation_vfx_materials.py`: `GroundRing` ErodeStart 0,4 -> 0,3.
- `setup_presentation_table.py`: `Bump.Wall` 0,4 -> 0,65 (tentativo 2). `Bump.Ceiling` lasciato a 0,4 (il suo problema è di gioco, vedi coda).
- `pie_capture_vfx.py`: `wall_bump` 90 -> 200 frame; chiave nuova `side_az` (default 90), `wall_bump` a 135.
## Esito Fable
- PASS palline: knockdown (sul corpo v214-216), atterraggio, rimbalzo terra, soffitto (al limite). Note dati facoltative: sfasare gli angoli di nascita (coppie fuse nel rimbalzo terra, mezzo giro in atterraggio).
- PASS anello atterraggio: ~80 cm contro ~100 cm misurati dell'anello del colpo L, banda ~10-12 cm; durata al limite basso (15 frame atterraggio, 13 knockdown). Facoltativo: ErodeStart 0,35-0,4.
- Bump muro: tentativo 1 (0,4, camera 90) FAIL dati, non si legge (puntini da 7 cm dentro il busto). Tentativo 2 (0,65, camera 135) non giudicabile: la camera finisce dietro al muro e la vittima è coperta da v158 a v170. Tentativi finiti (regola 5).
## Da decidere (owner, consigliata A)
A) tenere 0,65 (sulla carta corretto: palline ~9-12 cm, nascita ~38 cm, fuori dal busto) e confermarlo nel playtest; B) un'altra ricattura con camera a 45° o posta dalla normale del muro (controllare un frame a v160 prima della serie), ~5 $ Opus + ~3 $ Fable.
## Coda codice (gioco), invariata più due note
- Rimbalzi muro doppi; bump e WallBounce nello stesso frame (anche in `events_r4c0`, sim 1981); soffitto come WallBounce.
- Nuovo (Fable): primo rimbalzo terra con effetto nato ~60 cm da terra, Bump.Ceiling al petto della vittima: posizione evento al centro del corpo, non sul punto di contatto.
- Le sessioni OFF e ON non partono dallo stesso spawn (pawn a posizioni diverse in `capture.log`): le griglie off/on hanno inquadrature diverse. Da guardare nella catena di cattura.
## Prove (regola 11)
- Setup: `Saved/Logs/ci/m2r4-setup.txt` (9 sistemi, 0 errori), tabella `Saved/Logs/ci/m2r4-table.txt` (13 righe). Esiti `Saved/Logs/ci/m2r4-*.log.esito`: tutti OK.
- Catture: `Saved/PolishM2/r4{a,b,c}/vfx{0,1}` (`capture.log`, `finish: ok`). Eventi host: `docs/notes/reviews/polish-m2/r4/events_r4*.txt` (`grep netmode=2` dai `.ue.log`); intensità bump 0,65 in `events_r4c1.txt`.
- Griglie: 9 in `docs/notes/reviews/polish-m2/r4/`.
Costo stimato thread: ~14 $ Opus + ~7 $ Fable.
