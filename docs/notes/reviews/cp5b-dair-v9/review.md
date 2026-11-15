# Review: Overlord down-air v9 dopo il fix montage 30 Hz — FAIL (gioco) — 2026-11-15

Catture whiff / land / hit (integ-cp3 069287f7). Video indice 0: whiff v31, land v77, hit v69.
Mappa: whiff a0 = v38, montage finito v71, a terra v102; land a0 = v84, a terra v102, montage finito v117;
hit a0 = v76, hitstop a15 H1-H4 v91-v94, a terra v109. Nessuna base: unica cattura v9 dopo il fix (la variante cp5 era
lo stesso asset). Griglia `rv_dair_side.jpg` (w/l/h), vittima `rv_dair_hit_victim.jpg`.

| domanda | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| impatto | sì | v91-v96 lato, dietro | colonna verticale, piedi in basso; fermo netto di hitstop |
| tempo | sì | a12-a15 | invariato |
| primo_frame | sì | v38 whiff | 6741 px |
| peso | sì | a2-a13 | invariato |
| recupero | **no** (`gioco`) | whiff v66-v99; land v101-v116; hit v108-v118 | vedi scarti |
| pulizia | **no** (`gioco`) | hit v108-v118 lato | atterrato, l'attaccante compenetra la vittima (corpi sovrapposti) |

Vittima sul frame di colpo (v91): passa da in piedi alla posa di reazione in un frame, girata verso l'attaccante;
**nessun frame di posa sbagliata** (lo scatto di mezzo giro della review cp5 è sparito). Ferma v91-v96.
30 Hz: 0/32 frame ripetuti in tutte e tre, nessun frame a 0 px nello startup.

## Scarti
1. **gioco** — uscita a vuoto: finito il montage (v71) le gambe salgono in 10 frame nella posa di salto GASP (ginocchia
   avanti) e restano lì fino a terra (v99, 12787 px). Ora è una fusione morbida, non più lo scatto a v79/v88 della cp5, ma
   la mossa si "disfa" in aria.
2. **gioco / dati montage** — atterraggio durante la mossa: la coda aerea continua a terra v101-v116 (braccia incrociate in
   piedi). Decisione owner già proposta: sezione di atterraggio per aerea (rac.) o taglio al contatto.
3. **gioco** — compenetrazione con la vittima dopo l'atterraggio (v108-v118): manca la separazione tra capsule.
Numeri: `python cp5b-data/tools/rv_diff.py cp5b-dair-v9/dair9_dair_<cap>.mp4 <31|77|69> side`.
