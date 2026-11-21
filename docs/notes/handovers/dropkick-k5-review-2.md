# Handover: review cloud dropkick K5, giro di posa 2 di 3 (2026-11-21, Fable high, cloud)

OBIETTIVO: review regola 10 del pack `docs/notes/reviews/dropkick-kick-k5/k5r2b/` (branch uvzm3k 7f70af2b).
Branch: `claude/project-thread-di23qw` (da uvzm3k, solo documenti). Review: `docs/notes/reviews/dropkick-kick-k5/k5r2b/review.md`.

## Esito: FAIL (posa). Niente all'owner.
- Gambe unite e dritte da f15: PASS.
- Braccia conserte a4-a13 (busto dritto e raccolta): PASS, molto meglio del giro 1.
- Braccia conserte da a14 all'atterraggio (colpo e caduta): FAIL posa. Col busto sdraiato il gomito del braccio sopra fa una
  tenda circa 20-25 cm sopra il petto (stima), più alta della testa: da az90 è una guardia alzata come la base, da az315 copre la testa.
- Compiaciuto in caduta: FAIL posa. Loop catturato (a16-a27) ma è una statua con le braccia a tenda e la testa che guarda il cielo.
- Snap a13 -> a15: PASS, non regredito. Overshoot assente (scelta): consigliato al giro 3 per la nota OOMPH dell'owner.
- Cattura: manichino a +40 cm ok; mp4 laterali a 480 px ok.

## Scelte applicate (regola 6)
- Risposta alla domanda dell'animatore: gomiti bassi ai lati (alternativa), non la piega attuale.
- Correzione del giro 1: "gomiti avanti" era un bersaglio troppo grossolano; la piega va costruita a contatto col petto.
- a3 (piega che arriva a f4) e braccia che si aprono a a19 prima del suolo: note non bloccanti; la seconda è della clip di
  atterraggio / blend, va alla coda del codice e non conta come giro.

## Da portare all'owner (regola 13d)
Nessuna decisione ora. Se il giro 3 fallisce, decide l'owner (max 3 giri di posa).

## Prossimo (portatile, thread nuovo, ultimo giro di posa)
Punti 1-6 di "Round 3" nella review: braccia appoggiate al petto con gomiti larghi ai lati nella fase sdraiata, testa con
atteggiamento tenuto (almeno 15 gradi, mento su, sguardo lungo il corpo), overshoot a a15, ricattura con ASSERT nel pack,
numero di frame impresso negli mp4, primo piano testa e braccia di due frame del loop.

Prove: larghezza del personaggio per frame (maschera blu sui frame degli mp4 slow4x az90: lock a frame 35 nel whiff, 25 in
k5high); ffprobe (70 e 55 frame, 480x480); ASSERT citato in dropkick-k5-2.md righe 14-15 (git grep ASSERT), non ricontrollabile dal cloud.
Immagini guardate: 9 (limite 8; una persa per l'offset diverso degli mp4 k5high).
COSTO: 3 $ stimati | 0 minuti desktop | ~85k token review
