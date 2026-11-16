# Review: Overlord jab 1 → jab 2 (v3 r2) dopo il fix montage 30 Hz — FAIL (posa jab 2 + gioco) — 2026-11-15

Cattura `jab_jab1_jab2_faf.mp4` (integ-cp3 069287f7). Video indice 0 = v14 (etichetta in alto nel video = v vero).
Mappa (`rows_rle.py`): jab 1 a0 = v21, a6 = v27; jab 2 a0 = v33, a9 = v42; tap FAF v58 → nuovo jab 1 v59; montage finito v82.
Base = cattura cp5 (3d9567e5), stessi frame. Solo a vuoto.

| domanda | jab 1 | jab 2 | frame e vista | cosa si vede |
|---|---|---|---|---|
| impatto | sì | **no** (`posa`) | v27-v32 / v42-v44 dietro | jab 2 da dietro = stessa sagoma della tenuta del jab 1 (invariato, nessuna posa cambiata) |
| tempo | sì | sì | v26 → v27, v41 → v42 lato | il passo grande cade sul primo frame attivo in entrambi |
| primo_frame | sì | sì | v21, v33 | 4967 / 5040 px contro 0 dell'idle |
| peso | sì | sì | v21-v27, v35-v42 lato | invariato dalla r2 |
| recupero | **no** (`gioco`) | sì | v83-v130 lato | finito il montage, accovacciata profonda e risalita (`rv_jab_end_squat_side.jpg`) |
| pulizia | **sì** | **sì** | v21-v58 | **fix 30 Hz confermato**: 1 posa nuova per frame, 0 frame a 0 px nello startup (prima v23, v25 = 0 px) |

**Mano del jab 2 (tremolio).** Lato, zoom 2x v41-v55 (`rv_jab2_hand_side.jpg`): nessun tremolio visibile; la tenuta
v43-v46 scende piano (313 → 729 px/frame) e poi il braccio rientra. Se l'owner lo vede in gioco, è sotto la soglia di
questa cattura: clip `q2` per lui.

## Scarti
1. **gioco** — accovacciata a fine jab, v83-v130. Invariato, voce nota (ABP Crouch_to_Stand).
2. **posa** — jab 2 a10-a15: mano all'altezza del fianco, ≥30 cm sotto la tenuta del jab 1, visibile a destra da dietro
   (istruzione di r2 non ancora eseguita; resta giro 2 di 3).

Lo scarto 30 Hz della review cp5 è chiuso. Numeri: `python cp5b-data/tools/rv_diff.py cp5b-jab-chain/jab_jab1_jab2_faf.mp4 14 side|behind`;
`cd cp5b-data && python tools/count_repeats.py` (0/59).
