# Review: colpo con cue CP4 (KIT_Example Attack) dopo il fix — FAIL (dati montage del kit di esempio) — 2026-11-15

Cattura `cue_cue_hit.mp4` (integ-cp3 069287f7). Video indice 0 = v14. Mappa: tap v20, a0 = v21, colpo e hitstop v26
(6.1 H1-H2). Griglia `rv_cue_victim.jpg` (righe: lato, fronte 3/4, az0, cam di gioco; v24-v33).

| voce | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| vittima primo frame | **sì** | v25 → v26 lato, fronte 3/4 | da idle alla posa di colpo in un frame, girata verso l'attaccante. **Il frame girato di 90° della cp5 non c'è più** (fix snap 180°) |
| vittima direzione | sì | v26-v29 | busto indietro, lontano dall'attaccante |
| fermo di hitstop | sì | v26-v29 | vittima ferma; attaccante fermo |
| attacco e colpo insieme | **no** (`dati montage`) | v21-v33 lato | l'attaccante è ancora in idle quando la vittima reagisce (v21-v25: 0-13 px); la spinta della clip arriva dopo |
| shake vittima | parziale | v26-v29 | az0: baricentro alterna 187.6 / 192.3 / 187.9 / 192.0 px (≈4.5 px su vista 360, si vede); cam di gioco: 167.1-167.2 (invisibile a quella distanza) |

## Scarti
1. **dati montage** — kit di esempio: colpo a f4-6, spinta della clip ~20 frame dopo. Decisione owner già aperta: campo
   timeline "frame di inizio montage" (~$8-12).
2. Shake: invisibile nella cam di gioco. Ampiezza o direzione da rivedere quando si tara il kit vero (non bloccante).
Numeri: centroidi = script ad hoc su frame 12-15 (pixel grigi, crop per vista), `rv_diff.py ... 14 side`.
