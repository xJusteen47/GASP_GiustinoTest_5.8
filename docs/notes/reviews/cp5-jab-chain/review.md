# Review: Overlord jab 1 → jab 2 (v3 r2), catena con azione al FAF — FAIL — 2026-11-15

Cattura `jab_jab1_jab2_faf.mp4` (integ-cp3 3d9567e5), tap a v20, v32, v58. Frame video n = v − 14.
Mappa: jab 1 a0 = v21 (attivo a6-8 = v26-v29), jab 2 a0 = v33 (attivo a9-11 = v42-v44), tap a v58 = a25.3 del jab 2
(FAF 26), nuovo jab 1 a0 = v59, montage finito a v83, timeline finita a v87.

**Catena di cattura: valida.** Limiti: solo a vuoto (nessun dummy: contatto col bersaglio e frame di hitstop non
giudicati); nessuna base (su questo branch non esiste una variante del jab). L'azione al FAF c'è (v58 → v59).

## Jab 1
| domanda | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| impatto | sì | v26-v32 dietro, cam di gioco | braccio destro fuori dal profilo a destra, mano aperta; mano sinistra sul fianco |
| tempo | sì | v24 → v26 lato | caricamento a4, colpo a6: il passo più grande (9246 px) è sul primo frame attivo; poi posa tenuta |
| primo_frame | sì | v21 lato e dietro | 5027 px cambiati contro 0 dell'idle: bacino giù, busto che ruota |
| peso | sì | v21-v26 lato | parte il bacino, piede destro indietro, busto e spalle ruotano col colpo |
| recupero | **no** | v83-v126 lato | finito il montage il personaggio si accovaccia a fondo e risale in 43 frame (`gioco`, noto) |
| pulizia | **no** | v23, v25, v61-v69 | frame doppi: la posa avanza un frame sì e uno no (`gioco`) |

## Jab 2
| domanda | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| impatto | **no** | v42-v44 dietro contro v26-v32 | stessa sagoma del jab 1: braccio destro fuori a destra all'altezza della spalla, sinistra sul fianco (`posa`) |
| tempo | sì | v40 → v42 lato | mano sopra la testa a8, braccio disteso a10 in un passo; tenuta a10-a15 |
| primo_frame | sì | v33 | 4959 px: il braccio si alza subito (in r1 era identico alla tenuta del jab 1: corretto) |
| peso | sì | v36-v42 lato | busto indietro nel caricamento, passo avanti, peso sul piede avanti al colpo |
| recupero | sì | v52-v58 lato | fermo e vicino all'idle da a22; l'azione al FAF (v58 → v59) parte senza scatti |
| pulizia | **no** | v35, v37, v39, v41, v43 | frame doppi per tutto lo startup (`gioco`) |

**Confronto.** Classe jab: Mario jab 1 è 2 / 2-3 / 19 (rubrica). Qui lo startup è 6 e la tenuta 6 frame: è un jab
pesante per scelta di design, non fuori posto come ritmo. Da dietro non esiste riferimento Smash: giudicato per sagoma.
Alla pari di un tripla A oggi no, per i tre scarti sotto.

## Scarti (dal più grave)
1. **gioco** — accovacciata a fine jab, v83-v126 lato (`rv_jab_side_faf_end.jpg`). Voce nota (scelta Crouch_to_Stand di
   GASP): confermata ancora presente, parte sul primo frame senza montage (v83).
2. **gioco** — startup a 30 Hz. La posizione del montage va `0 2 2 4 4 6 6 …` (interi, a coppie) per i primi 6-10
   frame di ogni mossa, poi torna a 1.04 per frame. 12 frame doppi su 59. Il jab mostra 4 pose in 7 frame di startup.
   Al thread di codice: l'orologio di presentazione del montage aggancia la posizione a frame interi della simulazione
   che arrivano due per volta; deve avanzare ogni frame video dal primo.
3. **posa** — jab 2, a10-a15, braccio destro e busto: al contatto la mano va portata all'altezza del fianco (almeno
   30 cm sotto la tenuta del jab 1), sempre fuori dal profilo a destra vista da dietro, col busto inclinato sul piede
   avanti. Così da dietro il jab 1 è una diagonale in alto e il jab 2 una in basso. Una istruzione sola, livello
   `misurato`, non peggiorare: braccio visibile da dietro (era il FAIL di r1).

Nota, non scarto: la tenuta del jab 1 (v27-v32) è quasi immobile (48-319 px per frame).

## Lato gioco, per il thread di codice
Scarti 1 e 2. Lo scarto 2 vale per tutte le mosse (vedi dropkick e down-air).

## Decisioni dell'owner
Nessuna nuova. Restano aperte le sue domande sul jab 1 (29°, mano sul fianco, passo): da dietro il braccio a 29° si
legge; il contatto col dummy non è verificabile da questa cattura.

Numeri: px = `python cp5-data/tools/rv_diff.py cp5-jab-chain/jab_jab1_jab2_faf.mp4 14 side`; frame doppi =
`cd cp5-data && python tools/count_repeats.py`; mappa = `python cp5-data/tools/rows_rle.py cp5-data/jab/jab1_jab2_faf.jsonl`.
