# dropkick-k5-review-1 (cloud, 2026-11-21, Fable high) - review regola 10 del calcio K5, pacchetto k5b: FAIL (posa), giro 1
Branch claude/project-thread-bo3bag (da 72dtra ac6dacee), solo documenti. Review: docs/notes/reviews/dropkick-kick-k5/k5b/review.md
(+ review_arms_zoom.jpg, review_snap_az90.jpg).

## Esito
- Gambe unite e dritte da f15: PASS.
- Braccia conserte a3-a19: **FAIL posa**. Gomiti ai fianchi, avambracci in avanti, polsi a X staccati dal petto; a15 di lato un
  avambraccio col pugno esce dal petto. Si legge ancora come guardia. Correzione: gomiti avanti e su, avambracci orizzontali
  sovrapposti, mani sul braccio opposto; la clearance si ottiene spostando i gomiti, non le mani.
- Compiaciuto in caduta: NON verificabile (il loop non e' nel pacchetto, serve cattura alta SWEET_H ~700).
- Vita: carica a7-a13 PASS, scatto a13->a15 PASS (la base distende da a10 ad a15, K5 tiene la palla e chiude in 2 frame).
  Minori (posa): nessun overshoot visibile a a15-a17; a3-a6 seduto 4 frame.
- Cattura: i soli mp4 sono in vista gioco, personaggio ~40 px: servono mp4 az90/az315 e tasselli >= 400 px.
- Gioco: manichino mai colpito, niente hitstop (noto); a a15 gli stinchi sono dentro il manichino: spostarlo ~40 cm (regola 13e).

## Decisioni applicate (regola 6)
Nessuna clip all'owner (solo PASS). Niente da confermare dall'owner in questo giro.

## Prossimo (portatile, thread nuovo)
Rifare il fit delle braccia su startup e loop K5, overshoot facoltativo a a15, ricattura sweet + alta, nuova review cloud.

Prove: etichette delle griglie (a0 = base v138, k5 v139), ffprobe sugli mp4 (70 frame, 480x480), review_pack.py (a0 = frame 20 dell'mp4).
COSTO: 3 $ stimati | 0 minuti desktop | ~80k token review
