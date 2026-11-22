# Review: Overlord dropkick dopo il fix montage 30 Hz — FAIL (solo gioco) — 2026-11-15

Cattura `dropkick_dropkick_whiff.mp4` (integ-cp3 069287f7). Video indice 0 = v36. Mappa: a0 = v43, estensione a15 = v58,
tenuta in aria (loop a16-a26) v68-v102, a terra v103 (salto ad a59), montage finito v128, timeline finita v133.
Base = cattura cp5, stessi frame (`rv_dk_side_base_vs_new.jpg`, base sopra, nuova sotto).

| domanda | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| impatto | sì | v58 lato | corpo orizzontale, piedi avanti (invariato) |
| tempo | sì | v43-v58 | ora una posa per frame: nella base v45-v47 andavano a coppie |
| primo_frame | sì | v43 | 6638 px |
| peso | sì | v43-v102 | tenuta in aria leggibile, nessuno scatto ai giri del loop |
| recupero | **no** (`gioco`) | v103-v133 lato | a terra disteso immobile v105-v128 (0-6 px/frame v119-v128), poi a fine montage salta in 4 frame da disteso ad accovacciato (v129-v132, 4929-7459 px/frame): nessun rialzo animato |
| pulizia | sì | v43-v102 | 0/49 frame ripetuti |

## Scarti
1. **gioco** — niente get-up: scatto disteso → accovacciato a v129-v132, poi risalita della crouch GASP v133-v163.
   Serve la clip di rialzo + `DROPKICK_FOLLOW_UP` (già nel brief dropkick-land-1).
2. **gioco** — caduta del bacino all'atterraggio (53 cm, decisione owner capsula aperta).
Parte in aria: PASS. Clip per l'owner: `q4`, `q4b`.
Numeri: `python cp5b-data/tools/rv_diff.py cp5b-dropkick/dropkick_dropkick_whiff.mp4 36 side`.
