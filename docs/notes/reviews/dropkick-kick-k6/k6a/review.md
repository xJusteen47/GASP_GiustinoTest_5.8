# Review dropkick K6, pack k6a (cloud, 2026-11-21) - VERDETTO: FAIL posa (giro 1 di 3)

Build `claude/project-thread-jx1fv2` 5a821c53. Base = K5 r3. Guardate 8 immagini: tiles whiff az90, grid_high_settle,
5 fogli miei dai mp4 (384-480 px a frame: whiff az90 a0-a19, high az90 e az315 a12-a31, hit az90 a10-a29) piu' i due
allegati qui: `review_press_k5_over_k6_az315.jpg` (a2/a4/a6/a8/a11, K5 sopra K6) e `review_settle_zoom_az315.jpg`
(a15/16/17/18/20/22/24/27). ASSERT OK sulle tre run (assert_lines.txt).

## 1. Identita' (prima): PASS
- Gambe unite e dritte da a15 (az90 e az315): PASS.
- Braccia conserte: in piedi a4-a13, sdraiate da a14 con avambracci sul petto, testa visibile su az315: PASS.
- Smug in caduta: testa sollevata, mento su, sguardo lungo il corpo, tenuta per tutta la caduta alta: PASS.

## 2. Vita
- LINEA (richiesta 3, busto indietro e piegato, gambe avanti mai sotto il piano del corpo): PASS, ed e' il guadagno
  vero di K6. Su az90 a15-a18 il bacino e' il punto piu' basso, le gambe salgono verso i piedi, il busto e' curvo
  indietro: K5 e' una tavola che pende in avanti, K6 e' una V bassa che "siede" nel colpo. Tenuta in tutta la caduta.
- Snap a13 -> a15 in 2 frame: conservato, PASS.
- PRESS / RELAX delle braccia (richiesta 1): FAIL posa. Rispetto a K5 a4-a6 ha spalle un po' piu' chiuse, ma e' una
  differenza di POSA, non un movimento: dentro K6 tra a4, a6 e a8 le braccia non cambiano in modo leggibile su az315
  a 250 cm (foglio press). L'owner ha chiesto braccia che stringono e poi mollano: a velocita' reale non si vede.
- OVERSHOOT dopo il colpo (richiesta 2): FAIL posa per ampiezza. Il settle c'e' (cosce su fino a a17-a18, poi giu'
  a a22-a27; foglio settle) ma si legge solo in primo piano e a frame fermi: ~5 gradi. Movimento tra frame su az315
  a16-a18: K6 7,6 / 9,6 contro K5 6,0 / 6,1 (media differenza luma, comando sotto): un accento minimo.
  Testa e braccia nel settle: nessun flick o rilascio leggibile.
- Peso / secondario in generale: fuori dalla linea nuova, il video K6 a velocita' reale somiglia a K5.

## 3. Tecnico
- Clearance PASS (dal README, non ricontrollata). Nessuna compenetrazione visibile nei fogli.
- Hit: a15-a17 fermi (hitstop), poi il settle prosegue; niente da segnalare sulla posa.
- Coda FAF: non applicabile (attiva fino all'atterraggio, nessun FAF in aria).

## Cosa correggere (giro 2, etichetta: posa)
1. Press come MOVIMENTO, con contrasto dal prima e dal dopo (la clearance e' gia' a 0,07 cm: non stringere di piu'):
   a2-a3 gomiti aperti 8-10 cm piu' larghi di K5 (anticipo); a4-a5 chiusura con spalle su e dentro e petto che si
   comprime 6-8 gradi; a8 rilascio: gomiti giu' e fuori 5-6 cm, spalle giu'. Limite di silhouette: su az315 a 250 cm
   il gomito vicino deve spostarsi di almeno 8 cm tra a3 e a5 e tornare di almeno 4 cm entro a8.
2. Settle con ampiezza doppia: gambe/bacino 10-12 gradi oltre il lock con picco a17, rimbalzo 3-4 gradi SOTTO il lock
   a21-a23 ma mai sotto il piano del corpo; testa oltre lo smug di almeno 10 gradi, 2 frame dopo; avambracci che si
   staccano 4-5 cm dal petto e ricadono per ultimi. Incroci e gambe unite restano.
3. Non toccare: linea a15, snap a13-a15, smug, timeline, hitbox.

## Cattura (etichetta: cattura, non conta come giro)
- Press alta: az90 taglia testa e piedi, az315 taglia i piedi per tutto il settle: punte e testa non giudicabili.
  Camera a 350 cm o che segue il bacino.
- Regola owner 11-21 19:03: ogni cattura con vista da DIETRO e di LATO. Il pack ha lato e 3/4, manca dietro (az180).
- Hit: il manichino colpito e' quasi fuori quadro (stessa nota di k5r3).

## Prove
Movimento: `ffmpeg -i k6high_dropkick_sweet_whiff_az315_realspeed.mp4 -vf "select='between(n,34,48)',crop=480:300:0:0,
tblend=all_mode=difference,signalstats,metadata=print:key=lavfi.signalstats.YAVG:file=-" -f null -` (idem k5high).
