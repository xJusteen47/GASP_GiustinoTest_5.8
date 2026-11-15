# Review: Overlord down-air v9, a vuoto / atterraggio / colpo — FAIL (gioco e dati montage) — 2026-11-15

Catture `base/` e `variant/` (integ-cp3 3d9567e5). Frame video: whiff n = v − 32 (base), v − 31 (variant);
land n = v − 77; hit n = v − 69. Attivo a15-17, hitstop 4 frame.

**Catena di cattura: NON valida come confronto base/variante.** La "variante" è la copia sporca di
`AM_OverlordDAirV9.uasset`, che ha gli stessi valori del file committato (`docs/notes/handovers/anim-finish-4.md`
riga 30: "values equal to the committed ones, binary differs"). Le pose sono identiche frame per frame a parità di
`a` (griglia lato del colpo, base sopra e variante sotto). Le sole differenze sono tempi di gioco che cambiano da
un'esecuzione all'altra. Le sei catture valgono come due esecuzioni della stessa v9, e così sono giudicate.
Altri limiti: nel whiff la variante parte un frame prima (v38 contro v39); nel colpo il dummy è 62 cm avanti e i
piedi a v91 sono a circa 30 cm dalla sua testa.

| domanda | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| impatto | sì | v90-v94 lato, dietro, fronte 3/4 | da palla raccolta a colonna verticale, piedi in basso che guidano; braccia incrociate (chiavi dell'owner) |
| tempo | sì | a12-a15 lato | distensione in 4 frame, gli ultimi due portano il 52% del percorso; fermo netto nei 4 frame di hitstop |
| primo_frame | sì | v38 / v39 | 6654-6823 px contro circa 1500 della caduta: ginocchia su, braccia che chiudono |
| peso | sì | a2-a13 | raccolta stretta, il busto si apre prima delle gambe |
| recupero | **no** | whiff v72-v88; land v102-v120 | gambe che salgono e poi cadono a fine mossa; coda aerea che continua a terra |
| pulizia | **no** | land base v86-v116 | frame doppi (`gioco`) |

**Confronto.** Classe aerea verso il basso (tempi presi da Ganondorf dair): la parte in aria regge. Non è alla pari
di un tripla A per l'uscita e per l'atterraggio durante la mossa.

## Scarti
1. **gioco** — uscita in aria a vuoto (`rv_dair_whiff_exit_base_vs_variant.jpg`, lato). Finito il montage (v71/v72) le
   gambe salgono in posa "seduta" con le ginocchia avanti, poi cadono di colpo nella posa di caduta: nella base a v88
   (4773 px), nella variante a v79 (4221 px). Stesso asset, frame diverso: il salto delle gambe dell'11-14 c'è ancora
   e non è deterministico. Da controllare: che posa ha l'Anim BP mentre la timeline gira senza montage (v72-v79) e
   perché la posa di caduta arriva a fine timeline in un'esecuzione e 9 frame dopo nell'altra.
2. **dati montage** (più una clip corta di atterraggio, `posa`) — atterraggio durante la mossa (`rv_dair_land.jpg`,
   tocco a v102 = a18.6). La coda aerea continua a terra per 15 frame: da dietro gambe arcuate e braccia incrociate
   v102-v112, poi il recupero aereo (ginocchia su) giocato sul pavimento v114-v120, poi idle a v122. Serve il taglio
   all'atterraggio: la traccia che il dropkick già usa (salto a una sezione "land" quando Mover tocca terra) più una
   sezione di atterraggio di 8-10 frame.
3. **gioco** — frame doppi: nella cattura land della base tutto il montage va a 30 Hz (16 su 32); nelle altre i
   primi 4-10 frame. Stessa causa dello scarto 2 del jab.

Note: dopo l'atterraggio nel colpo (v108-v112) l'attaccante è dentro il dummy (braccia sovrapposte); da dietro la
posa del colpo ha poca sagoma di braccia, ma sono chiavi dell'owner e non si toccano.

## Lato gioco, per il thread di codice
Scarti 1 e 3; lo scarto 2 per la parte traccia. Prima di rifare un confronto serve una variante vera.

## Decisioni dell'owner
Una, di design: l'atterraggio durante un'aerea. A) sezione di atterraggio per ogni aerea, come in Smash (consigliata);
B) taglio del montage al tocco e atterraggio standard dell'Anim BP. Finché non decide resta com'è oggi.

Numeri: px = `python cp5-data/tools/rv_diff.py cp5-dair-v9/base/base_dair_whiff.mp4 32 side` (variant: 31);
frame doppi = `cd cp5-data && python tools/count_repeats.py`; mappa = `python cp5-data/tools/rows_rle.py cp5-data/dair_base/dair_land.jsonl`.
